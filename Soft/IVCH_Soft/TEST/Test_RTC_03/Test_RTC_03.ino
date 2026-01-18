/*
    Name:       Ver.IVCH_26_01_16_01.ino
    Created:	16.01.2026 16:54:33
    Author:     Alex



По нажатию кнопки на PC0(например, кнопка замыкает на GND), индикаторы отображают частоту.
В остальное время — время(часы, минуты, секунды).
Кнопка подтянута к питанию резистором!
Кнопка учитывается с программной антидребезговой задержкой.
*/


/*
индикаторы нужно обновлять только при изменении данных(часов / минут / секунд или частоты).
А при дальнейшем нажатии кнопки не тратить ресурсы i2c на повторяющийся вывод.

Также важно : вероятная причина, что частота не отображается — вы обновляете дисплей внутри условия кнопки, но время обновляется постоянно вне 
зависимости от изменений, а частота — только если был факт изменений частоты.

Вот доработанный фрагмент с правильной логикой сравнения предыдущих значений(часы : минуты : секунды, а также целая и дробная часть частоты) :
*/
#include "Arduino.h"
#include "PCF8575_simple.h"
#include <Wire.h>
#include "RTCSupport.h"
#include <stdio.h>

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

    // Для отладки — строка 
 /*   char strbuf[16];
    snprintf(strbuf, sizeof(strbuf), "%02u.%04u", d1 * 10 + d2, fract);*/
    //Serial.print(F("frequency:"));
    //Serial.println(strbuf);

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
    //pinMode(BUTTON_PIN, INPUT); // Кнопка к GND!
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
}

