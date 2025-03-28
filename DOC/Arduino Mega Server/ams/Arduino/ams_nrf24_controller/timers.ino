/*
  Modul Timers
  AMS nRF24 controller
  part of Arduino Mega Server project
*/

// timers
unsigned long uptime;
unsigned long fix1s;
unsigned long fix5s;
unsigned long fix20s;
unsigned long fix30s;
unsigned long fix1m;
unsigned long fix3m;
unsigned long fix5m;

// fuzzy timers
unsigned long fuz10s;
unsigned long fuz20s;
unsigned long fuz30s;
unsigned long fuz40s;
byte delta10s;
byte delta20s;
byte delta30s;
byte delta40s;

// data timers
unsigned long tmp1;

void timersInit() {
  uptime = millis() / 1000;
  fix1s =  uptime;  
  fix5s =  uptime; 
  fix20s = uptime;
  fix30s = uptime;
  fix1m =  uptime;
  fix3m =  uptime;
  fix5m =  uptime;

  fuz10s = uptime;
  fuz20s = uptime;
  fuz30s = uptime;
  fuz40s = uptime;
  delta10s = random(0, 6);
  delta20s = random(0, 10);
  delta30s = random(0, 15);
  delta40s = random(0, 20);

  tmp1 =  uptime;
}

void timersWorks() {
  uptime = millis() / 1000;
  if (uptime - fix1s  >=   1) {fix1s =  uptime; timer_1s =  true;}
  if (uptime - fix5s  >=   5) {fix5s =  uptime; timer_5s =  true;}
  if (uptime - fix20s >=  20) {fix20s = uptime; timer_20s = true;}
  if (uptime - fix30s >=  30) {fix30s = uptime; timer_30s = true;}
  if (uptime - fix1m  >=  60) {fix1m =  uptime; timer_1m =  true;}
  if (uptime - fix3m  >= 180) {fix3m =  uptime; timer_3m =  true;}
  if (uptime - fix5m  >= 300) {fix5m =  uptime; timer_5m =  true;}

  if (uptime - fuz10s >= 10 + delta10s) {fuz10s = uptime; fuzzy_10s = true; delta10s = random(0, 6);}
  if (uptime - fuz20s >= 20 + delta20s) {fuz20s = uptime; fuzzy_20s = true; delta20s = random(0, 10);}
  if (uptime - fuz30s >= 30 + delta30s) {fuz30s = uptime; fuzzy_30s = true; delta30s = random(0, 15);}
  if (uptime - fuz40s >= 40 + delta40s) {fuz40s = uptime; fuzzy_40s = true; delta40s = random(0, 20);}
  
  if (uptime - tmp1 >= temp1_period) {tmp1 = uptime; timer_temp1 = true;}  
}

void resetTimers() {
  timer_1s  = false;
  timer_5s  = false;
  timer_20s = false;
  timer_30s = false;
  timer_1m  = false;
  timer_3m  = false;
  timer_5m  = false;

  fuzzy_10s = false;
  fuzzy_20s = false;
  fuzzy_30s = false;
  fuzzy_40s = false;

  timer_temp1 = false;
}

void print1s() {
  #ifdef SERIAL_PRINT
    if (timer_1s) {
      Serial.println(F("."));
    }
  #endif
}
