/*
  Module Hardware
  part of Arduino Mega Server project
*/

#define MAX_MEMORY 8192
#define PERC_MEM (MAX_MEMORY / 100)
#define MAX_PROGRAM 253952
#define SKETCH_SIZE 68338

#define CONTROLLER "Arduino Mega"

#define NO_ETHERNET_SHIELD 0
#define ETHERNET_SHIELD    1
#define ETHERNET_SHIELD2   2
byte ethernetShield = NO_ETHERNET_SHIELD;


void hardwareInit() {
  initStart("Hardware");
  Serial.print(F(" Controller:      ")); Serial.println(CONTROLLER);
  Serial.print(F(" Total memory:    ")); Serial.println(MAX_MEMORY);
  Serial.print(F(" Free memory:     ")); Serial.print(freeMem()); Serial.print(F(" (")); Serial.print(freeMemP()); Serial.print(F("%)\n"));
  setNetworkAdapter();
  printNetworkAdapter();
  initDone();
}

void setNetworkAdapter() {
  #ifdef AMS_ETHERNET
    ethernetShield = ETHERNET_SHIELD;
  #endif
  #ifdef AMS_ETHERNET2
    ethernetShield = ETHERNET_SHIELD2;
  #endif
}

void printNetworkAdapter() {
  Serial.print(F(" Network adapter: "));
  switch (ethernetShield) {
    case ETHERNET_SHIELD:  Serial.println(F("Ethernet Shield"));   break;
    case ETHERNET_SHIELD2: Serial.println(F("Ethernet Shield 2")); break;
                  default: Serial.println(F("none"));
  }
}

int freeMem() {
  extern int __heap_start, *__brkval;
  int v;
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}

int freeMemP() {
  return freeMem() / PERC_MEM;
}

void printFreeMem() {
  Serial.print(freeMem());
  Serial.print(F(" (")); Serial.print(freeMemP()); Serial.print(F("%)"));
}

// CPU load / Cyclos in second

int summCyclosInSec = 0;
int cyclosInSec = 0;

void cyclosInSecCalc() {
  cyclosInSec = (100 - summCyclosInSec - 5) * 1.1;
  if (cyclosInSec < 0) {cyclosInSec = 0;}
  if (cyclosInSec > 100) {cyclosInSec = 100;}
  summCyclosInSec = 0;
}

void cyclosInSecWork() {
  summCyclosInSec++;
  if (cycle1s) {cyclosInSecCalc();}
}

// CPU load / Cyclos delay

unsigned long pointCpu    = 0;
unsigned long oldPointCpu = 0;
unsigned long deltaCpu    = 0;
#define MAX_CPU_BUFFER 10
unsigned long bufferCpuLoad[MAX_CPU_BUFFER];
int cyclosDelay = 0;

void shiftCpuLoad(unsigned long tick) {
  for (byte i = MAX_CPU_BUFFER - 1; i > 0; i--) {
    bufferCpuLoad[i] = bufferCpuLoad[i - 1];
  }
  bufferCpuLoad[0] = tick;
}

void averageCpuLoad() {
  unsigned long summ = 0;
  
  for (byte i = 0; i < MAX_CPU_BUFFER; i++) {
    summ += bufferCpuLoad[i];
  }
  cyclosDelay = summ / MAX_CPU_BUFFER;
}

void cyclosDelayWork() {
  pointCpu = millis();
  deltaCpu = pointCpu - oldPointCpu;
  oldPointCpu = pointCpu;
  shiftCpuLoad(deltaCpu);
  averageCpuLoad();
  cyclosDelay -= 8;
  if (cyclosDelay < 0) {cyclosDelay = 0;}
  if (cyclosDelay > 100) {cyclosDelay = 100;}
}

