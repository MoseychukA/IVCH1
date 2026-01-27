#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <IPAddress.h>

// –азрешаем сборку только на STM32 (чтобы случайно не собрать на другой архитектуре)
#if !defined(ARDUINO_ARCH_STM32)
#error "EthConfig.h:this configuration is intended for STM32 Arduino core (ARDUINO_ARCH_STM32)."
#endif

// --- Debug ---
#define SerialDebug Serial
#define DEBUG_ETHERNET_GENERIC_PORT SerialDebug
#define _ETG_LOGLEVEL_ 2

// --- W5500 pins (SPI1 default pins on STM32F103) ---
// SPI1:SCK=PA5,MISO=PA6,MOSI=PA7
// CS (SS) = PA4 (проверено вашим тестом)
#define USE_THIS_SS_PIN PA4

// Custom SPI pins for STM32F1 (дл€ вашего SPI_New)
#define CUR_PIN_MISO PA6
#define CUR_PIN_MOSI PA7
#define CUR_PIN_SCK PA5
#define CUR_PIN_SS PA4

// «ащита:если в какой-то версии/настройке Ethernet_Generic попытаетс€
// создать SPI_New внутри w5100_Impl.h,мы запрещаем это и предоставл€ем
// наш экземпл€р из EthConfig.cpp.
#ifndef SPI_NEW_INITIALIZED
#define SPI_NEW_INITIALIZED 1
#endif

// Ёкземпл€р SPI дл€ Ethernet_Generic (ќЅЏя¬Ћ≈Ќ»≈)
extern SPIClass SPI_New;

// MAC pool (ќЅЏя¬Ћ≈Ќ»я)
extern const uint16_t NUMBER_OF_MAC;
extern byte mac[][6];

// Default static IP options (ќЅЏя¬Ћ≈Ќ»я)
extern IPAddress ip;
extern IPAddress myDns;





//#pragma once
//#include <Arduino.h>
//#include <SPI.h>
//#include <IPAddress.h> // <-- ƒќЅј¬»“№ (дл€ IPAddress)
//
//// --- Debug ---
//#define SerialDebug Serial
//#define DEBUG_ETHERNET_GENERIC_PORT SerialDebug
//#define _ETG_LOGLEVEL_ 2
//
//// --- W5500 pins (SPI1) ---
//#define USE_THIS_SS_PIN PA4
//
//// Custom SPI pins for STM32F1
//#define CUR_PIN_MISO PA6
//#define CUR_PIN_MOSI PA7
//#define CUR_PIN_SCK PA5
//#define CUR_PIN_SS PA4
//
//// Ёкземпл€р SPI дл€ Ethernet_Generic (ќЅЏя¬Ћ≈Ќ»≈)
//extern SPIClass SPI_New;
//
//// MAC pool (ќЅЏя¬Ћ≈Ќ»я)
//extern const uint16_t NUMBER_OF_MAC;
//extern byte mac[][6];
//
//// Default static IP options (ќЅЏя¬Ћ≈Ќ»я)
//extern IPAddress ip;
//extern IPAddress myDns;