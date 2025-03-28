/*
  Module PIR
  part of Arduino Mega Server project
*/

#ifdef FEATURE_PIRS

// pins
#define PIR_1_PIN 12
#define PIR_2_PIN 13

#define DETECT_MOTION "Detect motion"
#define END_MOTION    "End motion"
#define PIR_1_NAME    "PIR1"
#define PIR_2_NAME    "PIR2"

// states
int pir1state = LOW; byte pir1 = 0;
int pir2state = LOW; byte pir2 = 0;

// init

void initPirs() {
  pinMode(PIR_1_PIN, INPUT);
  pinMode(PIR_2_PIN, INPUT);
  modulePirs = ENABLE;
  started(F("PIR"), true);
}

#ifdef EVENTS_PIRS
  void pirMess(char s[], char s2[]) {
    timeStamp();
    Serial.print(s);
    Serial.print(' ');
    Serial.println(s2);
  }
#endif

void pir1Actions(byte state, char mess[], byte major) {
  #ifdef EVENTS_PIRS
    pirMess(mess, PIR_1_NAME);
  #endif
  #ifdef FEATURE_MAJOR
    sendRequestM(PIR_1_NAME, major);
  #endif
  #ifdef FEATURE_LEDS
    if (modeLed == LED_PIR) {
      if (state == HIGH) {
        //green(led1, bright);
      } else {
          //black(led1);
        }
    }
  #endif
  pir1state = state;
}

void pir2Actions(byte state, char mess[], byte major) {
  #ifdef EVENTS_PIRS
    pirMess(mess, PIR_2_NAME);
  #endif
  #ifdef FEATURE_MAJOR
    sendRequestM(PIR_2_NAME, major);
  #endif
  #ifdef FEATURE_LEDS
    if (modeLed == LED_PIR) {
      if (state == HIGH) {
        //red(led1, bright);
      } else {
          //black(led1);
        }
    }
  #endif
  pir2state = state;
}

void pir1Work() {
  pir1 = digitalRead(PIR_1_PIN);
  if (pir1 == HIGH) {
    if (pir1state == LOW)  {pir1Actions(HIGH,   END_MOTION, 1);}
  } else {
    if (pir1state == HIGH) {pir1Actions(LOW, DETECT_MOTION, 0);}
  }
}

void pir2Work() {
  pir2 = digitalRead(PIR_2_PIN);
  if (pir2 == HIGH) {
    if (pir2state == LOW)  {pir2Actions(HIGH,   END_MOTION, 1);}
  } else {
    if (pir2state == HIGH) {pir2Actions(LOW, DETECT_MOTION, 0);}
  }
}

void workPirs() {
  pir1Work();
  pir2Work();
}

#endif // FEATURE_PIRS
