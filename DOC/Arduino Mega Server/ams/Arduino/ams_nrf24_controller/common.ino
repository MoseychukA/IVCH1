/*
  Modul Common
  AMS nRF24 controller
  part of Arduino Mega Server project
*/

// net stat

#ifdef NRF24_FEATURE
  void resetNetStat() {
    sendPacketsStat    = sendPackets;
    sendRetrysStat     = sendRetrys;
    sendErrorsStat     = sendErrors;
    receivePacketsStat = receivePackets;
    successPacketsStat = successPackets;
    sendPackets    = 0;
    sendRetrys     = 0;
    sendErrors     = 0;
    receivePackets = 0;
    successPackets = 0;
  }

  #ifdef SERIAL_PRINT
    void printNetStat() {
      Serial.print(F("Send packets:    "));
      Serial.println(sendPacketsStat);
      Serial.print(F("Recive packets:  "));
      Serial.println(receivePacketsStat);
      Serial.print(F("Success packets: "));
      Serial.println(successPacketsStat);
      Serial.print(F("Send errors:     "));
      Serial.println(sendErrorsStat);
      Serial.print(F("Send retrys:     "));
      Serial.println(sendRetrysStat);
    }
  #endif

  // check state

  byte currCheck = 0;
  void checkState() {
    byte const MAX_CHECK_STATE = 3;
    currCheck++;
    if (currCheck > MAX_CHECK_STATE) {currCheck = 1;}
    switch (currCheck) {
      case 1: if (lowMemory())  {sendMemoryAlarm();}  break;
      case 2: if (lowBattery()) {sendBatteryAlarm();} break;
      case 3: 
        #ifdef SIMULATION
          if (lowSelfTemp()) {sendSelfTempAlarm();}
        #endif
        break;
    }
  }

  // send data

  byte currData = 0;
  void sendData() {
    byte const MAX_DATA = 8;
    currData++;
    if (currData > MAX_DATA) {currData = 1;}
    switch (currData) {
      case 1: sendPacket(DTA_NAME);      break;
      case 2: sendPacket(DTA_ID);        break;
      case 3: sendPacket(DTA_ADDR);      break;
      case 4: sendPacket(DTA_FREE_MEM);  break;
      case 5: sendPacket(DTA_BATTERY);   break;
      case 6: sendPacket(DTA_HEALTH);    break;
      case 7: sendPacket(DTA_SELF_TEMP); break;
      case 8: sendPacket(DTA_TEMP_1);    break;
    }
  }

#endif // NRF24_FEATURE

// simulation

#ifdef SIMULATION
  void simulationWorks() {
    if (timer_5s) {
      simFreeMemP();
    }
    if (timer_20s) {
      simBattery();
      simSelfTemp();
    }
    
  }
#endif

