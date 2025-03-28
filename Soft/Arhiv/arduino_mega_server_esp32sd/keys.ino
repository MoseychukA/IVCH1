/*
  Module Keys
  part of Arduino Mega Server project
*/

#ifdef FEATURE_KEYS

#define KEY_PIN 14

int goLight = 0; // key (light) control

void initKeys() {
  pinMode(KEY_PIN, OUTPUT);
  moduleKeys = ENABLE;
  started(F("Keys"), true);
}

void workKeys() {
  if (goLight == 1) {
    digitalWrite(KEY_PIN, LOW);
  } else {
      digitalWrite(KEY_PIN, HIGH);   
    }
}

#endif // FEATURE_KEYS
