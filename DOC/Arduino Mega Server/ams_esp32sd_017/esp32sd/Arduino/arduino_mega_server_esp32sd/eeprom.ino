/*
  Module EEPROM
  part of Arduino Mega Server project

  NVS (ESP32 Non-volatile storage), similar EEPROM
*/

#include <Preferences.h>

#define NVS_ADDR          "addr"
#define NVS_NAME          "name"
#define NVS_ID            "id"
#define NVS_ID_MARKER     "idCheck"
#define NVS_ID_MARKER_VAL 231

#define MAX_ID_DIGS 15
byte buffId[MAX_ID_DIGS];

Preferences prefs;

void initEeprom() {
  initStart(F("EEPROM (NVS)"), false);
  prefs.begin("nvs", false); // RW mode

  eeCheckAddress();
  eeCheckId();
  
  if (!eeCheckName()) {eeSetName(SELF_NAME);}
 
  #ifdef SERIAL_PRINT
    printEeInfo();
  #endif
  initDone(false);
}

// Address

void eeSetAddress(byte addr) {
  prefs.putChar(NVS_ADDR, addr);
}

byte eeGetAddress() {
  return prefs.getChar(NVS_ADDR, 0);
}

#ifdef SERIAL_PRINT
  void eePrintAddress(String prefix, String suffix) {
    Serial.print(prefix); Serial.print(eeGetAddress()); Serial.print(suffix);
  }
#endif

void eeCheckAddress() {
  if (eeGetAddress() == 0) {
    eeSetAddress(random(1, 255));
  }
}

// ID

void eeSetId() {
  prefs.putBytes(NVS_ID, buffId, MAX_ID_DIGS);
  prefs.putUChar(NVS_ID_MARKER, NVS_ID_MARKER_VAL);
}

void eeGetId() {
  prefs.getBytes(NVS_ID, buffId, MAX_ID_DIGS);
}

void eeClearId() {
  prefs.remove(NVS_ID);
}

bool eeCheckIdMarker() {
  if (prefs.getUChar(NVS_ID_MARKER) == NVS_ID_MARKER_VAL) {
    return true;
  } else {
      return false;
    }
}

bool eeCorrectId() {
  byte current, back;
  byte match    = 0;
  byte increase = 0;
  byte decrease = 0;
  
  eeGetId();
  for (int i = 0; i < MAX_ID_DIGS; i++) {
    current = buffId[i];
    if (!validChar(current)) {return false;}
    if (current == back)     {match++;    if (match    > 5) {return false;}}
    if (current == back + 1) {increase++; if (increase > 5) {return false;}}
    if (current == back - 1) {decrease++; if (decrease > 5) {return false;}}
    back = current;
  }
  if (!eeCheckIdMarker()) {return false;}
  return true;
} // eeCorrectId()

#ifdef SERIAL_PRINT
  void eePrintId(String prefix, String suffix) {
    Serial.print(prefix);
    eeGetId();
    for (int i = 0; i < MAX_ID_DIGS; i++) {
      if (i != 0 && i % 5 == 0) {Serial.print(F("-"));}
      Serial.print((char)buffId[i]);
    }
    Serial.print(suffix);
  }
#endif

void eeCheckId() {
  if (eeCorrectId()) {
    eeGetId(); // ID already in buffId
  } else {
      generateId();
      eeSetId();
    }
}

// Name

boolean eeCheckName() {
  if (prefs.getString(NVS_NAME, "") != "") {
    return true;
  } else {
      return false;
    }
}

void eeSetName(String s) {
  prefs.putString(NVS_NAME, s);
}


#ifdef SERIAL_PRINT
  void eePrintName(String prefix, String suffix) {
    Serial.print(prefix); Serial.print(prefs.getString(NVS_NAME)); Serial.print(suffix);
  }
#endif


String stringName() {
  return prefs.getString(NVS_NAME);
}

// EEPROM (NVS) Info

#ifdef SERIAL_PRINT
  void printEeInfo() {
    eePrintName   (F(" Name:    "), "\n");
    eePrintId     (F(" ID:      "), "\n");
    eePrintAddress(F(" Address: "), "\n");
  }
#endif


void clearEeprom() {
  prefs.clear();
}

/*
  Notes:
  ======

  Put
  ---
  
  size_t putChar   (const char* key,      int8_t value);
  size_t putUChar  (const char* key,     uint8_t value);
  
  size_t putShort  (const char* key,     int16_t value);
  size_t putUShort (const char* key,    uint16_t value);
  
  size_t putInt    (const char* key,     int32_t value);
  size_t putUInt   (const char* key,    uint32_t value);
  size_t putLong   (const char* key,     int32_t value);
  size_t putULong  (const char* key,    uint32_t value);
  
  size_t putLong64 (const char* key,     int64_t value);
  size_t putULong64(const char* key,    uint64_t value);
  
  size_t putFloat  (const char* key,     float_t value);
  size_t putDouble (const char* key,    double_t value);
  
  size_t putBool   (const char* key,        bool value);
  
  size_t putString (const char* key, const char* value);
  size_t putString (const char* key,      String value);
  
  size_t putBytes  (const char* key, const void* value, size_t len);

  Get
  ---
  
  int8_t   getChar   (const char* key, int8_t   defaultValue = 0);
  uint8_t  getUChar  (const char* key, uint8_t  defaultValue = 0);

  int16_t  getShort  (const char* key, int16_t  defaultValue = 0);
  uint16_t getUShort (const char* key, uint16_t defaultValue = 0);

  int32_t  getInt    (const char* key, int32_t  defaultValue = 0);
  uint32_t getUInt   (const char* key, uint32_t defaultValue = 0);

  int32_t  getLong   (const char* key, int32_t  defaultValue = 0);
  uint32_t getULong  (const char* key, uint32_t defaultValue = 0);

  int64_t  getLong64 (const char* key, int64_t  defaultValue = 0);
  uint64_t getULong64(const char* key, uint64_t defaultValue = 0);

  float_t  getFloat  (const char* key, float_t  defaultValue = NAN);
  double_t getDouble (const char* key, double_t defaultValue = NAN);

  bool     getBool   (const char* key, bool     defaultValue = false);

  size_t   getString (const char* key, char* value, size_t maxLen);
  String   getString (const char* key, String   defaultValue = String());

  size_t   getBytes  (const char* key, void * buf, size_t maxLen);
*/

