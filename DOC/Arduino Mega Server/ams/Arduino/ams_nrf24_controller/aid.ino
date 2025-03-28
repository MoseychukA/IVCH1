/*
  Modul ID
  AMS nRF24 controller
  part of Arduino Mega Server project
*/

// admissible chars
byte const MIN_ID_CHAR = 39;
byte const MAX_ID_CHAR = 126;

int getSeed() {
  byte const MAX_ANALOG_PORTS  =  7;
  byte const MAX_DIGITAL_PORTS = 13;
  int seed = 0;
  for (int i = 0; i < MAX_ANALOG_PORTS;  i++) {seed +=  analogRead(i);}
  for (int i = 0; i < MAX_DIGITAL_PORTS; i++) {seed += digitalRead(i);} 
  return seed;
}

void randomInit() {
  long magic = 0;
  getSeed();
  for (int i = 0; i < random(9, 200); i++) {
    randomSeed(getSeed());
    for (int j = 0; j < random(9, 9999) + 1; j++) {
      magic += random(9, 9999);
    }
  }
  randomSeed(magic);
}

#ifdef ID_FEATURE

byte buffId[15];

void generateId() {
  long magic = 0;
  getSeed();
  for (int i = 0; i < random(9, 200); i++) {
    randomSeed(getSeed());
    for (int j = 0; j < random(9, 9999) + 1; j++) {
      magic += random(9, 9999);
    }
  }
  randomSeed(magic);
  for (byte i = 0; i < 15; i++) {
    buffId[i] = random(MIN_ID_CHAR, MAX_ID_CHAR);
  }
}

#ifdef SERIAL_PRINT
  void printId(String prefix, String suffix) {
    Serial.print(prefix);
    for (byte i = 0; i < 15; i++) {
      Serial.print(char(buffId[i]));
    }
    Serial.print(suffix);
  }
#endif

String stringId() {
  String s = "";
  for (byte i = 0; i < 15; i++) {
    s += char(buffId[i]);
  }
  return s;
}

#endif // ID_FEATURE
  
