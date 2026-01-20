/*
 Программа модуля часов и измерения питающей сети.
 Дисплей ST7789 SPI 170x320
 Микроконтроллер STM32F103VGT6
 Среда программирования Arduino IDE
 Generic STM32F1 series
 Опции настройки компилятора в файле "Настройки компилятора.png"
*/
#include "Arduino.h"
#include "PCF8575_simple.h"
#include <Wire.h>
#include "RTCSupport.h"
#include <stdint.h>

#include <TFT_eSPI.h> // Graphics and font library for ST7735 driver chip
#include <SPI.h>
#include <SD.h>

#include "Menu.h"
#include "Buttons.h"

#define SW1_PIN PC3 // Вызов меню/выход
#define SW2_PIN PC2 // Навигация (выбор страницы или пункта) вниз
#define SW3_PIN PC1 // Навигация вверх
#define SW4_PIN PC0 // Фиксация

TFT_eSPI tft = TFT_eSPI();
Menu menu_start(&tft);

Button btnSW1(SW1_PIN);
Button btnSW2(SW2_PIN);
Button btnSW3(SW3_PIN);
Button btnSW4(SW4_PIN);

#define PCF_HOUR   0x21
#define PCF_MIN    0x22
#define PCF_SEC    0x23
#define BUTTON_PIN PC0    // КНОПКА для отображения частоты

PCF8575_simple pcfHour(PCF_HOUR);
PCF8575_simple pcfMin(PCF_MIN);
PCF8575_simple pcfSec(PCF_SEC);


const uint8_t segTable[10] = {
  0b00111111, //0
  0b00000110, //1
  0b01011011, //2
  0b01001111, //3
  0b01100110, //4
  0b01101101, //5
  0b01111101, //6
  0b00000111, //7
  0b01111111, //8
  0b01101111  //9
};


#define INPUT_PIN PA1

volatile uint32_t last_capture = 0;
volatile uint32_t diff_sum = 0;
volatile uint16_t diff_count = 0;
volatile bool skip = false;

float frequency = 0.0f;

// dtostrf замена
char* dtostrf(double val, signed char width, unsigned char prec, char* sout) {
    sprintf(sout, "%*.*f", width, prec, val);
    return sout;
}

// --- Делитель частоты на 2 ---
void freqInterrupt() {
    static uint32_t prev = 0;
    if (skip) { skip = false; return; }
    skip = true;
    uint32_t now = micros();
    if (prev > 0) {
        uint32_t diff = now - prev;
        diff_sum += diff;
        diff_count++;
    }
    prev = now;
}

void setPCF(PCF8575_simple& pcf, uint8_t first, uint8_t second) {
    uint16_t word = 0;
    word |= (first & 0xFF);
    word |= ((uint16_t)second) << 8;
    pcf.write16(word);
}


void displayFrequency(float freq)
{
    // Ограничение диапазона. Только 2 целых разряда!

    if (isnan(freq) || freq < 0 || freq > 99.9999) {
        setPCF(pcfHour, 0xFF, 0xFF);
        setPCF(pcfMin, 0xFF, 0xFF);
        setPCF(pcfSec, 0xFF, 0xFF);
        return;
    }


    if (freq > 99.9999) freq = 99.9999;
    if (freq < 0) freq = 0.0;

    // Разбиваем на целую и дробную часть
    uint8_t d1 = (uint8_t)(freq / 10);              // десятки Гц
    uint8_t d2 = (uint8_t)(fmod(freq, 10));         // единицы Гц
    uint16_t fract = (uint16_t)(fmod(freq, 1) * 10000); // 4 знака после точки

    // Дробная часть разряды:
    uint8_t f1 = (fract / 1000) % 10;
    uint8_t f2 = (fract / 100) % 10;
    uint8_t f3 = (fract / 10) % 10;
    uint8_t f4 = (fract / 1) % 10;

    uint8_t dig[6];
    dig[0] = ~segTable[d1];
    dig[1] = ~segTable[d2] & ~(0b10000000); // Включить точку
    dig[2] = ~segTable[f1];
    dig[3] = ~segTable[f2];
    dig[4] = ~segTable[f3];
    dig[5] = ~segTable[f4];

    setPCF(pcfHour, dig[0], dig[1]);
    setPCF(pcfMin, dig[2], dig[3]);
    setPCF(pcfSec, dig[4], dig[5]);
}


void displayTime(uint8_t hours, uint8_t minutes, uint8_t seconds) {
    setPCF(pcfHour, ~segTable[hours / 10], ~segTable[hours % 10]);
    setPCF(pcfMin, ~segTable[minutes / 10], ~segTable[minutes % 10]);
    setPCF(pcfSec, ~segTable[seconds / 10], ~segTable[seconds % 10]);
}

RealtimeClock _rtc;
RealtimeClock rtc = _rtc;


//void test_indicators() 
//{
//    // Для каждого индикатора (0..5) по очереди выводим счет от 0 до 9
//    for (int digitIndex = 0; digitIndex < 6; digitIndex++) 
//    {
//        for (int num = 0; num < 10; num++) 
//        {
//            uint8_t left = 0, right = 0;
//            if (digitIndex % 2 == 0) 
//            {
//                // первый индикатор (левый) у данного PCF
//                left = ~segTable[num];   // инверсия ― инверсное управление!
//                right = 0xFF;            // погашен (все сегменты отрицательного логического уровня)
//            }
//            else 
//            {
//                // второй индикатор (правый) у данного PCF
//                left = 0xFF;             // погашен
//                right = ~segTable[num];  // инверсия
//            }
//            switch (digitIndex / 2) 
//            {
//            case 0: pcfHour.write16(left | (right << 8)); break;
//            case 1: pcfMin.write16(left | (right << 8)); break;
//            case 2: pcfSec.write16(left | (right << 8)); break;
//            }
//            delay(250);
//        }
//        // По окончании цикла гасим оба разряда
//        switch (digitIndex / 2) 
//        {
//        case 0: pcfHour.write16(0xFFFF); break;
//        case 1: pcfMin.write16(0xFFFF); break;
//        case 2: pcfSec.write16(0xFFFF); break;
//        }
//        delay(500); // пауза между разрядами
//    }
//}
//

Sd2Card card;
SdVolume volume;
SdFile root;

const int chipSelect = PA8;


void setup() 
{
    Serial.begin(115200);
    Wire.begin();
    rtc.begin();
    pcfHour.begin();
    pcfMin.begin();
    pcfSec.begin();
    pinMode(INPUT_PIN, INPUT);
    attachInterrupt(INPUT_PIN, freqInterrupt, RISING);
   // test_indicators();
   // rtc.setTime(0, 59, 18, 6, 17, 1, 2026);
    String ver_soft = __FILE__;
    int val_srt = ver_soft.lastIndexOf('\\');
    ver_soft.remove(0, val_srt + 1);
    val_srt = ver_soft.lastIndexOf('.');
    ver_soft.remove(val_srt);
    Serial.println(ver_soft);
   // TFTScreen->setVer(ver_soft);  // Сохранить строку с текущей версией.
    menu_start.setup();
    menu_start.drawStartPage();


    Serial.print("\nInitializing SD card...");

    // we'll use the initialization code from the utility libraries
    // since we're just testing if the card is working!
    if (!card.init(SPI_HALF_SPEED, chipSelect)) 
    {
        Serial.println("initialization failed. Things to check:");
        Serial.println("* is a card inserted?");
        Serial.println("* is your wiring correct?");
        Serial.println("* did you change the chipSelect pin to match your shield or module?");
        while (1);
    }
    else
    {
        Serial.println("Wiring is correct and a card is present.");
        // }

         // print the type of card
        Serial.println();
        Serial.print("Card type:         ");
        switch (card.type()) {
        case SD_CARD_TYPE_SD1:
            Serial.println("SD1");
            break;
        case SD_CARD_TYPE_SD2:
            Serial.println("SD2");
            break;
        case SD_CARD_TYPE_SDHC:
            Serial.println("SDHC");
            break;
        default:
            Serial.println("Unknown");
        }

        // Now we will try to open the 'volume'/'partition' - it should be FAT16 or FAT32
        if (!volume.init(card)) {
            Serial.println("Could not find FAT16/FAT32 partition.\nMake sure you've formatted the card");
            while (1);
        }

        Serial.print("Clusters:          ");
        Serial.println(volume.clusterCount());
        Serial.print("Blocks x Cluster:  ");
        Serial.println(volume.blocksPerCluster());

        Serial.print("Total Blocks:      ");
        Serial.println(volume.blocksPerCluster() * volume.clusterCount());
        Serial.println();

        // print the type and size of the first FAT-type volume
        uint32_t volumesize;
        Serial.print("Volume type is:    FAT");
        Serial.println(volume.fatType(), DEC);

        volumesize = volume.blocksPerCluster();    // clusters are collections of blocks
        volumesize *= volume.clusterCount();       // we'll have a lot of clusters
        volumesize /= 2;                           // SD card blocks are always 512 bytes (2 blocks are 1KB)
        Serial.print("Volume size (Kb):  ");
        Serial.println(volumesize);
        Serial.print("Volume size (Mb):  ");
        volumesize /= 1024;
        Serial.println(volumesize);
        Serial.print("Volume size (Gb):  ");
        Serial.println((float)volumesize / 1024.0);

        Serial.println("\nFiles found on the card (name, date and size in bytes): ");
        root.openRoot(volume);

        // list all files in the card with date and size
        root.ls(LS_R | LS_DATE | LS_SIZE);
    }
}

void loop() 
{
    static uint32_t lastFreqUpdate = 0;
    static float lastMeasuredFreq = 0.0f;

    static uint8_t prev_hour = 255, prev_min = 255, prev_sec = 255;
    static uint32_t prevFreqInt = 0, prevFreqFrac = 0;

    // --- 1. Расчет частоты раз в секунду ---
    if (millis() - lastFreqUpdate > 1000) 
    {
        noInterrupts();
        uint16_t n = diff_count;
        uint32_t sum = diff_sum;
        diff_count = 0; diff_sum = 0;
        interrupts();
        if (n > 0)
        {
            float Tavg = (float)sum / n;
            frequency = (1e6f / Tavg);
            lastMeasuredFreq = frequency;
        }
        lastFreqUpdate = millis();
    }

    // --- 2. Чтение кнопки с антидребезгом ---
    static bool lastBtnState = HIGH;
    static uint32_t lastDebounce = 0;
    bool btnState = digitalRead(BUTTON_PIN);

    if (btnState != lastBtnState) 
    {
        lastDebounce = millis();
        lastBtnState = btnState;
    }
    bool btnStableState = lastBtnState;
    if ((millis() - lastDebounce) > 25) 
    {
        btnStableState = btnState;
    }

    // --- 3. Вывод либо частоты, либо времени ТОЛЬКО при изменении ---
    if (btnStableState == LOW) 
    {
        // Кнопка нажата: ЧАСТОТА
        uint32_t curFreqInt = (uint32_t)lastMeasuredFreq;
        uint32_t curFreqFrac = (uint32_t)((lastMeasuredFreq - curFreqInt) * 10000.0);
        if (curFreqInt != prevFreqInt || curFreqFrac != prevFreqFrac) 
        {
            displayFrequency(lastMeasuredFreq);
            prevFreqInt = curFreqInt;
            prevFreqFrac = curFreqFrac;
            Serial.print(F("frequency:"));
            Serial.println(frequency, 4);

        }
    }
    else 
    {
        // Кнопка не нажата: ВРЕМЯ
        RTCTime now = rtc.getTime();
        if (now.hour != prev_hour || now.minute != prev_min || now.second != prev_sec) 
        {
            displayTime(now.hour, now.minute, now.second);
            prev_hour = now.hour;
            prev_min = now.minute;
            prev_sec = now.second;
 
            Serial.print(rtc.getDayOfWeekStr(now));
            Serial.print(F(" "));
            Serial.print(rtc.getDateStr(now));
            Serial.print(F(" - "));
            Serial.print(rtc.getTimeStr(now));
            Serial.println();

        }
    }
    btnSW1.update();
    btnSW2.update();
    btnSW3.update();
    btnSW4.update();

    if (!menu_start.isActive()) 
    {
        if (btnSW1.getEvent() == BTN_LONG) 
        {
            menu_start.activate(); // Вход в меню (выбор страницы)
        }
    }
    else 
    {
        // В режиме выбора страницы
        if (menu_start.getState() == MENU_PAGE_SELECT)
        {
            if (btnSW2.getEvent() == BTN_SHORT) menu_start.nextPage();
            if (btnSW3.getEvent() == BTN_SHORT) menu_start.prevPage();
            if (btnSW4.getEvent() == BTN_SHORT) menu_start.select(); // перейти к выбору пункта
            if (btnSW1.getEvent() == BTN_SHORT) menu_start.deactivate(); // выход из меню без выбора
        }
        // В режиме выбора пункта
        else if (menu_start.getState() == MENU_ITEM_SELECT) 
        {
            if (btnSW2.getEvent() == BTN_SHORT) menu_start.nextItem();
            if (btnSW3.getEvent() == BTN_SHORT) menu_start.prevItem();
            if (btnSW4.getEvent() == BTN_SHORT) menu_start.select(); // выбор пункта, выход из меню
            if (btnSW1.getEvent() == BTN_SHORT) menu_start.deactivate(); // выход из меню без выбора
        }
    }
    delay(10); // Для стабильности работы кнопок
}


