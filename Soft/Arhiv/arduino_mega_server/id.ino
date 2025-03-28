/*
  Module ID
  part of Arduino Mega Server project
*/

// Hardware
#define MAX_ANALOG_PORTS  16
#define MAX_DIGITAL_PORTS 54

// random init

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

int getSeed() {
  int seed = 0;
  for (byte i = 0; i < MAX_ANALOG_PORTS;  i++) {seed +=  analogRead(i);}
  for (byte i = 0; i < MAX_DIGITAL_PORTS; i++) {seed += digitalRead(i);}
  return seed;
}

void generateId() {
  randomInit();
  for (byte i = 0; i < MAX_ID_DIGS; i++) {
    buffId2[i] = random(MIN_ID_CHAR, MAX_ID_CHAR);
  }
}

String stringId() {
  String s = "";
  for (byte i = 0; i < MAX_ID_DIGS; i++) {
    s += char(buffId2[i]);
  }
  return s;
}

void printId(String prefix, String suffix) {
  Serial.print(prefix);
  for (byte i = 0; i < MAX_ID_DIGS; i++) {
    Serial.print(char(buffId2[i]));
  }
  Serial.print(suffix);
}

