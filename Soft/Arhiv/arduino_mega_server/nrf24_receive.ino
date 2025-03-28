/*
  Module nRF24 / Receive
  part of Arduino Mega Server project
*/

#ifdef NRF24_FEATURE

// Commands

void parseCommands() {
  switch (rxCommand) {
    case SET_NAME:
      //eeSetName(rxData);
      //eePrintName(F("Set: "), "\n");
      break;
    case SET_ADDR:
      //eeSetAddress(rxData.toInt());
      //Serial.print(F("Set: "));
      //Serial.println(eeGetAddress());
      break;
  } // switch (rxCommand)
} // parseCommands()

// Requests

void parseRequests() {
  switch (rxCommand) {
    //case GET_NAME:      setNotRequired(&sendName);     setPacketType(&sendName,     TYPE_ANSWER); sendPacket(DTA_NAME);      break;
    //case GET_FREE_MEM:  setNotRequired(&sendFreeMem);  setPacketType(&sendFreeMem,  TYPE_ANSWER); sendPacket(DTA_FREE_MEM);  break;
    //case GET_BATTERY:   setNotRequired(&sendBattery);  setPacketType(&sendBattery,  TYPE_ANSWER); sendPacket(DTA_BATTERY);   break;
    //case GET_SELF_TEMP: setNotRequired(&sendSelfTemp); setPacketType(&sendSelfTemp, TYPE_ANSWER); sendPacket(DTA_SELF_TEMP); break;
    //case GET_TEMP_1:    setNotRequired(&sendTemp1);    setPacketType(&sendTemp1,    TYPE_ANSWER); sendPacket(DTA_TEMP_1);    break;
    //case GET_ID:      setNotRequired(&sendId);       setPacketType(&sendId,       TYPE_ANSWER); sendPacket(DTA_ID);        break;
  }
}

// Data

void parseData() {
  switch (rxCommand) {
    case DTA_NAME:        nrf1Name     = rxData;           makeEcho(); sendPacket(BAK_ECHO); break;
    case DTA_ID:          nrf1Id       = rxData;           makeEcho(); sendPacket(BAK_ECHO); break;
    case DTA_ADDR:        nrf1Addr     = rxAddr;           makeEcho(); sendPacket(BAK_ECHO); break;
    case DTA_FREE_MEM:    nrf1FreeMem  = rxData.toInt();   makeEcho(); sendPacket(BAK_ECHO); break;
    case DTA_BATTERY:     nrf1Battery  = rxData.toInt();   makeEcho(); sendPacket(BAK_ECHO); break;
    case DTA_HEALTH:      nrf1Health   = rxData.toInt();   makeEcho(); sendPacket(BAK_ECHO); break;
    case DTA_SELF_TEMP:   nrf1SelfTemp = rxData.toFloat(); makeEcho(); sendPacket(BAK_ECHO); break;
    case DTA_TEMP_1:      nrf1Temp1    = rxData.toFloat(); makeEcho(); sendPacket(BAK_ECHO); break;
  }
}

// Alarms

void checkAlarm(struct packet *p) {
  resetSender(p);
  successPackets++;
  Serial.print(F("ALARM: "));
  Serial.print(p->command);
  Serial.println(F(" DONE"));
}

void parseAlarms() {
  switch (rxCommand) {
    case DTA_NAME:       makeEcho(); sendPacket(BAK_ECHO); break;
    case DTA_ID:         makeEcho(); sendPacket(BAK_ECHO); break;
    case DTA_ADDR:       makeEcho(); sendPacket(BAK_ECHO); break;
    case DTA_FREE_MEM:   makeEcho(); sendPacket(BAK_ECHO); break;
    case DTA_BATTERY:    makeEcho(); sendPacket(BAK_ECHO); break;
    case DTA_SELF_TEMP:  makeEcho(); sendPacket(BAK_ECHO); break;
    case DTA_HEALTH:     makeEcho(); sendPacket(BAK_ECHO); break;
    case DTA_TEMP_1:     makeEcho(); sendPacket(BAK_ECHO); break;
  }
}

#endif // NRF24_FEATURE
