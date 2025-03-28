/*
  Module Laurent
  part of Arduino Mega Server project
 
  byte CRbyte = 13; // HEX 0x0D
  byte LFbyte = 10; // HEX 0x0A
*/

#ifdef LAURENT_FEATURE

byte LAURENT_IP[] = {192, 168, 1, 19};
#define LAURENT_PORT 2424

#define MAX_LEN_LREQUEST 25
String lrequest = "";

EthernetClient lclient;


void laurentInit() {
  initStart("Laurent");
  timeStamp(); Serial.print(F(" Connect to Laurent... "));
  if (lclient.connect(LAURENT_IP, LAURENT_PORT)) {
    Serial.println(F("OK"));
    lclient.stop();
    
    // Send test command
    timeStamp(); Serial.println(F(" Selftest..."));
    sprintf(buf, "$KE");   
    sendLaurentRequest();

    // Send password (default: "Laurent")
    timeStamp(); Serial.print(F(" Set password..."));
    sprintf(buf, "$KE,PSW,SET,Laurent");   
    sendLaurentRequest();
  } else {
      Serial.println(F("failed"));
    }
  delay(500);
    
  // останавливаем выдачу DATA
  sprintf(buf, "$KE,DAT,OFF");
  sendLaurentRequest();
  delay(100);
  
  // выключаем реле
  sprintf(buf, "$KE,REL,2,0");
  sendLaurentRequest();
  
  moduleLaurent = MODULE_ENABLE;
  initDone();
} // laurentInit()

void sendLaurentRequest() {
  if (lclient.connect(LAURENT_IP, LAURENT_PORT)) {
    timeStamp(); Serial.print(F("Command: "));
    Serial.println(buf);
    lclient.println(buf);
    delay(100);
  
    timeStamp(); Serial.print(F("Answer:  "));
    lrequest = "";
    while(lclient.available() != 0) {
      char c = lclient.read();
      Serial.print(c);
      if (lrequest.length() < MAX_LEN_LREQUEST) {
        lrequest += (c);
      }
    }
    delay(500);
    lclient.stop();
  } else {
      timeStamp(); Serial.println(F("Error sending command"));
    }
} // sendLaurentRequest()

void laurentWorks() {
  if (cycle3m) {
    sprintf(buf, "$KE");
    sendLaurentRequest();
  }
}

#endif // LAURENT_FEATURE
