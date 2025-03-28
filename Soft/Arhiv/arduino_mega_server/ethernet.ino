/*
  Module Ethernet
  part of Arduino Mega Server project
*/

void ethernetInit() {
  initStart(F("Ethernet"));
  Ethernet.begin(SELF_MAC, SELF_IP, GATEWAY);
  Serial.print(F(" MAC: ")); printMac(SELF_MAC); Serial.println();
  Serial.print(F(" IP: "));  printIp (SELF_IP);  Serial.println();
  initDone();
}

