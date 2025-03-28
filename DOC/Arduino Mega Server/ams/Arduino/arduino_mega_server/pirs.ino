/*
  Module PIRs
  part of Arduino Mega Server project
*/

#ifdef PIRS_FEATURE

//#define EVENTS_PIRS

// Pins
#define PIR1_PIN 22
#define PIR2_PIN 23
#define PIR3_PIN 24
#define PIR4_PIN 25
#define PIR5_PIN 26

// states
int pir1state = LOW; int pir1 = 0;
int pir2state = LOW; int pir2 = 0;
int pir3state = LOW; int pir3 = 0;
int pir4state = LOW; int pir4 = 0;
int pir5state = LOW; int pir5 = 0;

// init
void pirsInit() {
  pinMode(PIR1_PIN, INPUT);
  pinMode(PIR2_PIN, INPUT);
  pinMode(PIR3_PIN, INPUT);
  pinMode(PIR4_PIN, INPUT);
  pinMode(PIR5_PIN, INPUT);
  modulePirs = MODULE_ENABLE;
  started("PIR");
}

void detect(char s[]) {
  #ifdef EVENTS_PIRS
    timeStamp();
    Serial.print("Detect motion ");
    Serial.println(s);
  #endif
}

void endMmotion(char s[]) {
  #ifdef EVENTS_PIRS
    timeStamp();
    Serial.print("End motion ");
    Serial.println(s);
  #endif
}

void pirsWorks() {
  // pir1
  pir1 = digitalRead(PIR1_PIN);
  if (pir1 == HIGH) { 
    if (pir1state == LOW) {
      detect("PIR 1");
      pir1state = HIGH;
      #ifdef MAJORDOMO_FEATURE
        sendRequestM("pirSTD", 1);
      #endif
      #ifdef LEDS_FEATURE
        if (modeLED == LED_PIR) {
          //green(led1, bright);
        }
      #endif
    }
  } else {
    if (pir1state == HIGH){
      endMmotion("PIR 1");
      pir1state = LOW;
      #ifdef MAJORDOMO_FEATURE
        sendRequestM("pirSTD", 0);
      #endif
      #ifdef LEDS_FEATURE
        //black(led1);
      #endif
    }
  }
    
  // pir2
  pir2 = digitalRead(PIR2_PIN);
  if (pir2 == HIGH) { 
    if (pir2state == LOW) {
      detect("PIR 2");
      pir2state = HIGH;
      #ifdef MAJORDOMO_FEATURE
        sendRequestM("pirHAL", 1);
      #endif
      #ifdef LEDS_FEATURE
        if (modeLED == LED_PIR) {
          //blue(led1, bright);
        }
      #endif
    }
  } else {
    if (pir2state == HIGH){
      endMmotion("PIR 2");
      pir2state = LOW;
      #ifdef MAJORDOMO_FEATURE
        sendRequestM("pirHAL", 0);
      #endif
      #ifdef LEDS_FEATURE
        //black(led1);
      #endif
    }
  }

  // pir3
  pir3 = digitalRead(PIR3_PIN);
  if (pir3 == HIGH) { 
    if (pir3state == LOW) {
      detect("PIR 3");
      pir3state = HIGH;
      // setReqSens01("pirKID", 1);
      /*if (MODE == LED_PIR_01) {
        yellow(BRIGHT);
      }  */
    }
  } else {
    if (pir3state == HIGH){
      endMmotion("PIR 3");
      pir3state = LOW;
      // setReqSens01("pirKID", 0);
      #ifdef LEDS_FEATURE
        //black(led1);
      #endif
    }
  }

  // pir4
  pir4 = digitalRead(PIR4_PIN);
  if (pir4 == HIGH) { 
    if (pir4state == LOW) {
      detect("PIR 4");
      pir4state = HIGH;
      #ifdef MAJORDOMO_FEATURE
        //sendRequestM("pirKUH", 1);
      #endif
      #ifdef LEDS_FEATURE
        if (modeLED == LED_PIR) {
          //cyan(led1, bright);
        } 
      #endif
    }
  } else {
    if (pir4state == HIGH){
      endMmotion("PIR 4");
      pir4state = LOW;
      #ifdef MAJORDOMO_FEATURE
        //sendRequestM("pirKUH", 0);
      #endif
      #ifdef LEDS_FEATURE
        //black(led1);
      #endif
    }
  }

  // pir5
  pir5 = digitalRead(PIR5_PIN);
  if (pir5 == HIGH) { 
    if (pir5state == LOW) {
      detect("PIR 5");
      pir5state = HIGH;
      #ifdef MAJORDOMO_FEATURE
        sendRequestM("pirPRH", 1);
      #endif
      #ifdef LEDS_FEATURE
        if (modeLED == LED_PIR) {
          //red(led1, bright);
        }
      #endif
    }
  } else {
    if (pir5state == HIGH){
      endMmotion("PIR 5");
      pir5state = LOW;
      #ifdef MAJORDOMO_FEATURE
        sendRequestM("pirPRH", 0);
      #endif
      #ifdef LEDS_FEATURE
        //black(led1);
      #endif
    }
  }
} // pirsWorks()

#endif // PIRS_FEATURE
