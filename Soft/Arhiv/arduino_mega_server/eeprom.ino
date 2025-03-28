/*
  Module EEPROM
  part of Arduino Mega Server project
*/

#include <EEPROM.h>

int  eeVolume;
byte eeAddress;

// Buffer ID
#define MAX_ID_DIGS 15
byte buffId2[MAX_ID_DIGS];

// Chars
#define MIN_ID_CHAR 39
#define MAX_ID_CHAR 126

#define EE_ADR_ADDRESS  100
#define EE_ID_ADR       102
#define EE_ID_LEN        15
#define EE_ID_CHECK_ADR 117
#define EE_ID_CHECK_VAL 231
#define EE_NAME_ADR     118
#define EE_NAME_LEN      15

// EEPROM init

void eepromInit() {
  initStart(F("EEPROM"));
  eeVolume = EEPROM.length();
  
  if (!eeCheckName()) {
    eeSetName(SELF_NAME);
  }

  eeAddress = eeGetAddress();
  if (eeAddress == 0 || eeAddress == 255) {
    eeSetAddress(random(1, 255));
    eeAddress = eeGetAddress();
  }
  
  eeCheckId();

  printEeInfo();
  initDone();
}

// Address

void eeSetAddress(byte addr) {
  EEPROM.write(EE_ADR_ADDRESS, addr);
}

byte eeGetAddress() {
  return EEPROM.read(EE_ADR_ADDRESS);
}

// Name

bool eeCheckName() {
  for (int i = EE_NAME_ADR; i < EE_NAME_ADR + EE_NAME_LEN; i++) {
    if (EEPROM[i] != 0 && EEPROM[i] != 255) {return true;}
  }
  return false;
}

void eeSetName(String s) {
  for (int i = EE_NAME_ADR; i < EE_NAME_ADR + EE_NAME_LEN; i++) {
    if (i - EE_NAME_ADR > s.length()) {
      EEPROM[i] = 0;
    } else {
        EEPROM[i] = s[i - EE_NAME_ADR];
      }
  }
}

void eePrintName(String prefix, String suffix) {
  Serial.print(prefix);
  for (int i = EE_NAME_ADR; i < EE_NAME_ADR + EE_NAME_LEN; i++) {
    Serial.print(char(EEPROM[i]));
  }
  Serial.print(suffix);
}

String stringName() {
  String s = "";
  for (byte i = EE_NAME_ADR; i < EE_NAME_ADR + EE_NAME_LEN; i++) {
    if (EEPROM[i] == 0 || EEPROM[i] == 255) {
      break;
    }
    s += char(EEPROM[i]);
  }
  return s;
}

// id

void eeClearId() {
  for (int i = EE_ID_ADR; i < EE_ID_ADR + EE_ID_LEN; i++) {EEPROM[i] = 0;}
  EEPROM[EE_ID_CHECK_ADR] = 0;
}

void eeSetId() {
  for (int i = EE_ID_ADR; i < EE_ID_ADR + EE_ID_LEN; i++) {
    EEPROM[i] = buffId2[i - EE_ID_ADR];
  }
  EEPROM[EE_ID_CHECK_ADR] = EE_ID_CHECK_VAL;
}

bool eeCorrectId() {
  byte current, back;
  byte match    = 0;
  byte increase = 0;
  byte decrease = 0;
  
  for (int i = EE_ID_ADR; i < EE_ID_ADR + EE_ID_LEN; i++) {
    current = EEPROM[i];
    if (current < MIN_ID_CHAR || current > MAX_ID_CHAR) {return false;}
    if (current == back)     {match++;    if (match    > 5) {return false;}}
    if (current == back + 1) {increase++; if (increase > 5) {return false;}}
    if (current == back - 1) {decrease++; if (decrease > 5) {return false;}}
    back = current;
  }
  if (EEPROM[EE_ID_CHECK_ADR] != EE_ID_CHECK_VAL) {return false;}
  return true;
}

void eePrintId(String prefix, String suffix) {
  Serial.print(prefix);
  for (int i = EE_ID_ADR; i < EE_ID_ADR + EE_ID_LEN; i++) {
    Serial.print(char(EEPROM[i]));
  }
  Serial.print(suffix);
}

void eeSetBuffId() {
  for (int i = EE_ID_ADR; i < EE_ID_ADR + EE_ID_LEN; i++) {
    buffId2[i - EE_ID_ADR] = EEPROM[i];
  }
}

void eeCheckId() {
  if (!eeCorrectId()) {
    generateId();
    eeSetId();
  } else {
      eeSetBuffId();
    }
}

// EEPROM Info

void printEeInfo() {
  eePrintName(F(" Name: "), "\n");
  //eePrintId(F("ID: "), "\n");
  Serial.print(F(" Address: ")); Serial.print(eeGetAddress()); Serial.println();
}

/*
void clearEeprom() {
  for (int i = 0; i < eeVolume; i++) {
    EEPROM[i] = 0;
  }
}

void printEeprom(int start, int finish) {
  for (int i = start; i < finish; i++) {
    byte val = EEPROM[i];
    Serial.print(i);
    Serial.print(F("\t"));
    Serial.print(val);
    Serial.print(F("\t"));
    Serial.print(char(val));
    Serial.println();
  }
}
*/

