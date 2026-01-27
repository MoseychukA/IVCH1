#pragma once
#include <Arduino.h>
#include <Wire.h>

class PCF8575_simple {
public:
    PCF8575_simple(uint8_t address);
    void begin();
    void write16(uint16_t data); 

private:
    uint8_t _address;
};
