//**************************************************************************************************
// Filename                     main.h
// Platform                     STM32F10x
// Author                       Aveal (http://microtechnics.ru/)
// Current version              1.0.0
// Date                         11.01.2015
//**************************************************************************************************

#ifndef MAIN_H
#define MAIN_H



//**************************************************************************************************
// Includes
//**************************************************************************************************

#include "stm32f10x_conf.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "sdcard.h"
#include "stm32f10x_flash.h"
#include "ff.h"



//**************************************************************************************************
// Declarations and definitions
//**************************************************************************************************

// Bootloader key configuration
#define BOOTLOADER_KEY_START_ADDRESS            (uint32_t)0x08009800
#define BOOTLOADER_KEY_PAGE_NUMBER              20
#define BOOTLOADER_KEY_VALUE                    0xAAAA5555

// Flash configuration
#define MAIN_PROGRAM_START_ADDRESS              (uint32_t)0x0800A000
#define MAIN_PROGRAM_PAGE_NUMBER                21
#define NUM_OF_PAGES                            256
#define FLASH_PAGE_SIZE                         2048



#endif // #ifndef MAIN_H

//****************************************** end of file *******************************************
