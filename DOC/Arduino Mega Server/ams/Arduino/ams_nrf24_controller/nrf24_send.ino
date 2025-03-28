/*
  Modul nRF24 Send
  AMS nRF24 controller
  part of Arduino Mega Server project
*/

#ifdef NRF24_FEATURE

// Make packet

void clearTxRegion(int start, int finish) {
  for (int i = start; i < finish; i++) {
    buffTx[i] = 0;
  }
}

void setTxRegion(int start, String str) {
  for (byte i = 0; i < str.length(); i++) {
    buffTx[start + i] = str[i];
  }
}

void makePacket(struct Sender *p, byte tpe, byte cmd, byte dest, String pwd, String dta) {
  // sender
  if (p->required) {p->active = true;  p->success = false;}
              else {p->active = false; p->success = true;}

  p->command = cmd;
  p->timer   = millis();
  
  byte type;
  if (p->type) {type = p->type;}
          else {type = tpe;}
  
  // packet
  buffTx[POS_TYPE]     = type;
  buffTx[POS_COMMAND]  = cmd;
  buffTx[POS_ADDRESS]  = eeAddress;
  buffTx[POS_DEST]     = dest;
  buffTx[POS_REQUIRED] = p->required;
  buffTx[POS_NUMBER]   = p->number;

  clearTxRegion(POS_PASSWORD, PACKET_MAX_BYTES);
  setTxRegion(POS_PASSWORD, pwd);
  setTxRegion(POS_DATA,     dta);

  p->required = true;
}

// Make packets

void makePacketName()     {makePacket(&sendName,     TYPE_DATA, DTA_NAME,      255, "", stringName());}
void makePacketAddress()  {makePacket(&sendAddress,  TYPE_DATA, DTA_ADDR,      255, "", String(eeAddress));}
void makePacketFreeMem()  {makePacket(&sendFreeMem,  TYPE_DATA, DTA_FREE_MEM,  255, "", String(freeMemP()));}
void makePacketBattery()  {makePacket(&sendBattery,  TYPE_DATA, DTA_BATTERY,   255, "", String(battery()));}
void makePacketSelfTemp() {makePacket(&sendSelfTemp, TYPE_DATA, DTA_SELF_TEMP, 255, "", String(getSelfTemp(), 1));}
void makePacketHealth()   {makePacket(&sendHealth,   TYPE_DATA, DTA_HEALTH,    255, "", String(healthController()));}

#ifdef ID_FEATURE
  void makePacketId()     {makePacket(&sendId,       TYPE_DATA, DTA_ID,        255, "", stringId());}
#endif

#ifdef TEMP_FEATURE
  void makePacketTemp() {
    String data = String(temp1, 1);
    switch (temp1NeedSend) {
      case NEED_SEND_TEMP:        makePacket(&sendTemp1, TYPE_DATA,  DTA_TEMP_1, 255, "", data);       break;
      case NEED_SEND_ERROR:       makePacket(&sendTemp1, TYPE_ALARM, DTA_TEMP_1, 255, "", "err");      break;
      case NEED_SEND_REMOVED:     makePacket(&sendTemp1, TYPE_ALARM, DTA_TEMP_1, 255, "", "rem");      break;
      case NEED_SEND_NOT_PRESENT: makePacket(&sendTemp1, TYPE_ALARM, DTA_TEMP_1, 255, "", "not pres"); break;
      case NEED_SEND_ALARM_UP:    makePacket(&sendTemp1, TYPE_ALARM, DTA_TEMP_1, 255, "", data);       break;
      case NEED_SEND_ALARM_DOWN:  makePacket(&sendTemp1, TYPE_ALARM, DTA_TEMP_1, 255, "", data);       break;
    }
    temp1NeedSend = NO_NEED_SEND;
  }
#endif

// Wrappers

void sendMemoryAlarm()   {setPacketType(&sendFreeMem, TYPE_ALARM); sendPacket(DTA_FREE_MEM);}
void sendBatteryAlarm()  {setPacketType(&sendBattery, TYPE_ALARM); sendPacket(DTA_BATTERY);}
void sendSelfTempAlarm() {setPacketType(&sendSelfTemp,TYPE_ALARM); sendPacket(DTA_SELF_TEMP);}

// Send packet

void sendPacket(byte packet) {
  radio.stopListening();

  switch (packet) {
    case DTA_NAME:       makePacketName();     break;
    case DTA_ADDR:       makePacketAddress();  break;
    case DTA_FREE_MEM:   makePacketFreeMem();  break;
    case DTA_BATTERY:    makePacketBattery();  break;
    case DTA_HEALTH:     makePacketHealth();   break;
    case DTA_SELF_TEMP:  makePacketSelfTemp(); break;
    #ifdef ID_FEATURE
      case DTA_ID:       makePacketId();       break;
    #endif
    #ifdef TEMP_FEATURE
      case DTA_TEMP_1:   makePacketTemp();     break;
    #endif
    
  } // switch (packet)

  #ifdef SERIAL_PRINT
    printPacketType(buffTx[POS_TYPE], OUT);
    printBuff(buffTx);
    Serial.println();
  #endif

  sendPackets++;

  if (!radio.write(&buffTx, PACKET_MAX_BYTES)) {
    #ifdef SERIAL_PRINT
      Serial.println(F("SEND FAILED"));
      sendErrors++;
    #endif
  }
  radio.startListening();
} // sendPacket( )

// Check retrys

void checkRetrys(struct Sender *p, byte command) {
  byte const MAX_TRYOUTS = 2;
  int const TRYOUT_DELAY = 3000;

  if (p->active) {
    if (millis() - p->timer > TRYOUT_DELAY) {
      sendRetrys++;
      p->tryouts++;
      p->timer = millis();
      #ifdef SERIAL_PRINT
        Serial.print(F("TRY: "));
        Serial.println(p->tryouts);
      #endif
      sendPacket(command);
      if (p->tryouts > MAX_TRYOUTS) {     
        sendErrors++;
        p->errors++;
        resetSender(p);
        p->success = false; // ! after resetSender(p)
        #ifdef SERIAL_PRINT
          Serial.print(F("SEND "));
          Serial.print(p->command);
          Serial.println(F(" ERROR"));
        #endif
      }
    }
  }
} // checkRetrys()

void retrysControl() {
  checkRetrys(&sendAddress,  DTA_ADDR);
  checkRetrys(&sendBattery,  DTA_BATTERY);
  checkRetrys(&sendFreeMem,  DTA_FREE_MEM);
  checkRetrys(&sendSelfTemp, DTA_SELF_TEMP);
  #ifdef TEMP_FEATURE
    checkRetrys(&sendTemp1,  DTA_TEMP_1);
  #endif
}

#endif // NRF24_FEATURE

