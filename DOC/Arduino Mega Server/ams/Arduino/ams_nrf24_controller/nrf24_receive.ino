/*
  Modul nRF24 Receive
  AMS nRF24 controller
  part of Arduino Mega Server project
*/

#ifdef NRF24_FEATURE

// Commands

void parseCommands() {
  switch (rxCommand) {
    case SET_NAME:
      eeSetName(rxData);
      #ifdef SERIAL_PRINT
        eePrintName(F("Set: "), "\n");
      #endif
      break;
    case SET_ADDR:
      eeSetAddress(rxData.toInt());
      #ifdef SERIAL_PRINT
        Serial.print(F("Set: "));
        Serial.println(eeGetAddress());
      #endif
      break;
  } // switch (rxCommand)
} // parseCommands()

// Requests

void parseRequests() {
  switch (rxCommand) {
    case GET_NAME:      setNotRequired(&sendName);     setPacketType(&sendName,     TYPE_ANSWER); sendPacket(DTA_NAME);      setPacketType(&sendName,     TYPE_DATA); break;
    #ifdef ID_FEATURE
      case GET_ID:      setNotRequired(&sendId);       setPacketType(&sendId,       TYPE_ANSWER); sendPacket(DTA_ID);        setPacketType(&sendId,       TYPE_DATA); break;
    #endif
    case GET_ADDR:      setNotRequired(&sendAddress);  setPacketType(&sendAddress,  TYPE_ANSWER); sendPacket(DTA_ADDR);      setPacketType(&sendAddress,  TYPE_DATA); break;    
    case GET_FREE_MEM:  setNotRequired(&sendFreeMem);  setPacketType(&sendFreeMem,  TYPE_ANSWER); sendPacket(DTA_FREE_MEM);  setPacketType(&sendFreeMem,  TYPE_DATA); break;
    case GET_BATTERY:   setNotRequired(&sendBattery);  setPacketType(&sendBattery,  TYPE_ANSWER); sendPacket(DTA_BATTERY);   setPacketType(&sendBattery,  TYPE_DATA); break;
    case GET_SELF_TEMP: setNotRequired(&sendSelfTemp); setPacketType(&sendSelfTemp, TYPE_ANSWER); sendPacket(DTA_SELF_TEMP); setPacketType(&sendSelfTemp, TYPE_DATA); break;
    case GET_HEALTH:    setNotRequired(&sendHealth);   setPacketType(&sendHealth,   TYPE_ANSWER); sendPacket(DTA_HEALTH);    setPacketType(&sendHealth,   TYPE_DATA); break;
    #ifdef TEMP_FEATURE
      case GET_TEMP_1:    setNotRequired(&sendTemp1);    setPacketType(&sendTemp1,    TYPE_ANSWER); temp1NeedSend = NEED_SEND_TEMP; sendPacket(DTA_TEMP_1); setPacketType(&sendTemp1,    TYPE_DATA); break;
    #endif

  }
}

// Data

void parseData() {

}

// Alarms

void parseAlarms() {

}

#endif // NRF24_FEATURE

