/*
  Module nRF24 Send
  part of Arduino Mega Server project
*/

#ifdef NRF24_FEATURE

// temperature
#define NO_NEED_SEND          0
#define NEED_SEND_TEMP        1
#define NEED_SEND_ERROR       2
#define NEED_SEND_REMOVED     3
#define NEED_SEND_NOT_PRESENT 4
#define NEED_SEND_ALARM_UP    5
#define NEED_SEND_ALARM_DOWN  6
byte temp1NeedSend = NO_NEED_SEND;

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

void makePacket(struct packet *p, byte tpe, byte cmd, byte dest, String pwd, String dta) {
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
  //////////////buffTx[POS_ADDRESS]  = eeAddress;
  buffTx[POS_DEST]     = dest;
  buffTx[POS_REQUIRED] = p->required;
  buffTx[POS_NUMBER]   = p->number;

  clearTxRegion(POS_PASSWORD, PACKET_MAX_BYTES);
  setTxRegion(POS_PASSWORD, pwd);
  setTxRegion(POS_DATA,     dta);
}

// Make packets

//void makePacketEcho()        {makePacket(&sendEcho,        TYPE_ECHO,    buffTx[POS_COMMAND], 255, "", "");}

void makePacketGetName()     {makePacket(&pktGetName,     TYPE_REQUEST, GET_NAME,      255, "", "");}
void makePacketGetAddress()  {makePacket(&pktGetAddress,  TYPE_REQUEST, GET_ADDR,      255, "", "");}
void makePacketGetFreeMem()  {makePacket(&pktGetFreeMem,  TYPE_REQUEST, GET_FREE_MEM,  255, "", "");}
void makePacketGetBattery()  {makePacket(&pktGetBattery,  TYPE_REQUEST, GET_BATTERY,   255, "", "");}
void makePacketGetSelfTemp() {makePacket(&pktGetSelfTemp, TYPE_REQUEST, GET_SELF_TEMP, 255, "", "");}
void makePacketGetHealth()   {makePacket(&pktGetHealth,   TYPE_REQUEST, GET_HEALTH,    255, "", "");}
void makePacketGetId()       {makePacket(&pktGetId,       TYPE_REQUEST, GET_ID,        255, "", "");}
void makePacketGetTemp1()    {makePacket(&pktGetTemp1,    TYPE_REQUEST, GET_TEMP_1,    255, "", "");}

// Wrappers

void sendMemoryAlarm()   {setPacketType(&pktGetFreeMem, TYPE_ALARM); sendPacket(DTA_FREE_MEM);}
void sendBatteryAlarm()  {setPacketType(&pktGetBattery, TYPE_ALARM); sendPacket(DTA_BATTERY);}
void sendSelfTempAlarm() {setPacketType(&pktGetSelfTemp,TYPE_ALARM); sendPacket(DTA_SELF_TEMP);}

// Send packet

void sendPacket(byte packet) {
  radio.stopListening();

  switch (packet) {
    case BAK_ECHO:       //makePacketEcho();
         break;
    case GET_NAME:       makePacketGetName();     break;
    case GET_ADDR:       makePacketGetAddress();  break;
    case GET_FREE_MEM:   makePacketGetFreeMem();  break;
    case GET_BATTERY:    makePacketGetBattery();  break;
    case GET_SELF_TEMP:  makePacketGetSelfTemp(); break;
    case GET_HEALTH:     makePacketGetHealth();   break;
    case GET_ID:         makePacketGetId();       break;
    case GET_TEMP_1:     makePacketGetTemp1();    break;
  } // switch (packet)

  printPacketType(buffTx[POS_TYPE], OUT);
  printBuff(buffTx);
  Serial.println();

  sendPackets++;

  if (!radio.write(&buffTx, PACKET_MAX_BYTES)) {
    Serial.println(F("SEND FAILED"));
    sendErrors++;
  }
  radio.startListening();
} // sendPacket( )

// Check retrys

void checkRetrys(struct packet *p, byte command) {
  #define MAX_TRYOUTS  2
  #define TRYOUT_DELAY 3000

  if (p->active) {
    if (millis() - p->timer > TRYOUT_DELAY) {
      sendRetrys++;
      p->tryouts++;
      p->timer = millis();
      timeStamp(); Serial.print(F("TRY: ")); Serial.println(p->tryouts);
      sendPacket(command);
      if (p->tryouts > MAX_TRYOUTS) {
        sendErrors++;
        p->errors++;
        resetSender(p);
        p->success = false; // ! after resetSender(p)
        timeStamp(); Serial.print(F("SEND "));
        Serial.print(p->command);
        Serial.println(F(" ERROR"));
      }
    }
  }
} // checkRetrys()

void retrysControl() {
  checkRetrys(&pktGetName,     GET_NAME);
  checkRetrys(&pktGetId,       GET_ID);
  checkRetrys(&pktGetAddress,  GET_ADDR);
  checkRetrys(&pktGetFreeMem,  GET_FREE_MEM);
  checkRetrys(&pktGetBattery,  GET_BATTERY);
  checkRetrys(&pktGetSelfTemp, GET_SELF_TEMP);
  checkRetrys(&pktGetHealth,   GET_HEALTH);
  #ifdef DS18B20_FEATURE
    checkRetrys(&pktGetTemp1,  GET_TEMP_1);
  #endif
}

#endif // NRF24_FEATURE
