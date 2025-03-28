/*
  Module Timers
  part of Arduino Mega Server project
*/

unsigned long timer1s;
unsigned long timer5s;
unsigned long timer10s;
unsigned long timer20s;
unsigned long timer30s;
unsigned long timer1m;
unsigned long timer3m;
unsigned long timer5m;

void initTimers() {
  unsigned long uptimeSec = millis() / 1000;
  timer1s  = uptimeSec;  
  timer5s  = uptimeSec; 
  timer10s = uptimeSec;
  timer20s = uptimeSec;
  timer30s = uptimeSec;
  timer1m  = uptimeSec;
  timer3m  = uptimeSec;
  timer5m  = uptimeSec;
}

void eraseCyclos() {
  cycle1s  = false;
  cycle5s  = false;
  cycle20s = false;
  cycle10s = false;
  cycle30s = false;
  cycle1m  = false;
  cycle3m  = false;
  cycle5m  = false;
}

void workTimers() {
  unsigned long timeSec = millis() / 1000;
  if (timeSec - timer1s >=   1) {
                                    timer1s  = timeSec; cycle1s  = true;
    if (timeSec - timer5s  >=   5) {timer5s  = timeSec; cycle5s  = true;}
    if (timeSec - timer20s >=  20) {timer20s = timeSec; cycle20s = true;}
    if (timeSec - timer10s >=  10) {timer10s = timeSec; cycle10s = true;}
    if (timeSec - timer30s >=  30) {timer30s = timeSec; cycle30s = true;}
    if (timeSec - timer1m  >=  60) {timer1m  = timeSec; cycle1m  = true;}
    if (timeSec - timer3m  >= 180) {timer3m  = timeSec; cycle3m  = true;}
    if (timeSec - timer5m  >= 300) {timer5m  = timeSec; cycle5m  = true;}
  }
}

