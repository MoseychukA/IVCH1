/*
  AMS nRF24 controller
  version 0.17
  part of Arduino Mega Server project
  ©Hi-Lab.ru, 2018
  
  License: Free for personal use, "as is" and without any warranties
  Home:    http://hi-lab.ru/arduino-mega-server (russian)
           http://hi-lab.ru/english/arduino-mega-server (english)
  Email:   info@hi-lab.ru

  For commercial use version AMS Pro:
  https://hi-lab.ru/arduino-mega-server/ams-pro
  
  IDE: Arduino 1.8.5
  
  Hardware
  --------
  1. Arduino Pro Mini (ATmega328 32K, 3.3V, 8MHz)
  2. Module nRF24L01
  3. USB-UART adapter (for programming)

  Quick start:
  ------------
  1. Connect Arduino Pro Mini with nRF24L01 and USB-UART adapter
  2. Upload sketch to Arduino
  3. Run Arduino Mega Server
  4. Enjoy and donate on page http://hi-lab.ru/arduino-mega-server/details/donate
*/

#define NRF24_FEATURE
#define ID_FEATURE
#define TEMP_FEATURE
//#define CONTACTS_FEATURE

#define SERIAL_PRINT
//#define SIMULATION

// hardware
int const MEMORY = 2048;

// config
//String const NAME_DEFAULT = "AMS controller";
char const NAME_DEFAULT[]   = "AMS controller";

// timers
boolean timer_1s  = false;
boolean timer_5s  = false;
boolean timer_20s = false;
boolean timer_30s = false;
boolean timer_1m  = false;
boolean timer_3m  = false;
boolean timer_5m  = false;

// fuzzy timers
boolean fuzzy_10s = false;
boolean fuzzy_20s = false;
boolean fuzzy_30s = false;
boolean fuzzy_40s = false;

// data timers
boolean timer_temp1 = false;
unsigned long temp1_period = 300;

#ifdef NRF24_FEATURE
  // commands
  byte const GET_NAME       = 1;
  byte const DTA_NAME       = 2;
  byte const SET_NAME       = 3;
  byte const GET_ADDR       = 4;
  byte const DTA_ADDR       = 5;
  byte const SET_ADDR       = 6;
  byte const GET_FREE_MEM   = 7;
  byte const DTA_FREE_MEM   = 8;
  byte const GET_BATTERY    = 9;
  byte const DTA_BATTERY    = 10;
  byte const GET_SELF_TEMP  = 11;
  byte const DTA_SELF_TEMP  = 12;
  byte const GET_TEMP_1     = 13;
  byte const DTA_TEMP_1     = 14;
  byte const GET_HEALTH     = 15;
  byte const DTA_HEALTH     = 16;

  #ifdef ID_FEATURE
    byte const GET_ID       = 100;
    byte const DTA_ID       = 101;
    byte const SET_ID       = 102;
    byte const GEN_ID       = 103;
  #endif

  byte const BAK_ECHO       = 200;
  
  // sender structure
  typedef struct Sender {
    boolean active      = false;
    boolean required    = true;
    boolean success     = false;
    byte type           = 0;
    byte command        = 0;
    byte number         = 0;
    byte tryouts        = 0;
    int errors          = 0;
    String result       = "";
    unsigned long timer = millis();
  } sender;

  sender sendEcho,
    sendName, sendId, sendAddress,
    sendFreeMem, sendBattery, sendSelfTemp,
    sendTemp1,
    sendHealth;
#endif // NRF24_FEATURE

#ifdef TEMP_FEATURE
  // temperature
  byte const NO_NEED_SEND          = 0;
  byte const NEED_SEND_TEMP        = 1;
  byte const NEED_SEND_ERROR       = 2;
  byte const NEED_SEND_REMOVED     = 3;
  byte const NEED_SEND_NOT_PRESENT = 4;
  byte const NEED_SEND_ALARM_UP    = 5;
  byte const NEED_SEND_ALARM_DOWN  = 6;
  byte temp1NeedSend = NO_NEED_SEND;

  // temperature
  int const TEMP_NOT_DEFINED =  -99;
  int const TEMP_NOT_PRESENT =  -98;
  int const TEMP_REMOVED     = -127;
  int const TEMP_ERROR       =   85;
  float temp1     = TEMP_NOT_DEFINED;
  float temp1Back = TEMP_NOT_DEFINED;
#endif


#ifdef NRF24_FEATURE
 // net stat
 int sendPackets    = 0;
 int sendRetrys     = 0;
 int sendErrors     = 0;
 int receivePackets = 0;
 int successPackets = 0;
 int sendPacketsStat    = 0;
 int sendRetrysStat     = 0;
 int sendErrorsStat     = 0;
 int receivePacketsStat = 0;
 int successPacketsStat = 0;
#endif

/* setup
------------------------------------ */

void setup() {
  Serial.begin(9600);
  #ifdef SERIAL_PRINT
    Serial.println();
    Serial.print(NAME_DEFAULT);
    Serial.println(F(" START"));
    #ifdef SIMULATION
      Serial.println(F("MODE: SIMULATION"));
    #endif
  #endif

  randomInit();
  timersInit();
  eepromInit();
  
  //eeSetName(NAME_DEFAULT);
  #ifdef SERIAL_PRINT
    printEeInfo();
  #endif

  Serial.print(F("Free mem: "));
  Serial.println(freeMemP());

  #ifdef TEMP_FEATURE
    tempInit();
    tempWorks();
  #endif

  #ifdef CONTACTS_FEATURE
    contactsInit();
  #endif
  
  #ifdef NRF24_FEATURE
    nrf24Init();
  #endif

  #ifdef SERIAL_PRINT
    Serial.print(NAME_DEFAULT);
    Serial.println(F(" Init DONE"));
    Serial.println();
    Serial.print(NAME_DEFAULT);
    Serial.println(F(" WORK"));
  #endif
} // setup()

/* loop
------------------------------------ */

void loop() {
  timersWorks();
  
  #ifdef TEMP_FEATURE
    tempWorks();
  #endif

  #ifdef CONTACTS_FEATURE
    contactsWorks();
  #endif

  #ifdef SIMULATION
    simulationWorks();
  #endif

  #ifdef NRF24_FEATURE
    if (timer_5s) {
      checkState();
    }
    if (timer_1m) {
      sendData();
    }
    if (timer_3m) {
      resetNetStat();
      printNetStat();
    }
    nrf24Works();
  #endif
  
  print1s();
  resetTimers();
} // loop()

