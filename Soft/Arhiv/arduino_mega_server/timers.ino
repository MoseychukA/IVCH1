/*
  Module Timers
  part of Arduino Mega Server project
*/

// Cycles
unsigned long timeSec;
unsigned long timer1s;
unsigned long timer5s;
unsigned long timer20s;
unsigned long timer30s;
unsigned long timer1m;
unsigned long timer3m;
unsigned long timer5m;

void timersInit() {
  unsigned long uptimeSec = millis() / 1000;
  timeSec  = uptimeSec;
  timer1s  = uptimeSec;  
  timer5s  = uptimeSec; 
  timer20s = uptimeSec;
  timer30s = uptimeSec;
  timer1m  = uptimeSec;
  timer3m  = uptimeSec;
  timer5m  = uptimeSec;
}

void timersWorks() {
  timeSec = millis() / 1000;
    if (timeSec - timer1s  >=  1)  {
                                    timer1s  = timeSec; cycle1s  = true;
    if (timeSec - timer5s  >=  5)  {timer5s  = timeSec; cycle5s  = true;}
    if (timeSec - timer20s >= 20)  {timer20s = timeSec; cycle20s = true;}
    if (timeSec - timer30s >= 30)  {timer30s = timeSec; cycle30s = true;}
    if (timeSec - timer1m  >= 60)  {timer1m  = timeSec; cycle1m  = true;}
    if (timeSec - timer3m  >= 180) {timer3m  = timeSec; cycle3m  = true;}
    if (timeSec - timer5m  >= 300) {timer5m  = timeSec; cycle5m  = true;}
  }
}

void eraseCyclos() {
  cycle1s  = false;
  cycle5s  = false;
  cycle20s = false;
  cycle30s = false;
  cycle1m  = false;
  cycle3m  = false;
  cycle5m  = false;
}

// Random Timer

unsigned long tmrRnd = millis();
unsigned long intervaRnd = 0;

bool timerRnd(long minInt, long maxInt) {
  unsigned long dely = millis() - tmrRnd;
  if (dely > intervaRnd) {
    intervaRnd = random(minInt, maxInt);
    tmrRnd = millis();
    return true;
  }
  return false;
}

// Profiling

unsigned long cycle_start = 0;

void profStart() {
  cycle_start = millis() / 1000;
}

void profCalc() {
  unsigned long cycle_end = millis() / 1000;
  unsigned long cycle_delta = cycle_end - cycle_start;
   
  if (cycle_delta > 0) {
    //Serial.print("-->");
    //Serial.print(cycle_delta);
    //Serial.println("s");
  }
}

// Print

void print1s() {
  if (cycle1s) {
    Serial.println(F("."));
  }
}

