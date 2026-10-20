/*
 * Clock generator for a custom board, ATmega328P, TQFP-32.
 * External crystal: 16.384 MHz (16384 kHz), CPU prescaler: 1.
 * PD2 / Arduino D2 / package pin 32: 32768 Hz, 50% duty.
 * PD4 / Arduino D4 / package pin  2: positive 100 ms pulse every 1 s.
 * PB0 / Arduino D8 / package pin 12: positive GPS PPS input (ICP1).
 *
 * Timer1 interrupts at 65536 Hz. Both outputs start LOW.
 * GPS is qualified for three edges; phase synchronization is at most once/minute.
 * Only Timer1 compare interrupts are enabled; GPS capture is polled.
 * See README_RU.md for Arduino IDE and programmer settings.
 */

#include <Arduino.h>
#include <avr/interrupt.h>
#include <avr/power.h>
#include <avr/sleep.h>
#include <avr/wdt.h>

#if !defined(__AVR_ATmega328P__)
#error "Select ATmega328P (Arduino AVR Boards / Arduino Uno)."
#endif

// Use the actual crystal frequency, independent of the IDE's F_CPU default.
constexpr uint32_t CRYSTAL_HZ = 16384000UL;
constexpr uint32_t OUTPUT_HZ = 32768UL;
constexpr uint32_t TIMER_EVENTS_HZ = OUTPUT_HZ * 2UL;
constexpr uint32_t TIMER_COUNTS = CRYSTAL_HZ / TIMER_EVENTS_HZ;
constexpr uint32_t PD4_PULSE_WIDTH_MS = 100UL;
// Rounded to the nearest 15.258789 us timer event: 6554 ticks = 100.006104 ms.
constexpr uint16_t PD4_HIGH_TICKS = static_cast<uint16_t>(
    (TIMER_EVENTS_HZ * PD4_PULSE_WIDTH_MS + 500UL) / 1000UL);
constexpr uint32_t GPS_SYNC_INTERVAL_SECONDS = 60UL;
constexpr uint32_t GPS_SYNC_INTERVAL_TICKS =
    GPS_SYNC_INTERVAL_SECONDS * TIMER_EVENTS_HZ;
constexpr uint32_t GPS_MIN_PERIOD_TICKS = TIMER_EVENTS_HZ * 95UL / 100UL;
constexpr uint32_t GPS_MAX_PERIOD_TICKS = TIMER_EVENTS_HZ * 105UL / 100UL;
constexpr uint8_t GPS_REQUIRED_EDGES = 3;

static_assert(CRYSTAL_HZ % TIMER_EVENTS_HZ == 0,
              "The requested frequency must divide the crystal exactly.");
static_assert(TIMER_COUNTS >= 1 && TIMER_COUNTS <= 65536UL,
              "Timer1 compare value does not fit.");
static_assert(TIMER_EVENTS_HZ == 65536UL,
              "The 16-bit second divider requires 65536 events per second.");
static_assert(PD4_PULSE_WIDTH_MS > 0 && PD4_PULSE_WIDTH_MS < 1000UL,
              "PD4 pulse width must be between 1 and 999 ms.");
static_assert(GPS_SYNC_INTERVAL_SECONDS >= 1UL &&
              GPS_SYNC_INTERVAL_SECONDS <= 32767UL,
              "GPS synchronization interval must be 1..32767 seconds.");

static volatile uint16_t secondDivider = 0;
static volatile uint32_t generatorTicks = 0;

// Single-producer/single-consumer mailboxes: publish the byte flag LAST.
// The producer never overwrites a full mailbox, so no cli() is needed in loop().
static volatile uint32_t gpsEdgeTicks = 0;
static volatile uint8_t gpsEdgeReady = 0;
static volatile uint16_t syncEdgeTick = 0;
static volatile uint8_t syncPending = 0;

// Process PPS qualification outside the fast interrupt. Unsigned subtraction
// also handles generatorTicks wrapping after about 18.2 hours.
void synchronizeFromGps()
{
  static uint32_t lastEdge = 0;
  static uint32_t lastSync = 0;
  static uint8_t goodEdges = 0;
  static bool synchronized = false;

  if (!gpsEdgeReady) {
    return;
  }
  const uint32_t edge = gpsEdgeTicks;
  gpsEdgeReady = 0;

  if (goodEdges == 0) {
    lastEdge = edge;
    goodEdges = 1;
    return;
  }

  const uint32_t elapsed = edge - lastEdge;
  if (elapsed < GPS_MIN_PERIOD_TICKS) {
    goodEdges = 1;
    return; // Ignore early glitches without moving the last valid timestamp.
  }
  lastEdge = edge;
  if (elapsed > GPS_MAX_PERIOD_TICKS) {
    goodEdges = 1; // Requalify after a missing/late PPS.
    return;
  }
  if (goodEdges < GPS_REQUIRED_EDGES) {
    ++goodEdges;
  }
  if (goodEdges < GPS_REQUIRED_EDGES || syncPending) {
    return;
  }
  if (synchronized && edge - lastSync < GPS_SYNC_INTERVAL_TICKS) {
    return;
  }

  syncEdgeTick = static_cast<uint16_t>(edge);
  syncPending = 1;
  lastSync = edge;
  synchronized = true;
}

ISR(TIMER1_COMPA_vect)
{
  // Writing ONE to a PIN register toggles only that output bit.
  // Keep the fast edge before the divider's variable-length branch.
  PIND = _BV(PD2);

  const uint32_t now = ++generatorTicks;

  // One positive pulse per 65536 events = one second.
  const uint16_t phase = ++secondDivider;
  if (phase == 0) {
    PORTD |= _BV(PD4);
  } else if (phase == PD4_HIGH_TICKS) {
    PORTD &= ~_BV(PD4);
  }

  // ICP1 hardware latches rising edges without a second interrupt handler.
  // Do not reset TCNT1: the 32768 Hz timebase must remain continuous.
  if (TIFR1 & _BV(ICF1)) {
    TIFR1 = _BV(ICF1);
    if (!gpsEdgeReady) {
      gpsEdgeTicks = now;
      gpsEdgeReady = 1;
    }
  }

  // Rebase only during LOW; a pulse already HIGH always finishes normally.
  // The next rising edge is one second after the accepted GPS edge. A phase
  // correction can lengthen/shorten one LOW interval, never the 100 ms HIGH.
  if (syncPending && !(PORTD & _BV(PD4))) {
    secondDivider = static_cast<uint16_t>(now) - syncEdgeTick;
    syncPending = 0;
  }
}

void setup()
{
  cli();

  MCUSR &= ~_BV(WDRF);
  wdt_disable();
  clock_prescale_set(clock_div_1);

  // Dedicated generator: avoid jitter caused by other interrupt handlers.
  TIMSK0 = 0;
  TIMSK1 = 0;
  TIMSK2 = 0;
  EIMSK = 0;
  PCICR = 0;
  UCSR0B = 0;
  SPCR &= ~_BV(SPIE);
  TWCR = 0;
  ADCSRA &= ~_BV(ADIE);
  ACSR &= ~(_BV(ACIE) | _BV(ACIC));   // Capture source is ICP1, not comparator.
  EECR &= ~_BV(EERIE);

  // Set the output latches LOW before enabling the drivers.
  PORTD &= ~(_BV(PD2) | _BV(PD4));
  DDRD |= _BV(DDD2) | _BV(DDD4);
  secondDivider = 0;
  generatorTicks = 0;
  gpsEdgeReady = 0;
  syncPending = 0;

  DDRB &= ~_BV(DDB0);                 // GPS PPS: PB0 / ICP1 / TQFP pin 12.
  PORTB &= ~_BV(PB0);                 // No internal pull-up. Fit 10k to GND.

  TCCR1A = 0;                         // Normal GPIO, no OC1A/OC1B output.
  TCCR1B = 0;                         // Stop Timer1 while configuring.
  TCCR1C = 0;
  TCNT1 = 0;
  OCR1A = static_cast<uint16_t>(TIMER_COUNTS - 1UL); // 249.
  TIFR1 = _BV(ICF1) | _BV(OCF1B) | _BV(OCF1A) | _BV(TOV1);
  TIMSK1 = _BV(OCIE1A);

  // Idle sleep keeps Timer1 running and gives repeatable interrupt latency.
  set_sleep_mode(SLEEP_MODE_IDLE);
  sleep_enable();
  TCCR1B = _BV(ICNC1) | _BV(ICES1) | _BV(WGM12) | _BV(CS10);
  // CTC, CPU clock / 1; capture rising edges with hardware noise cancellation.
  sei();
}

void loop()
{
  synchronizeFromGps();
  sleep_cpu();
}
