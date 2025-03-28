/*
  Arduino Mega Server for ESP32 SD
  version 0.17
  2017, Hi-Lab.ru
  
  License: Free for personal use and education, without any warranties
  Home:    http://hi-lab.ru/arduino-mega-server (Russian)
           http://hi-lab.ru/english/arduino-mega-server (English)
  E-mail:  info@hi-lab.ru

  AMS Pro: http://hi-lab.ru/arduino-mega-server/ams-pro (projects and commercial use)
  
  IDE:      Arduino 1.6.5 r2
  Hardware: ESP32 with SD card

  Connections:
  ------------
  SD Card | ESP32
    D3       SS
    CMD      MOSI
    VSS      GND
    VDD      3.3V
    CLK      SCK
    VSS      GND
    D0       MISO

  Optional:
  ---------
  LED (yellow) - D4
  LED (red)    - D21
  LED (green)  - D22
  RGB LED      - D32, D27, D33

  Pathes of the Project:
  ---------------------
  Arduino IDE settings:
  ...\Documents\Arduino

  Libraries folder:
  ...\Documents\Arduino\libraries\

  Sketches folders:
  ...\Sketches\esp32sd\Arduino\arduino_mega_server_esp32sd\
  ...\Sketches\esp32sd\arduino_serial_commander\
  
  Settings:
  ---------
  In Sketch (Wi-Fi): SSID, PASSWORD of your Wi-Fi network
  In Browser:        IP address 192.168.1.70

  Quick start:
  ------------
  0. Connect ESP32 module and microSD card
  1. Install Arduino IDE 1.6.5 (r2)
  2. Install ESP32 drivers (https://github.com/espressif/arduino-esp32)
  3. Put files from folder "sd" to microSD card
  4. Print SSID and PASSWORD of your Wi-Fi router in tab "Wi-Fi" of sketch
  5. Upload Sketch "arduino_mega_server_esp32sd" to module ESP32
  6. Open in your browser address "192.168.1.70"
  7. Enjoy and donate on page http://hi-lab.ru/arduino-mega-server/details/donate
*/

// modules
#define FEATURE_UPLOAD
#define FEATURE_SD
#define FEATURE_TIME
#define FEATURE_NTP
#define FEATURE_SEND
//#define FEATURE_MAJOR
#define FEATURE_CONTACTS
#define FEATURE_PIRS
#define FEATURE_KEYS
#define FEATURE_LEDS
//#define FEATURE_TEMP

// debug
//#define ELECTRO_DEBUG
//#define EVENTS_CONTACTS
//#define EVENTS_PIRS
//#define TEMP_DEBUG
#define SERIAL_PRINT

#include "SPI.h"
#include "FS.h"
#include "SD.h"
#include <WiFi.h>
#include <TimeLib.h>

#define SELF_NAME F("ESP32 SD")
byte SELF_IP[] = {192, 168, 1, 70};

// Authorization
#define AUTH_OFF 0
#define AUTH_ON  1
byte authMode = AUTH_OFF;
// online encode: base64encode.org
String AUTH_HASH = "Authorization: Basic YWRtaW46YW1z"; // admin:ams

// time provider
#define TIME_NONE     0
#define TIME_NETWORK  1
#define TIME_RTC      2
byte timeProvider = TIME_NETWORK;

// mode work
#define MODE_SERVER   1
#define MODE_UPDATE   2
byte modeWork = MODE_SERVER;

// modules
#define DISABLE       0
#define ENABLE        1
#define NOT_COMPILLED 2
byte moduleUpload   = NOT_COMPILLED;
byte moduleTime     = NOT_COMPILLED;
byte moduleNtp      = NOT_COMPILLED;
byte moduleEthernet = NOT_COMPILLED;
byte moduleSd       = NOT_COMPILLED;
byte moduleServer   = NOT_COMPILLED;
byte moduleSend     = NOT_COMPILLED;
byte moduleMajor    = NOT_COMPILLED;
byte modulePing     = NOT_COMPILLED;
byte modulePirs     = NOT_COMPILLED;
byte moduleContacts = NOT_COMPILLED;
byte moduleTemp     = NOT_COMPILLED;
byte moduleLeds     = NOT_COMPILLED; 
byte moduleKeys     = NOT_COMPILLED;
byte moduleElectro  = NOT_COMPILLED;
byte moduleNoo      = NOT_COMPILLED;
byte moduleMr1132   = NOT_COMPILLED;
byte moduleNrf24    = NOT_COMPILLED;

// timers
boolean cycle1s  = false;
boolean cycle5s  = false;
boolean cycle10s = false;
boolean cycle20s = false;
boolean cycle30s = false;
boolean cycle1m  = false;
boolean cycle3m  = false;
boolean cycle5m  = false;

// strings
char buf[200];

#ifdef FEATURE_LEDS
  #define LED_EMPTY 0
  #define LED_PIR 1
  byte modeLed = LED_EMPTY;
#endif

void install() {

}

void setup() {
  initSerial();
  printWelcome();
  initRandom();
  install();
  initEeprom();
  initAbstract();
  initHardware();
  initTimers();
  initWifi();
  initSd();
  initNtp();
  initTime();
  initServer();

  #ifdef FEATURE_UPLOAD
    initUpload();
  #endif
  #ifdef FEATURE_SEND
    initSend();
  #endif
  #ifdef FEATURE_MAJOR
    initMajordomo();
  #endif
  #ifdef FEATURE_TEMP
    initTemp();
  #endif
  #ifdef FEATURE_CONTACTS
    initContacts();
  #endif
  #ifdef FEATURE_PIRS
    initPirs();
  #endif
  #ifdef FEATURE_LEDS
    initLeds();
  #endif
  #ifdef FEATURE_KEYS
    initKeys();
  #endif
  
  printInitDone();
} // setup

void loop() {
  #ifdef FEATURE_UPLOAD
    workUpload();
  #endif

  if (modeWork == MODE_SERVER) {
    workTimers();
    workTime();
    workServer();
  
    #ifdef FEATURE_TEMP
      workTemp();
    #endif
    #ifdef FEATURE_CONTACTS
      workContacts();
    #endif
    #ifdef FEATURE_PIRS
      workPirs();
    #endif
    #ifdef FEATURE_KEYS
      workKeys();
    #endif

    workAbstract();
    workLoopEnd();
  } // MODE_SERVER
} // loop

