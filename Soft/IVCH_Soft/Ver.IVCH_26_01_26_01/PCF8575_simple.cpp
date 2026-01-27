#include "PCF8575_simple.h"

PCF8575_simple::PCF8575_simple(uint8_t address) : _address(address) {}

void PCF8575_simple::begin() {
    Wire.begin();
}

void PCF8575_simple::write16(uint16_t data) {
    Wire.beginTransmission(_address);
    Wire.write(data & 0xFF);        // ������� ���� (P00...P07)
    Wire.write((data >> 8) & 0xFF); // ������� ���� (P08...P15)
    Wire.endTransmission();
}
