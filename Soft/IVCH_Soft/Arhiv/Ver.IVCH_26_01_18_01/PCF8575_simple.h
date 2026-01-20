#pragma once
#include <Arduino.h>
#include <Wire.h>

class PCF8575_simple {
public:
    PCF8575_simple(uint8_t address);
    void begin();
    void write16(uint16_t data); // отправить 16 бит на расширитель

private:
    uint8_t _address;
};