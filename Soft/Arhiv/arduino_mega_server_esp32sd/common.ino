/*
  Module Common
  part of Arduino Mega Server project
*/

// Print init

void initStart(String s, bool ts) {
  if (ts) {timeStamp();}
  Serial.print(F("Module ")); Serial.print(s); Serial.println(F("..."));
}

void initDone(bool ts) {
  if (ts) {timeStamp();}
  Serial.println(F("==========="));
}

void started(String s, bool ts) {
  if (ts) {timeStamp();}
  Serial.print(F("Module ")); Serial.print(s);
  Serial.println(F("... started"));
}

// Print values

void printValue(String parameter, String value) {
  Serial.print(parameter); Serial.print(": "); Serial.println(value);
}

void printMeasure(String parameter, String value, String measure) {
  Serial.print(parameter); Serial.print(value); Serial.println(measure);
}

// Print network

void printMac(byte mac[]) {
  for (byte i = 0; i < 6; i++) {
    Serial.print(mac[i], HEX);
    if (i == 5) {break;}
    Serial.print(" ");
  }
}

// String network

String stringIp(byte ip[]) {
  String s = "";
  for (byte i = 0; i < 4; i++) {
    s += ip[i];
    if (i == 3) {return s;}
    s += '.';
  }
}
// Welcome

void printWelcome() {
  Serial.println();
  Serial.print(F("AMS for "));
  Serial.print(SELF_NAME);
  Serial.println(F(" started..."));
}

void printInitDone() {
  Serial.print(F("GLOBAL Init DONE (")); Serial.print(millis() / 1000); Serial.println(F("s)"));
  Serial.println();
  Serial.println(F("AMS WORK"));
  timeStamp(); printFreeMem("");
}

// Lifer

byte lifer;

void setLifer() {
  lifer++;
  if (lifer > 6) {
    lifer = 0;
  }
}

