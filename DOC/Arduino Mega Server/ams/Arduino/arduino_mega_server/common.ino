/*
  Module Common
  part of Arduino Mega Server project
*/

// Start modules

void started(String s) {
  if (printTimeStamp) {timeStamp();}
  Serial.print(F("Module ")); Serial.print(s); Serial.println(F("... started"));
}

void initStart(String s) {
  if (printTimeStamp) {timeStamp();}
  Serial.print(F("Module ")); Serial.print(s); Serial.println(F("..."));
}

void initDone() {
  if (printTimeStamp) {timeStamp();}
  Serial.println(F("============"));
}

// Print

void printIp(byte ip[]) {
  for (byte i = 0; i < 4; i++) {
    Serial.print(ip[i]);
    if (i == 3) {break;}
    Serial.print(".");
  }
}

void printMac(byte mac[]) {
  for (byte i = 0; i < 6; i++) {
    Serial.print(mac[i], HEX);
    if (i == 5) {break;}
    Serial.print(" ");
  }
}

// Send

void sendIpClient(byte ip[], EthernetClient cl) {
  for (byte i = 0; i < 4; i++) {
    cl.print(ip[i]);
    if (i == 3) {break;}
    cl.print(".");
  }
}

