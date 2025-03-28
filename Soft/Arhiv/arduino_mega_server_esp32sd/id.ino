/*
  Module ID
  part of Arduino Mega Server project
*/

#define MAX_ANALOG_PORTS  19
#define MAX_DIGITAL_PORTS 39

#define MAX_CODE 34
byte code[] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F','G','H','J','K','L','M','N','P','Q','R','S','T','U','V','W','X','Y','Z'};

// random init

void initRandom() {
  long magic = 0;
  getSeed();
  for (int i = 0; i < random(9, 200); i++) {
    randomSeed(getSeed());
    for (int j = 0; j < random(9, 9999) + 1; j++) {
      magic += random(9, 9999);
    }
  }
  randomSeed(magic);
  started("Random", false);
}

int getSeed() {
  int seed = 0;
  for (byte i = 0; i < MAX_ANALOG_PORTS;  i++) {seed +=  analogRead(i);}
  for (byte i = 0; i < MAX_DIGITAL_PORTS; i++) {seed += digitalRead(i);} 
  return seed;
}

bool validChar(byte b) {
  for (byte i = 0; i < MAX_CODE; i++) {
    if (code[i] == b) {
      return true;
    }
  }
  return false;
}

void generateId() {
  for (byte i = 0; i < MAX_ID_DIGS; i++) {
    buffId[i] = code[random(0, MAX_CODE)];
  }
  
  /*
  String chip  = String(ESP.getChipId(),      HEX);

  String flash = String(ESP.getFlashChipId(), HEX);
  chip.toUpperCase();
  flash.toUpperCase();
 
  for (byte i = 0; i < chip.length(); i++) {buffId[i] = chip.charAt(i);}
  buffId[chip.length()] = '-';
  byte fstart = chip.length() + 1;
  for (byte i = fstart; i < fstart + flash.length(); i++) {
    if (i > MAX_ID_DIGS - 1) {break;}
    buffId[i] = flash.charAt(i - fstart);
  }
  buffId[fstart + flash.length()] = '-';
  */
  
} // generateId()

String stringId() {
  String s = "";
  for (byte i = 0; i < MAX_ID_DIGS; i++) {
    s += char(buffId[i]);
  }
  return s;
}

#ifdef SERIAL_PRINT
  void printId(String prefix, String suffix) {
    Serial.print(prefix);
    for (byte i = 0; i < MAX_ID_DIGS; i++) {
      Serial.print(char(buffId[i]));
    }
    Serial.print(suffix);
  }
#endif

