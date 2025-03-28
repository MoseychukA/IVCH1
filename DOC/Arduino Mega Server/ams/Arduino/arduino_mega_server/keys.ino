/*
  Module Keys
  part of Arduino Mega Server project
*/

#ifdef KEYS_FEATURE

int goLight = 0;
#define KEY_PIN 3

void keysInit() {
  pinMode(KEY_PIN, OUTPUT);
  moduleKeys = MODULE_ENABLE;
  started("Keys");
}

void keysWorks() {
  if (goLight == 1) {
    digitalWrite(KEY_PIN, LOW);
  } else {
      digitalWrite(KEY_PIN, HIGH);   
    }
}

#endif // KEYS_FEATURE
