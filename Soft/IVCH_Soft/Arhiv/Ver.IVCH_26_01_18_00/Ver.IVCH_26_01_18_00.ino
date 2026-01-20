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

TFT_eSPI tft = TFT_eSPI();  // Invoke library, pins defined in User_Setup.h

// Определяем пины кнопок
#define SW1_PIN PC3 // Вызов/выход меню (длинное/короткое)
#define SW2_PIN PC2 // Навигация вверх/стр.
#define SW3_PIN PC1 // Навигация вниз/стр.
#define SW4_PIN PC0 // ОК/Запись/выход из пункта

#define MENU_PAGES 5
#define MENU_ITEMS 5

// Структура меню
const char* menu[MENU_PAGES][MENU_ITEMS] = {
  {"Page1-Item1", "Page1-Item2", "Page1-Item3", "Page1-Item4", "Page1-Item5"},
  {"Page2-Item1", "Page2-Item2", "Page2-Item3", "Page2-Item4", "Page2-Item5"},
  {"Page3-Item1", "Page3-Item2", "Page3-Item3", "Page3-Item4", "Page3-Item5"},
  {"Page4-Item1", "Page4-Item2", "Page4-Item3", "Page4-Item4", "Page4-Item5"},
  {"Page5-Item1", "Page5-Item2", "Page5-Item3", "Page5-Item4", "Page5-Item5"},
};

int currPage = 0;
int currItem = 0;
bool menuActive = false;


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

void doResetI2C(uint8_t sclPin, uint8_t sdaPin) { /* ... */ }
void resetI2C() { doResetI2C(SCL, SDA); }

void test_indicators() 
{
    // Для каждого индикатора (0..5) по очереди выводим счет от 0 до 9
    for (int digitIndex = 0; digitIndex < 6; digitIndex++) 
    {
        for (int num = 0; num < 10; num++) 
        {
            uint8_t left = 0, right = 0;
            if (digitIndex % 2 == 0) 
            {
                // первый индикатор (левый) у данного PCF
                left = ~segTable[num];   // инверсия ― инверсное управление!
                right = 0xFF;            // погашен (все сегменты отрицательного логического уровня)
            }
            else 
            {
                // второй индикатор (правый) у данного PCF
                left = 0xFF;             // погашен
                right = ~segTable[num];  // инверсия
            }
            switch (digitIndex / 2) 
            {
            case 0: pcfHour.write16(left | (right << 8)); break;
            case 1: pcfMin.write16(left | (right << 8)); break;
            case 2: pcfSec.write16(left | (right << 8)); break;
            }
            delay(250);
        }
        // По окончании цикла гасим оба разряда
        switch (digitIndex / 2) 
        {
        case 0: pcfHour.write16(0xFFFF); break;
        case 1: pcfMin.write16(0xFFFF); break;
        case 2: pcfSec.write16(0xFFFF); break;
        }
        delay(500); // пауза между разрядами
    }
}

//
//static const uint8_t PIN_SD_CS = PA8; // CD/DAT3
//static const uint8_t PIN_SD_MISO = PA6; // DAT0
//static const uint8_t PIN_SD_MOSI = PA7; // CMD
//static const uint8_t PIN_SD_SCK = PA5; // CLK
//
//// Создаём SPI на нужных пинах (MISO, MOSI, SCK)
//SPIClass SPI_1(PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_SCK);
//
//void listDir(File dir, int numTabs) 
//{
//    while (true) 
//    {
//        File entry = dir.openNextFile();
//        if (!entry) break;
//
//        for (int i = 0; i < numTabs; i++) Serial.print('\t');
//        Serial.print(entry.name());
//
//        if (entry.isDirectory()) {
//            Serial.println("/");
//            listDir(entry, numTabs + 1);
//        }
//        else {
//            Serial.print("\t");
//            Serial.println(entry.size());
//        }
//        entry.close();
//    }
//}

Sd2Card card;
SdVolume volume;
SdFile root;

// change this to match your SD shield or module;
// Arduino Ethernet shield: pin 4
// Adafruit SD shields and modules: pin 10
// Sparkfun SD shield: pin 8
// MKRZero SD: SDCARD_SS_PIN
const int chipSelect = PA8;




void setup() 
{
    Serial.begin(115200);
    Wire.begin();
    resetI2C();
    rtc.begin();
    pcfHour.begin();
    pcfMin.begin();
    pcfSec.begin();
    pinMode(INPUT_PIN, INPUT);
    attachInterrupt(INPUT_PIN, freqInterrupt, RISING);
   // test_indicators();
   // rtc.setTime(0, 59, 18, 6, 17, 1, 2026);
    tft.init();
    tft.setRotation(3);
    tft.fillScreen(TFT_BLACK);
    //pinMode(SW1_PIN, INPUT_PULLUP);
    //pinMode(SW2_PIN, INPUT_PULLUP);
    //pinMode(SW3_PIN, INPUT_PULLUP);
    //pinMode(SW4_PIN, INPUT_PULLUP);

    // Приветствие/стартовый экран
    tft.setTextColor(TFT_GREEN); tft.setTextSize(2);
    tft.drawString("STM32F103 Menu Demo", 20, 40);

  //  Test_TFT();
    String ver_soft = __FILE__;
    int val_srt = ver_soft.lastIndexOf('\\');
    ver_soft.remove(0, val_srt + 1);
    val_srt = ver_soft.lastIndexOf('.');
    ver_soft.remove(val_srt);
    Serial.println(ver_soft);
   // TFTScreen->setVer(ver_soft);  // Сохранить строку с текущей версией.



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

    if (checkLongPress(SW1_PIN)) { // Длинное нажатие
        menuMode(true);
    }
    if (menuActive) {
        menuHandler();
    }
}

void menuMode(bool active) {
    menuActive = active;
    currPage = 0;
    currItem = 0;
    if (menuActive) {
        drawMenu();
        // Ждём отпускания SW1 во избежание ложного выхода
        while (digitalRead(SW1_PIN) == LOW);
        delay(50); // Мини-антидребезг
    }
    else {
        tft.fillScreen(TFT_BLACK);
    }
}

// Функция отрисовки меню
void drawMenu() 
{
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE); tft.setTextSize(2);
    char pageStr[16];
    sprintf(pageStr, "Page %d/5", currPage + 1);
    tft.drawString(pageStr, 10, 10);
    for (int i = 0; i < MENU_ITEMS; i++) 
    {
        if (i == currItem) 
        {
            tft.setTextColor(TFT_YELLOW, TFT_BLUE);
        }
        else 
        {
            tft.setTextColor(TFT_WHITE, TFT_BLACK);
        }
        tft.drawString(menu[currPage][i], 30, 40 + i * 30);
    }
}

// Функция обработки меню
void menuHandler() 
{
    // Навигация страниц/пунктов
    if (buttonPressed(SW2_PIN)) 
    {
        currItem--;
        if (currItem < 0) {
            currItem = MENU_ITEMS - 1;
        }
        drawMenu();
        delay(150);
    }
    if (buttonPressed(SW3_PIN)) 
    {
        currItem++;
        if (currItem >= MENU_ITEMS) 
        {
            currItem = 0;
        }
        drawMenu();
        delay(150);
    }

    // Короткое нажатие SW1 — выход из меню
    if (buttonPressed(SW1_PIN)) 
    {
        menuActive = false;
        tft.fillScreen(TFT_BLACK);
        delay(200);
    }
    // ОК — фиксируем выбор (SW4)
    if (buttonPressed(SW4_PIN)) 
    {
        handleMenuSelect(currPage, currItem);
        menuActive = false;
        tft.fillScreen(TFT_BLACK);
        delay(200);
    }
}

// Реакция на выбор пункта меню (можно расширять своё действие)
void handleMenuSelect(int page, int item) 
{
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_GREEN); tft.setTextSize(2);
    tft.drawString("Selected:", 10, 40);
    tft.drawString(menu[page][item], 10, 80);
    delay(1000);
}

// Проверка длинного нажатия (пример)
bool checkLongPress(uint8_t pin) 
{
    if (digitalRead(pin) == LOW) 
    {
        unsigned long t0 = millis();
        while (digitalRead(pin) == LOW) 
        {
            if (millis() - t0 > 700) // 700 мс — длинное нажатие
                return true;
        }
    }
    return false;
}

// Проверка короткого нажатия
bool buttonPressed(uint8_t pin) 
{
    static uint32_t last_press[4] = { 0,0,0,0 }; // для SW1-4
    int idx = 3 - (pin - PC0); // PC0->3, PC1->2, PC2->1, ...
    if (digitalRead(pin) == LOW && (millis() - last_press[idx] > 250))
    {
        last_press[idx] = millis();
        while (digitalRead(pin) == LOW);
        return true;
    }
    return false;
}


void Test_TFT()
{

    // Fill screen with grey so we can see the effect of printing with and without 
 // a background colour defined
    tft.fillScreen(TFT_BLACK);

    // Set "cursor" at top left corner of display (0,0) and select font 2
    // (cursor will move to next line automatically during printing with 'tft.println'
    //  or stay on the line is there is room for the text with tft.print)
    tft.setCursor(0, 0, 2);
    // Set the font colour to be white with a black background, set text size multiplier to 1
    tft.setTextColor(TFT_WHITE, TFT_BLACK);  tft.setTextSize(1);
    // We can now plot text on screen using the "print" class
    tft.println("Hello World!");

    // Set the font colour to be yellow with no background, set to font 7
    tft.setTextColor(TFT_YELLOW); tft.setTextFont(2);
    tft.println(1234.56);

    // Set the font colour to be red with black background, set to font 4
    tft.setTextColor(TFT_RED, TFT_BLACK);    tft.setTextFont(4);
    tft.println((long)3735928559, HEX); // Should print DEADBEEF

    // Set the font colour to be green with black background, set to font 2
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextFont(2);
    tft.println("Groop");

    // Test some print formatting functions
    float fnumber = 123.45;
    // Set the font colour to be blue with no background, set to font 2
    tft.setTextColor(TFT_BLUE);    tft.setTextFont(2);
    tft.print("Float = "); tft.println(fnumber);           // Print floating point number
    tft.print("Binary = "); tft.println((int)fnumber, BIN); // Print as integer value in binary
    tft.print("Hexadecimal = "); tft.println((int)fnumber, HEX); // Print as integer number in Hexadecimal


}