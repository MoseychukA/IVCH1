/*
  Arduino Mega Server
  version 0.17
  ©Hi-Lab.ru, 2018
  
  License: Free for personal use, "as is" and without any warranties
  Home:    https://hi-lab.ru/arduino-mega-server (Russian)
           https://hi-lab.ru/english/arduino-mega-server (English)
  Email:   info@hi-lab.ru

  For commercial use and projects version AMS Pro:
  https://hi-lab.ru/arduino-mega-server/ams-pro

  Hardware
  --------
  Arduino Mega 2560
  Ethernet Shield
  microSD card
  
  Software
  --------
  IDE: Arduino 1.8.5
    Processing 1.5.1

  Arduino IDE settings
  --------------------
  Path to sketches folder:
  ...\ams\Arduino

  Sketches
  --------  
  Arduino Mega Server:      arduino_mega_server.ino
  Arduino Serial Commander: arduino_serial_commander.pde
  
  Quick start
  -----------
  1. Put files from archive sd.zip to microSD card (to root)
  2. Load sketch "arduino_mega_server" to Arduino Mega
  3. Open on your browser address 192.168.1.37
  4. Enjoy and donate on page http://hi-lab.ru/arduino-mega-server/details/donate
*/

// Hardware
//#define ETHERNET_SHIELD_2

// Features
#define SD_INFO_FEATURE
//#define SD_FILES_FEATURE
#define NTP_FEATURE
#define UPLOAD_FEATURE
#define SEND_FEATURE
//#define MAJORDOMO_FEATURE
//#define LAURENT_FEATURE
#define PIRS_FEATURE
#define DS18B20_FEATURE
#define CONTACTS_FEATURE
//#define ELECTRO_FEATURE
#define KEYS_FEATURE
#define LEDS_FEATURE
//#define MT1132_FEATURE
//#define MR1132_FEATURE
//#define NRF24_FEATURE

// Libraries

#include <SPI.h>

#ifndef ETHERNET_SHIELD_2
  #include <Ethernet.h>
#else
  #include <Ethernet2.h>
#endif

#include <SD.h>
#include <Wire.h>
#include <TimeLib.h>
#include <DS1307RTC.h>

// Version
#define SELF_NAME F("Arduino Mega Server")
#define VERSION   F("0.17")

// Logic
#define OFF   0
#define ON    1
#define EMPTY 0

// Ethernet settings
byte SELF_MAC[] = {0x00, 0x2A, 0xF5, 0x12, 0x67, 0x37};
byte SELF_IP[]  = {192, 168,  1, 177};
//byte DNS[]    = {   ,    ,   ,   };
byte GATEWAY[]  = {192, 168,  1,  1};

// Authorization
bool authMode = OFF;
// Online encode to Base64: base64encode.org
#define AUTH_HASH F("Authorization: Basic YWRtaW46YW1z") // admin:ams

// Time
#define TIME_NONE    0
#define TIME_NETWORK 1
#define TIME_RTC     2
byte timeProvider = TIME_NETWORK;

// Sockets
#define FREE_SOCKETS 1

// Work mode
#define MODE_SERVER 1
#define MODE_UPDATE 2
byte modeWork = MODE_SERVER;

// Durations
time_t prevEventElectroCycle;
time_t prevEventMegaLive;
time_t prevEventRequest;

// Serial dynamic mode
bool serialDynamic = OFF;

// nRF24

#ifdef NRF24_FEATURE
  // Commands
  #define GET_NAME      1
  #define DTA_NAME      2
  #define SET_NAME      3
  #define GET_ADDR      4
  #define DTA_ADDR      5
  #define SET_ADDR      6
  #define GET_FREE_MEM  7
  #define DTA_FREE_MEM  8
  #define GET_BATTERY   9
  #define DTA_BATTERY   10
  #define GET_SELF_TEMP 11
  #define DTA_SELF_TEMP 12
  #define GET_TEMP_1    13
  #define DTA_TEMP_1    14
  #define GET_HEALTH    15
  #define DTA_HEALTH    16
  #define GET_ID        100
  #define DTA_ID        101
  #define SET_ID        102
  #define GEN_ID        103
  #define BAK_ECHO      200

  // nRF24 packets
  typedef struct packet {
    bool active         = false;
    bool required       = true;
    bool success        = false;
    byte type           = 0;
    byte command        = 0;
    byte number         = 0;
    byte tryouts        = 0;
    int errors          = 0;
    String result       = "";
    unsigned long timer = millis();
  } Packet;

  Packet pktEcho,
         pktGetName, pktGetId, pktGetAddress,
         pktGetFreeMem, pktGetBattery, pktGetSelfTemp,
         pktGetHealth, pktGetTemp1;

  // net stat
  int sendPackets        = 0;
  int sendRetrys         = 0;
  int sendErrors         = 0;
  int receivePackets     = 0;
  int successPackets     = 0;
  int sendPacketsStat    = 0;
  int sendRetrysStat     = 0;
  int sendErrorsStat     = 0;
  int receivePacketsStat = 0;
  int successPacketsStat = 0;
    
  // nRF24 controller
  String nrf1Name;
  String nrf1Id;
  byte   nrf1Addr;
  byte   nrf1Battery;
  byte   nrf1Health;
  int    nrf1FreeMem;
  float  nrf1SelfTemp;
  float  nrf1Temp1;
#endif // NRF24_FEATURE


// Modules
#define MODULE_DISABLE 0
#define MODULE_ENABLE  1
#define NOT_COMPILLED  2
byte moduleSdInfo   = NOT_COMPILLED;
byte moduleSdFiles  = NOT_COMPILLED;
byte moduleNtp      = NOT_COMPILLED;
byte moduleUpload   = NOT_COMPILLED;
byte moduleSend     = NOT_COMPILLED;
byte moduleMajor    = NOT_COMPILLED;
byte moduleLaurent  = NOT_COMPILLED;
byte moduleDs18b20  = NOT_COMPILLED;
byte modulePirs     = NOT_COMPILLED;
byte moduleContacts = NOT_COMPILLED;
byte modulePm       = NOT_COMPILLED;
byte moduleLeds     = NOT_COMPILLED; 
byte moduleKeys     = NOT_COMPILLED;
byte moduleMr1132   = NOT_COMPILLED;
byte moduleMt1132   = NOT_COMPILLED;
byte moduleNrf24    = NOT_COMPILLED;

// Init print timeStamp
bool printTimeStamp = false;

// buffer
char buf[200];

// for sample Ajax
bool LED_state[4] = {0};

// electro
bool oscill = false;

// Cyclos
bool cycle1s  = false;
bool cycle5s  = false;
bool cycle20s = false;
bool cycle30s = false;
bool cycle1m  = false;
bool cycle3m  = false;
bool cycle5m  = false;

#ifdef LEDS_FEATURE
  #define LED_PIR 1
  byte modeLED = EMPTY;
#endif

/* Setup
----------------------------------- */

void setup() {
  serialInit();
  printStart();

  hardwareInit();
  randomInit();
  eepromInit();

  ethernetInit();
  sdCardInit();
  
  #ifdef SD_INFO_FEATURE
    sdInfoInit();
  #endif
  #ifdef SD_FILES_FEATURE
    sdFilesInit();
  #endif
  #ifdef NTP_FEATURE
    ntpInit();
  #endif
  timeInit();
  printTimeStamp = true;
  serverInit();
  timersInit();
  #ifdef UPLOAD_FEATURE
    uploadInit();
  #endif
  #ifdef SEND_FEATURE
    sendInit();
  #endif
  #ifdef MAJORDOMO_FEATURE
    majorInit();
  #endif
  #ifdef LAURENT_FEATURE
    laurentInit();
  #endif
  #ifdef DS18B20_FEATURE
    tempInit();
  #endif
  #ifdef CONTACTS_FEATURE
    contactsInit();
  #endif
  #ifdef PIRS_FEATURE
    pirsInit();
  #endif
  #ifdef ELECTRO_FEATURE
    electroInit();
  #endif
  #ifdef KEYS_FEATURE
    keysInit();
  #endif
  #ifdef LEDS_FEATURE
    ledsInit();
  #endif
  #ifdef MT1132_FEATURE
    mt1132Init();
  #endif
  #ifdef MR1132_FEATURE
    mr1132Init();
  #endif
  #ifdef NRF24_FEATURE
    nrf24Init();
  #endif
  
  abstractInit();
  printInitDone();
} // setup()

/* Loop
----------------------------------- */

void loop() {
  #ifdef UPLOAD_FEATURE
    uploadWorks();
  #endif

  if (modeWork == MODE_SERVER) {
    profStart();
    timersWorks();
    timeWorks();

    serverWorks();

    #ifdef MAJORDOMO_FEATURE
      majorWorks();
    #endif
    #ifdef LAURENT_FEATURE
      laurentWorks();
    #endif
    #ifdef DS18B20_FEATURE
      tempWorks();
    #endif 
    #ifdef CONTACTS_FEATURE
      contactsWorks();
    #endif
    #ifdef PIRS_FEATURE
      pirsWorks();
    #endif
    #ifdef ELECTRO_FEATURE
      electroWorks();
    #endif
    #ifdef KEYS_FEATURE
      keysWorks();
    #endif
    #ifdef MR1132_FEATURE
      mr1132Work();
    #endif
    #ifdef NRF24_FEATURE
      nrf24Works();
    #endif

    abstractWorks();
    endLoopWorks();
  } // modeWork
} // loop()
