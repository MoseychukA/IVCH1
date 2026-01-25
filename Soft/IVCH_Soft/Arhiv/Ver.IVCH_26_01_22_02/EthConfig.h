#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <IPAddress.h> // <-- ÄÎÁÀÂÈÒÜ (äëÿ IPAddress)

// --- Debug ---
#define SerialDebug Serial
#define DEBUG_ETHERNET_GENERIC_PORT SerialDebug
#define _ETG_LOGLEVEL_ 2

// --- W5500 pins (SPI1) ---
#define USE_THIS_SS_PIN PA4

// Custom SPI pins for STM32F1
#define CUR_PIN_MISO PA6
#define CUR_PIN_MOSI PA7
#define CUR_PIN_SCK PA5
#define CUR_PIN_SS PA4

// Ýêçåìïëÿð SPI äëÿ Ethernet_Generic (ÎÁÚßÂËÅÍÈÅ)
extern SPIClass SPI_New;

// MAC pool (ÎÁÚßÂËÅÍÈß)
extern const uint16_t NUMBER_OF_MAC;
extern byte mac[][6];

// Default static IP options (ÎÁÚßÂËÅÍÈß)
extern IPAddress ip;
extern IPAddress myDns;