/*
  Modul Hard
  AMS nRF24 controller
  part of Arduino Mega Server project
*/

// Free mem

int freeMem() {
  extern int __heap_start, *__brkval;
  int v;
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}

#ifdef SIMULATION
  byte simMemP;
  
  void simFreeMemP() {
    simMemP = random(0, 101);
  }

  int freeMemP() {
    return simMemP;
  }
#else
  int freeMemP() {
    return freeMem() / (MEMORY / 100);
  }
#endif // SIMULATION

byte healthMem() {
  byte health = 0;
  int freeP = freeMemP();
  
  if (freeP > 50) {health = 100;}
  if (freeP <= 50 && freeP > 40) {health = 80;}
  if (freeP <= 40 && freeP > 30) {health = 50;}
  if (freeP <= 30 && freeP > 20) {health = 20;}
  if (freeP <= 20) {health = 0;}
  return health;
}

byte const LOW_MEMORY = 30;

boolean lowMemory() {
  if (freeMemP() < LOW_MEMORY) {
    return true;
  } else {
      return false;
    }
}

// Vcc

long getRawVcc() {
  long result;
  // read 1.1V reference against AVcc
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  delay(2); // wait for Vref to settle
  ADCSRA |= _BV(ADSC); // convert
  while (bit_is_set(ADCSRA,ADSC));
  result = ADCL;
  result |= ADCH<<8;
  result = 1126400L / result; // back-calculate AVcc in mV
  return result;
}
/*
float getVcc() {
  float result;
  float k = 0.14;
  long rawVcc = getRawVcc();
  result = (float)rawVcc * 0.001 - k;
  return result;
}
*/
#define MIN_V 2800
#define MAX_V 3620

#ifdef SIMULATION
  byte simBatt;
  
  void simBattery() {
    simBatt = random(0, 101);
  }

  int battery() {
    return simBatt;
  }
#else
int battery() {
  int batteryV = getRawVcc();
  int result = map(batteryV, MIN_V, MAX_V, 0, 100);
  return min(result, 100);
}
#endif // SIMULATION


byte const LOW_BATTERY = 30;

boolean lowBattery() {
  if (battery() < LOW_BATTERY) {
    return true;
  } else {
      return false;
    }
}

// Self temperature

#ifdef SIMULATION
  float const LOW_SELF = 90.0;
  float simSelf;
  
  void simSelfTemp() {
    simSelf = random(0, 41);
  }

  float getSelfTemp() {
    return simSelf;
  }

  boolean lowSelfTemp() {
  if (getSelfTemp() < LOW_SELF) {return true;}
                           else {return false;}
  }
  
#else
  float getSelfTemp() {
    float tGain = 0.9338;
    float tOffset = 300.0; // 290.7
    signed long resultTemp;
    float resultTempFloat;
    // read temperature sensor against 1.1V reference
    ADMUX = _BV(REFS1) | _BV(REFS0) | _BV(MUX3);
    delay(10); // wait for Vref to settle
    ADCSRA |= _BV(ADSC); // convert
    while (bit_is_set(ADCSRA,ADSC));
    resultTemp = ADCL;
    resultTemp |= ADCH<<8;
    resultTempFloat = (float)resultTemp * tGain - tOffset; // apply calibration correction
    return resultTempFloat;
  }
#endif // SIMULATION

// Health

byte healthController() {
  return healthMem();
}

