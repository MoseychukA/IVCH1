// Runs the compiled AVR instructions and GPIO writes, not a port of the algorithm.
// Input Capture is not implemented by avr8js: the fixture injects the hardware
// ICF1 flag on PPS rising edges. Analog input levels/noise filtering are untested.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const { execFileSync } = require('node:child_process');
const { CPU, avrInstruction, AVRIOPort, portBConfig, portDConfig } = require('avr8js');

const hexPath = process.argv[2] || path.join(__dirname, '../firmware/ClockGenerator_ATmega328P.ino.hex');
const nm = process.argv[3] || 'avr-nm';
const symbolText = execFileSync(nm, ['-n', hexPath.replace(/\.hex$/, '.elf')], { encoding: 'utf8' });
function symbol(name) {
  const line = symbolText.split(/\r?\n/).find(line => line.trim().endsWith(name));
  assert(line, `Missing ELF symbol: ${name}`);
  return parseInt(line.split(/\s+/)[0], 16) & 0xffff;
}
const pendingAddr = symbol('_ZL11syncPending');
const ticksAddr = symbol('_ZL14generatorTicks');
const clock = 16384000;
const compareIRQ = { address: 0x16, enableRegister: 0x6f, enableMask: 2,
  flagRegister: 0x36, flagMask: 2 };

function program() {
  const bytes = new Uint8Array(32768);
  let upper = 0;
  for (const line of fs.readFileSync(hexPath, 'utf8').trim().split(/\r?\n/)) {
    const record = Buffer.from(line.slice(1), 'hex');
    assert.equal(record.reduce((sum, byte) => (sum + byte) & 255, 0), 0, 'HEX checksum');
    const size = record[0], address = record.readUInt16BE(1), type = record[3];
    if (type === 4) upper = record.readUInt16BE(4) * 65536;
    if (type === 0) bytes.set(record.subarray(4, 4 + size), upper + address);
  }
  return new Uint16Array(bytes.buffer);
}

// Enumerate interrupt branches using the real compiled instructions. This
// includes the longest combination: pulse end + capture + phase correction.
let maxIsrCycles = 0;
for (const phase of [0, 6553, 65535]) {
  for (let flags = 0; flags < 16; flags++) {
    const cpu = new CPU(program());
    cpu.SP = 0x8ff;
    cpu.pc = symbol('__vector_11') / 2;
    cpu.dataView.setUint16(symbol('_ZL13secondDivider'), phase, true);
    cpu.data[0x36] = flags & 1 ? 0x20 : 0;
    cpu.data[symbol('_ZL12gpsEdgeReady')] = flags & 2 ? 1 : 0;
    cpu.data[pendingAddr] = flags & 4 ? 1 : 0;
    cpu.data[0x2b] = flags & 8 ? 16 : 0; // PORTD in data-space.
    let instructions = 0;
    for (;;) {
      const reti = cpu.progMem[cpu.pc] === 0x9518;
      avrInstruction(cpu);
      assert(++instructions < 200, 'ISR did not finish');
      if (reti) break;
    }
    maxIsrCycles = Math.max(maxIsrCycles, cpu.cycles);
  }
}
assert(maxIsrCycles < 239, `ISR exceeds 250-cycle budget: ${maxIsrCycles}`);
console.log(JSON.stringify({ name: 'ISR worst branch budget', maxIsrCycles, availableCycles: 250 }));
if (process.argv.includes('--budget-only')) process.exit(0);

function run(name, seconds, pps, expectedRequests, { seedWrap = false } = {}) {
  const cpu = new CPU(program());
  const pb = new AVRIOPort(cpu, portBConfig);
  const pd = new AVRIOPort(cpu, portDConfig);
  const rises = [], widths = [], requests = [];
  let previousD2, highStart, d2Count = 0, minD2 = Infinity, maxD2 = 0;
  pd.addListener((value, old) => {
    if ((value ^ old) & 4) {
      if (previousD2 !== undefined) {
        const delta = cpu.cycles - previousD2;
        minD2 = Math.min(minD2, delta);
        maxD2 = Math.max(maxD2, delta);
      }
      previousD2 = cpu.cycles;
      d2Count++;
    }
    if ((value ^ old) & 16) {
      if (value & 16) { highStart = cpu.cycles; rises.push(cpu.cycles); }
      else { widths.push(cpu.cycles - highStart); highStart = undefined; }
    }
  });

  let nextCompare = Infinity, epoch, sleeping = false, eventIndex = 0;
  let lastPending = 0, seeded = false, timerWritesAfterStart = 0;
  // Emulate the Timer1 CTC event schedule. The firmware never reads TCNT1.
  cpu.writeHooks[0x81] = value => {
    if ((value & 7) === 1) {
      epoch = cpu.cycles;
      nextCompare = epoch + 250;
      timerWritesAfterStart++;
    }
  };
  cpu.writeHooks[0x36] = value => {
    cpu.clearInterruptByFlag(compareIRQ, value);
    cpu.data[0x36] &= ~value;
    return true;
  };
  const events = pps.flatMap(time => [
    { cycle: Math.round(time * clock), high: true },
    { cycle: Math.round((time + 0.1) * clock), high: false }
  ]).sort((a, b) => a.cycle - b.cycle);
  const end = Math.round(seconds * clock);
  while (cpu.cycles < end) {
    if (sleeping) {
      cpu.cycles = Math.min(nextCompare, events[eventIndex]?.cycle ?? Infinity, end);
    }
    while (eventIndex < events.length && events[eventIndex].cycle <= cpu.cycles) {
      const event = events[eventIndex++];
      pb.setPin(0, event.high);
      if (event.high && (cpu.data[0x81] & 0x40)) cpu.data[0x36] |= 0x20;
    }
    if (cpu.cycles >= nextCompare) {
      nextCompare += 250;
      cpu.setInterruptFlag(compareIRQ);
      if (sleeping) { cpu.cycles += 4; sleeping = false; }
    }
    cpu.tick();
    if (sleeping || cpu.cycles >= end) continue;
    const isSleep = cpu.progMem[cpu.pc] === 0x9588 && (cpu.data[0x53] & 1);
    avrInstruction(cpu);
    if (isSleep) {
      sleeping = true;
      if (seedWrap && !seeded) {
        cpu.dataView.setUint32(ticksAddr, 0xfffff000, true);
        seeded = true;
      }
    }
    const pending = cpu.data[pendingAddr];
    if (pending && !lastPending) requests.push(cpu.cycles / clock);
    lastPending = pending;
  }

  assert.equal(timerWritesAfterStart, 1, `${name}: timer was restarted`);
  assert.equal(requests.length, expectedRequests, `${name}: synchronization requests ${requests}`);
  assert(rises.length >= Math.floor(seconds) - 1 - expectedRequests,
    `${name}: missing second-output pulses`);
  assert(d2Count > seconds * 65536 - 12, `${name}: lost fast output edges`);
  assert(minD2 >= 240 && maxD2 <= 260, `${name}: fast edge spacing ${minD2}..${maxD2}`);
  for (const width of widths) {
    assert(Math.abs(width - 6554 * 250) <= 12, `${name}: shortened/extended HIGH: ${width}`);
  }
  for (let i = 1; i < rises.length; i++) {
    const previous = rises[i - 1] / clock, current = rises[i] / clock;
    const transition = requests.some(time => time >= previous - 0.15 && time <= current + 0.15);
    if (!transition) {
      assert(Math.abs(rises[i] - rises[i - 1] - clock) <= 12,
        `${name}: non-periodic PD4 outside a phase correction`);
    }
  }
  for (let i = 1; i < requests.length; i++) {
    assert(requests[i] - requests[i - 1] >= 59.999, `${name}: correction more often than 60 s`);
  }
  if (expectedRequests) {
    // Every synchronization must produce an aligned rising edge one second later.
    for (const request of requests) {
      const acceptedPps = pps.filter(time => time <= request).at(-1);
      const nearest = Math.min(...rises.map(cycle => Math.abs(cycle / clock - acceptedPps - 1)));
      assert(nearest < 0.00004, `${name}: phase error ${nearest} s`);
    }
  }
  console.log(JSON.stringify({ name, seconds, requests, pd4Pulses: rises.length,
    d2SpacingCycles: [minD2, maxD2], maxHighErrorUs:
      Math.max(...widths.map(width => Math.abs(width - 6554 * 250))) * 1e6 / clock }));
}

run('autonomous', 4, [], 0);
run('GPS arrives during HIGH', 7, Array.from({ length: 6 }, (_, n) => 0.04 + n), 1);
run('GPS near timer rollover', 7, Array.from({ length: 6 }, (_, n) => 0.99999 + n), 1);
run('timestamp wrap', 7, Array.from({ length: 6 }, (_, n) => 0.35 + n), 1, { seedWrap: true });
run('reject 5 Hz input', 4, Array.from({ length: 19 }, (_, n) => 0.15 + n * 0.2), 0);
const longPps = Array.from({ length: 66 }, (_, n) => n + (n < 25 ? 0.35 : 0.37))
  .filter(time => time < 20 || time > 25).concat([0.75, 5.75]).sort((a, b) => a - b);
run('60 s interval, glitches, GPS loss and return, holdover', 70, longPps, 2);
