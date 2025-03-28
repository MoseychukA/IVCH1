/*
  Modul nRF24
  AMS nRF24 controller
  part of Arduino Mega Server project
*/

#ifdef NRF24_FEATURE

#include <SPI.h>
#include "RF24.h"

// nRF24L01 modul configuration
RF24 radio(7, 8); // CE, CSN pins
byte addresses[][6] = {"1Node", "2Node"};

// packet buffers
byte const PACKET_MAX_BYTES = 32;
byte buffTx[PACKET_MAX_BYTES];
byte buffRx[PACKET_MAX_BYTES];

// packet structure
byte const POS_TYPE      = 0;
byte const POS_COMMAND   = 1;
byte const POS_ADDRESS   = 2;
byte const POS_DEST      = 3;
byte const POS_REQUIRED  = 4;
byte const POS_NUMBER    = 5;
byte const POS_PASSWORD  = 9;
byte const POS_DATA      = 17;

// packet type
byte const TYPE_EMPTY   = 0;
byte const TYPE_COMMAND = 1;
byte const TYPE_REQUEST = 2;
byte const TYPE_DATA    = 3;
byte const TYPE_ALARM   = 4;
byte const TYPE_ANSWER  = 5;
byte const TYPE_ECHO    = 6;

// packet variables
byte   rxType;
byte   rxCommand;
byte   rxAddress;
byte   rxDestination;
byte   rxRequired;
byte   rxNumber;
String rxPassword;
String rxData;

// required answer
byte const NOT_REQUIRED = 0;
byte const REQUIRED     = 1;

// logic
byte const NOT_DEF = 0;
byte const OUT     = 0;
byte const IN      = 1;

// nRF24 init

void nrf24Init() {
  radio.begin();
  radio.setPALevel(RF24_PA_LOW); // default RF24_PA_MAX
  
  // Open pipes
  radio.openWritingPipe(   addresses[1]);
  radio.openReadingPipe(1, addresses[0]);
  radio.startListening();
}

#ifdef SERIAL_PRINT
  void printBuff(byte buff[]) {
    for (byte i = 0; i < POS_PASSWORD; i++) {
      if      (buff[i] < 10)                 {Serial.print(F("   "));}
      else if (buff[i] > 9 && buff[i] < 100) {Serial.print(F("  "));}
      else if (buff[i] > 99)                 {Serial.print(F(" "));}
      Serial.print(buff[i]);
    }
    Serial.print(F(" "));
    for (byte i = POS_PASSWORD; i < POS_DATA; i++) {
      if (buff[i] == 0) {break;}
      Serial.print(char(buff[i]));
    }
    Serial.print(F(" "));
    for (byte i = POS_DATA; i < PACKET_MAX_BYTES; i++) {
      if (buff[i] == 0) {break;}
      Serial.print(char(buff[i]));
    }
  } // printBuff( )
#endif // SERIAL_PRINT

// set packet parameters

void setNotRequired(struct Sender *p) {
  p->required = NOT_REQUIRED;
}

void setPacketType(struct Sender *p, byte type) {
  p->type = type;
}

void setNextNumber(struct Sender *p) {
  p->number++;
  if (p->number > 255) {
    p->number = 0;
  }
}

boolean success(struct Sender *p) {
  return p->success;
}

// parse buffer

void parseBuffRx() {
  rxType        = buffRx[POS_TYPE];
  rxCommand     = buffRx[POS_COMMAND];
  rxAddress     = buffRx[POS_ADDRESS];
  rxDestination = buffRx[POS_DEST];
  rxRequired    = buffRx[POS_REQUIRED];
  rxNumber      = buffRx[POS_NUMBER];

  rxPassword = "";
  for (byte i = POS_PASSWORD; i < POS_DATA; i++) {
    if (buffRx[i] == 0) {break;}
    rxPassword += char(buffRx[i]);
  }
  rxData = "";
  for (byte i = POS_DATA; i < PACKET_MAX_BYTES; i++) {
    if (buffRx[i] == 0) {break;}
    rxData += char(buffRx[i]);
  }
}

#ifdef SERIAL_PRINT
  void printPacketType(byte type, byte dir) {
    String arrow;
    if (dir == IN) {arrow = F("<-");}
              else {arrow = F("->");}
  
    switch (type) {
      case TYPE_EMPTY:   Serial.print(F("Empty:   ")); Serial.print(arrow); break;
      case TYPE_COMMAND: Serial.print(F("Command: ")); Serial.print(arrow); break;
      case TYPE_REQUEST: Serial.print(F("Request: ")); Serial.print(arrow); break;
      case TYPE_DATA:    Serial.print(F("Data:    ")); Serial.print(arrow); break;
      case TYPE_ALARM:   Serial.print(F("Alarm:   ")); Serial.print(arrow); break;
      case TYPE_ANSWER:  Serial.print(F("Answer:  ")); Serial.print(arrow); break;
      case TYPE_ECHO:    Serial.print(F("Echo:    ")); Serial.print(arrow); break;
      default: Serial.print(F("Unknown: "));
    }
  }
#endif

void nrf24Works() {
  if (radio.available()) {

    while (radio.available()) {
      radio.read(&buffRx, PACKET_MAX_BYTES);
    }
    
    receivePackets++;
    parseBuffRx();

    #ifdef SERIAL_PRINT
      printPacketType(rxType, IN);
      printBuff(buffRx);
      Serial.println();
    #endif

    switch (rxType) {
      case TYPE_COMMAND: parseCommands(); break;
      case TYPE_DATA:    parseData();     break;
      case TYPE_ECHO:    parseEcho();     break;
      case TYPE_ANSWER:  parseAnswers();  break;
      case TYPE_REQUEST: parseRequests(); break;
      case TYPE_ALARM:   parseAlarms();   break;
      default: ;
    }
  } // if (radio.available())
  
  retrysControl();
} // nrf24Works()

#endif // NRF24_FEATURE

