/*
  Module Hardware
  part of Arduino Mega Server project
*/

#define MAX_MEMORY 294912
#define MAX_FREE_MEM 285736
#define PERC_FREE_MEM (MAX_FREE_MEM / 100)
#define MAX_PROGRAM 1044464
#define MIN_CKETCH 474555

extern "C" {      
  uint8_t temprature_sens_read();
  uint32_t hall_sens_read();
}

#define BASE_HALL -21
#define MAX_HBUF 6
int hbuf[MAX_HBUF];

void initHardware() {
  initStart(F("Hardware"), false);
  printArduinoIde();
  Serial.print(F(" SDK version:   ")); Serial.println(ESP.getSdkVersion());
  Serial.print(F(" Chip revision: ")); Serial.println(ESP.getChipRevision());
  
  uint64_t chipid = ESP.getEfuseMac();                          // chip ID is essentially its MAC address (length 6 bytes)
  Serial.printf(" Chip ID:       %04X",(uint16_t)(chipid>>32)); // high 2 bytes
  Serial.printf("%08X\n",(uint32_t)chipid);                     // low 4 bytes
  
  Serial.print(F(" CPU freq:      ")); Serial.print  (ESP.getCpuFreqMHz()); Serial.println(" MHz");
  Serial.print(F(" Free memory:   ")); Serial.print  (ESP.getFreeHeap()); Serial.print(F(" (")); printFreeMemP(); Serial.println(F("%)"));
  Serial.print(F(" Flash mode:    ")); Serial.println(ESP.getFlashChipMode());
  Serial.print(F(" Flash speed:   ")); Serial.print  (ESP.getFlashChipSpeed() / 1000000); Serial.println(" MHz");
  Serial.print(F(" Flash size:    ")); Serial.println(ESP.getFlashChipSize());
  //Serial.print(F(" Cycle count:   ")); Serial.println(ESP.getCycleCount());

  Serial.print(F(" Self temp:     ")); Serial.print(selfTemp(), 1);  Serial.println(" C");
  
  setBaseHall(0);
  initDone(false);
} // hardwareInit()

// Arduino IDE

void printArduinoIde() {
  Serial.print(F(" Arduino IDE:   "));
  Serial.print(ARDUINO / 10000);
  Serial.write('.');
  Serial.print((ARDUINO % 10000) / 100);
  Serial.write('.');
  Serial.println(ARDUINO % 100);
}

// Self temp sensor

float selfTemp() {
  uint8_t tf = temprature_sens_read();
  float   tc = (tf - 32) / 1.8;
  return tc;
}

// Self hall sensor

void setBaseHall(int h) {
  for (byte i = 0; i < sizeof(hbuf) / 2; i++) {
    hbuf[i] = h;
  }
}

void addHall(int h) {
  for (byte i = sizeof(hbuf) / 2 - 1; i > 0; i--) {
    hbuf[i] = hbuf[i - 1];
  }
  hbuf[0] = h;
}

int getAverageHall() {
  int sum = 0.0;
  for (byte i = 0; i < sizeof(hbuf) / 2; i++) {
    sum += hbuf[i];
  }
  return sum / ((int)sizeof(hbuf) / 2);
}

int selfHall() {
  int h = 0;
  uint32_t r = hall_sens_read();
  if (r < 32768) {
    h = r + BASE_HALL;
  } else {
      h = (r + BASE_HALL) - 65536;
    }
  return (int)h;
}

void hallSensorWorks() {
  addHall(selfHall());
  Serial.println(getAverageHall());

  selfHall() >  10 ? wellowOn() : wellowOff();
  selfHall() < -10 ? redOn()    : redOff();
}

// Free mem

void printFreeMemP() {
  Serial.print(ESP.getFreeHeap() / PERC_FREE_MEM);
}

int freeMem() {
  return ESP.getFreeHeap();
}

// CPU load
// ESP-12F, 80 MHz, QIO 40 MHz

// cyclos in sec
long cyclosInSec  = 0;
long cyclosInSecP = 0;

void calcCyclosP() {
  cyclosInSecP = 100 - (cyclosInSec / 10);
  if (cyclosInSecP <   0) {cyclosInSecP =   0;}
  if (cyclosInSecP > 100) {cyclosInSecP = 100;}
  cyclosInSec = 0;
}

void cyclosInSecWork() {
  cyclosInSec++;
  if (cycle1s) {calcCyclosP();}
}

// cyclos delay
#define MAX_CPU_BUFFER 10
unsigned long bufferCpuLoad[MAX_CPU_BUFFER];
unsigned long oldCycle = 0;

byte cyclosDelayP() {
  unsigned long summ = 0;
  for (byte i = 0; i < MAX_CPU_BUFFER; i++) {summ += bufferCpuLoad[i];}
  int cyclosDelay = summ / MAX_CPU_BUFFER; // delay >= 100 ms (100% load)
  cyclosDelay /= 2; //  k=10 => delay >= 1 s (100% load)
  if (cyclosDelay <   0) {cyclosDelay =   0;}
  if (cyclosDelay > 100) {cyclosDelay = 100;}
  return cyclosDelay;
}

void cyclosDelayWork() {
  unsigned long now2 = millis();
  for (byte i = MAX_CPU_BUFFER - 1; i > 0; i--) {
    bufferCpuLoad[i] = bufferCpuLoad[i - 1];
  }
  bufferCpuLoad[0] = now2 - oldCycle;
  oldCycle = now2;
}

