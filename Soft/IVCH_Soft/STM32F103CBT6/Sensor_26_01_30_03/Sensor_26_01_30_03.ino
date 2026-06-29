#include <OneWireSTM.h>   // Библиотека Arduino_STM32-master.zip
//#include <STM32_TM1637.h> // http://forum.rcl-radio.ru/misc.php?action=pan_download&item=403&download=1
//#include <EEPROM.h>       // Входит в состав набора библиотек Arduino_STM32-master

//STM32_TM1637 tm(PB0, PB1);// CLK, DIO
OneWire  ds(8); // A7

//  CLK      PB0 (TM1637)
//  DIO      PB1 (TM1637)
//  OUT      PA7 (DS18B20)

byte i, present = 0, type_s = 0, data[12], addr[8];
float celsius;
byte w;
int reg;
const float gis = 1.0;// гистерезис
unsigned long times;

void setup() {
    Serial.begin(9600);



    //tm.brig(7); // ЯРКОСТЬ 0...7
    //EEPROM.init(0x801F000, 0x801F800, 0x400);// 1024 byte
    //pinMode(PB6, INPUT); // рег. темп. +
    //pinMode(PB5, INPUT); // рег. темп. -
    //pinMode(PB7, OUTPUT);// выход управления реле
    //reg = EEPROM.read(10) - 50;
}

void loop() {
    if (millis() - times > 1000) {
        ///////// 18b20 //////////////////////
        if (!ds.search(addr)) { ds.reset_search(); delay(250); return; }
        ds.reset(); ds.select(addr); ds.write(0x44, 1); delay(500); present = ds.reset(); ds.select(addr); ds.write(0xBE);
        for (i = 0; i < 9; i++) { data[i] = ds.read(); }
        int16_t raw = (data[1] << 8) | data[0]; if (type_s) { raw = raw << 3; }celsius = (float)raw / 16.0;
        //////// end 18b20 ////////////////////
    }

    if (digitalRead(PB6) == HIGH) { reg++; w = 1; times = millis(); if (reg > 125) { reg = 125; }delay(200); }
    if (digitalRead(PB5) == HIGH) { reg--; w = 1; times = millis(); if (reg < -50) { reg = -50; }delay(200); }

    if (millis() - times < 3000) { tm.print_float(reg, 0, 0b01010000, 0, 0, 0); }
    else { tm.print_float(celsius, 1, 0, 0, 0, 0); }

    if (reg >= celsius + gis) { digitalWrite(PB7, HIGH); }
    if (reg <= celsius - gis) { digitalWrite(PB7, LOW); }

    if (w == 1) { w = 0; EEPROM.update(10, reg + 50); }
}






//
///*
// Sensor node:STM32F103CBT6 (Arduino IDE / STM32duino)
//
// RS485 (UART2):
// RO -> PA3 (RX2)
// DI -> PA2 (TX2)
// RE/DE -> PC14 (HIGH=TX,LOW=RX)
//
// LoRa RFM95 868:
// NSS -> PA4
// RST -> PB0
// DIO0 -> PB13
// SCK -> PA5
// MOSI -> PA7
// MISO -> PA6
//
// I2C sensors:
// SCL -> PB6
// SDA -> PB7
// BMP180 (pressure) 1 min
//
// Extra sensors:
// DS18B20 -> PA8 (1-Wire)  [via OneWireSTM]
// DHT22 -> PB12
//
// GPS:
// USART1 RX -> PA10
// send TIME every 1 sec
//*/
//
//#include <Arduino.h>
//#include <Wire.h>
//#include <SPI.h>
//
//#include <Adafruit_BMP085.h>
//#include <TinyGPSPlus.h>
//#include <LoRa.h>
//#include <DHT.h>
//
//#include "OneWireSTM.h"
//
//// ---------------- Pins ----------------
//static const uint8_t RS485_DE_RE = PC14;
//
//static const uint8_t LORA_NSS = PA4;
//static const uint8_t LORA_RST = PB0;
//static const uint8_t LORA_DIO0 = PB13;
//
//static const uint8_t ONEWIRE_PIN = PB14;// PA8; // DS18B20
//static const uint8_t DHT_PIN = PB12;    // DHT22
//static const uint8_t DHT_TYPE = DHT22;
//
//// ---------------- UARTs ----------------
//HardwareSerial RS485(PA3, PA2); // PA3 RX, PA2 TX
//HardwareSerial GPS(PA10, PA9);  // PA10 RX, PA9 TX (TX not required)
//
//// ---------------- Sensors ----------------
//Adafruit_BMP085 bmp;
//TinyGPSPlus gps;
//DHT dht(DHT_PIN, DHT_TYPE);
//
//// ---------------- Protocol ----------------
//static uint16_t crc16_modbus(const uint8_t* data, size_t len)
//{
//    uint16_t crc = 0xFFFF;
//    for (size_t i = 0; i < len; i++) {
//        crc ^= data[i];
//        for (uint8_t b = 0; b < 8; b++) {
//            if (crc & 1) crc = (crc >> 1) ^ 0xA001;
//            else crc >>= 1;
//        }
//    }
//    return crc;
//}
//
//static void printHexByte(uint8_t b)
//{
//    static const char* H = "0123456789ABCDEF";
//    Serial.write(H[(b >> 4) & 0x0F]);
//    Serial.write(H[b & 0x0F]);
//}
//
//static void printFrameHex(const uint8_t* frame, size_t n)
//{
//    for (size_t i = 0; i < n; i++) {
//        printHexByte(frame[i]);
//        if (i + 1 < n) Serial.write(' ');
//    }
//}
//
//static void rs485SendFrame(const uint8_t* frame, size_t n)
//{
//    digitalWrite(RS485_DE_RE, HIGH);
//    delayMicroseconds(20);
//    RS485.write(frame, n);
//    RS485.flush();
//    delayMicroseconds(20);
//    digitalWrite(RS485_DE_RE, LOW);
//}
//
//static bool loraSendFrame(const uint8_t* frame, size_t n)
//{
//    LoRa.beginPacket();
//    LoRa.write(frame, n);
//    return (LoRa.endPacket() == 1);
//}
//
//static uint8_t gSeq = 0;
//
//// Build and send:AA 55 LEN TYPE SEQ PAYLOAD CRClo CRChi
//static void sendPacket(uint8_t type, const uint8_t* payload, uint8_t payloadLen, bool viaRs485, bool viaLora)
//{
//    const uint8_t LEN = (uint8_t)(2 + payloadLen);
//    uint8_t buf[64];
//    size_t p = 0;
//
//    buf[p++] = 0xAA;
//    buf[p++] = 0x55;
//    buf[p++] = LEN;
//    buf[p++] = type;
//    buf[p++] = gSeq++;
//
//    for (uint8_t i = 0; i < payloadLen; i++) buf[p++] = payload[i];
//
//    uint16_t crc = crc16_modbus(&buf[2], (size_t)(1 + LEN));
//    buf[p++] = (uint8_t)(crc & 0xFF);
//    buf[p++] = (uint8_t)(crc >> 8);
//
//    Serial.print(F("TX type=0x"));
//    printHexByte(type);
//    Serial.print(F(" len="));
//    Serial.print((unsigned)p);
//    Serial.print(F(" seq="));
//    Serial.print((unsigned)buf[4]);
//    Serial.print(F(" frame=["));
//    printFrameHex(buf, p);
//    Serial.println(F("]"));
//
//    if (viaRs485) {
//        rs485SendFrame(buf, p);
//        Serial.println(F(" -> RS485 sent"));
//    }
//    if (viaLora) {
//        bool ok = loraSendFrame(buf, p);
//        Serial.print(F(" -> LoRa sent="));
//        Serial.println(ok ? F("OK") : F("FAIL"));
//    }
//}
//
//// ---------------- GPS Unix UTC ----------------
//static bool gpsToUnixUtc(uint32_t& outUnixUtc, bool& outFix, uint8_t& outSats)
//{
//    outFix = gps.location.isValid() && gps.location.age() < 5000;
//    outSats = gps.satellites.isValid() ? (uint8_t)gps.satellites.value() : 0;
//
//    if (!gps.date.isValid() || !gps.time.isValid()) return false;
//
//    int y = gps.date.year();
//    int mo = gps.date.month();
//    int d = gps.date.day();
//    int h = gps.time.hour();
//    int mi = gps.time.minute();
//    int s = gps.time.second();
//
//    auto daysFromCivil = [](int y, int m, int d)->int32_t {
//        y -= (m <= 2);
//        const int era = (y >= 0 ? y : y - 399) / 400;
//        const unsigned yoe = (unsigned)(y - era * 400);
//        const unsigned doy = (153u * (unsigned)(m + (m > 2 ? -3 : 9)) + 2u) / 5u + (unsigned)d - 1u;
//        const unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
//        return (int32_t)(era * 146097 + (int)doe - 719468);
//    };
//
//    int32_t days = daysFromCivil(y, mo, d);
//    int64_t epoch = (int64_t)days * 86400LL + (int64_t)h * 3600LL + (int64_t)mi * 60LL + (int64_t)s;
//    if (epoch < 0) epoch = 0;
//    outUnixUtc = (uint32_t)epoch;
//    return true;
//}
//
//// ---------------- DS18B20 via OneWireSTM ----------------
//static const uint8_t DS18B20_CFG_9BIT = 0x1F; // config for 9-bit resolution
//
//static bool ds18_setResolution9bit(uint8_t pin)
//{
//    OneWire ow(pin);
//    if (!ow.reset()) return false;
//
//    ow.write(0xCC); // SKIP ROM
//    ow.write(0x4E); // WRITE SCRATCHPAD
//    ow.write(0x00); // TH
//    ow.write(0x00); // TL
//    ow.write(DS18B20_CFG_9BIT); // config
//
//    ow.reset();
//    ow.write(0xCC); // SKIP ROM
//    ow.write(0x48); // COPY SCRATCHPAD
//    delay(15);
//    ow.reset();
//    return true;
//}
//
//static bool ds18_startConversion(uint8_t pin)
//{
//    OneWire ow(pin);
//    if (!ow.reset()) return false;
//    ow.write(0xCC); // SKIP ROM
//    ow.write(0x44); // CONVERT T
//    return true;
//}
//
//static bool ds18_readTempC(uint8_t pin, float& outTempC)
//{
//    outTempC = NAN;
//
//    OneWire ow(pin);
//    if (!ow.reset()) return false;
//
//    ow.write(0xCC); // SKIP ROM
//    ow.write(0xBE); // READ SCRATCHPAD
//
//    uint8_t data[9];
//    for (uint8_t i = 0; i < 9; i++) data[i] = ow.read();
//
//    if (OneWire::crc8(data, 8) != data[8]) return false;
//
//    // DS18B20 raw temp
//    int16_t raw = (int16_t)((data[1] << 8) | data[0]);
//
//    // apply resolution mask based on config bits
//    uint8_t cfg = data[4] & 0x60;
//    if (cfg == 0x00) raw &= ~7;       // 9-bit
//    else if (cfg == 0x20) raw &= ~3;  // 10-bit
//    else if (cfg == 0x40) raw &= ~1;  // 11-bit
//    // 12-bit:keep
//
//    outTempC = (float)raw / 16.0f;
//
//    // sanity
//    if (outTempC < -55.0f || outTempC > 125.0f) 
//    {
//        outTempC = NAN;
//        return false;
//    }
//    return true;
//}
//
//// ---------------- Scheduler ----------------
//static uint32_t tLastTimeTx = 0;
//static uint32_t tLastSensTx = 0;
//
//static const bool USE_RS485 = true;
//static const bool USE_LORA = true;
//
//static void printSensorsOncePerMin(int32_t p_pa, float dsT, float dhtT, float dhtRH)
//{
//    Serial.println(F("SENS:"));
//    Serial.print(F(" BMP180 Pressure:")); Serial.print((long)p_pa); Serial.println(F(" Pa"));
//
//    Serial.print(F(" DS18B20 Temp:"));
//    if (isnan(dsT)) Serial.println(F("NaN"));
//    else { Serial.print(dsT, 2); Serial.println(F(" C")); }
//
//    Serial.print(F(" DHT22 Temp:"));
//    if (isnan(dhtT)) Serial.println(F("NaN"));
//    else { Serial.print(dhtT, 2); Serial.println(F(" C")); }
//
//    Serial.print(F(" DHT22 RH:"));
//    if (isnan(dhtRH)) Serial.println(F("NaN"));
//    else { Serial.print(dhtRH, 2); Serial.println(F(" %")); }
//}
//
//void setup()
//{
//    Serial.begin(115200);
//    delay(1500);
//
//    // версия
//    String ver_soft = __FILE__;
//    int val_srt = ver_soft.lastIndexOf('\\');
//    if (val_srt >= 0) ver_soft.remove(0, val_srt + 1);
//    val_srt = ver_soft.lastIndexOf('.');
//    if (val_srt >= 0) ver_soft.remove(val_srt);
//    Serial.print("************ ");
//    Serial.print(ver_soft);
//    Serial.println(" ************");
//
//    // RS485
//    pinMode(RS485_DE_RE, OUTPUT);
//    digitalWrite(RS485_DE_RE, LOW);
//    RS485.begin(115200);
//
//    // GPS
//    GPS.begin(9600);
//
//    // BMP180 I2C
//    Wire.setSCL(PB6);
//    Wire.setSDA(PB7);
//    Wire.begin();
//    Wire.setClock(100000);
//
//    bool okBmp = bmp.begin();
//    Serial.print(F("BMP180:")); Serial.println(okBmp ? F("OK") : F("FAIL"));
//
//    // DS18B20 (OneWireSTM)
//    pinMode(ONEWIRE_PIN, INPUT_PULLUP); // + внутренняя подтяжка
//    {
//        OneWire ow(ONEWIRE_PIN);
//        Serial.print(F("DS18B20 presence:"));
//        Serial.println(ow.reset() ? F("YES") : F("NO"));
//    }
//    bool okRes = ds18_setResolution9bit(ONEWIRE_PIN);
//    Serial.print(F("DS18B20 set 9bit:"));
//    Serial.println(okRes ? F("OK") : F("FAIL"));
//
//    // DHT22
//    dht.begin();
//    Serial.println(F("DHT22:init"));
//
//    // SPI
//    SPI.setSCLK(PA5);
//    SPI.setMOSI(PA7);
//    SPI.setMISO(PA6);
//    SPI.begin();
//
//    // LoRa
//    LoRa.setPins(LORA_NSS, LORA_RST, LORA_DIO0);
//    bool okLora = LoRa.begin(868E6);
//    Serial.print(F("LoRa:")); Serial.println(okLora ? F("OK") : F("FAIL"));
//}
//
//void loop()
//{
//    while (GPS.available()) gps.encode(GPS.read());
//
//    uint32_t now = millis();
//
//    // TIME every 1s
//    if (now - tLastTimeTx >= 60000UL) {
//        tLastTimeTx = now;
//
//        uint32_t unixUtc = 0;
//        bool fix = false;
//        uint8_t sats = 0;
//
//        if (gpsToUnixUtc(unixUtc, fix, sats)) {
//            Serial.print(F("GPS TIME:unixUtc="));
//            Serial.print((unsigned long)unixUtc);
//            Serial.print(F(" fix="));
//            Serial.print(fix ? F("1") : F("0"));
//            Serial.print(F(" sats="));
//            Serial.println((unsigned)sats);
//
//            uint8_t pl[6];
//            pl[0] = (uint8_t)(unixUtc & 0xFF);
//            pl[1] = (uint8_t)((unixUtc >> 8) & 0xFF);
//            pl[2] = (uint8_t)((unixUtc >> 16) & 0xFF);
//            pl[3] = (uint8_t)((unixUtc >> 24) & 0xFF);
//            pl[4] = (uint8_t)(fix ? 1 : 0);
//            pl[5] = sats;
//
//            sendPacket(0x01, pl, sizeof(pl), USE_RS485, USE_LORA);
//        }
//    }
//
//    // SENS every 60s
//    if (now - tLastSensTx >= 10000UL) {
//        tLastSensTx = now;
//
//        // BMP180
//        int32_t p_pa = (int32_t)bmp.readPressure();
//
//        // DS18B20 via OneWireSTM
//        float dsT = NAN;
//        if (ds18_startConversion(ONEWIRE_PIN)) {
//            delay(120); // 9-bit conversion typical 94ms
//            (void)ds18_readTempC(ONEWIRE_PIN, dsT);
//        }
//
//        // DHT22
//        float dhtRH = dht.readHumidity();
//        float dhtT = dht.readTemperature();
//
//        printSensorsOncePerMin(p_pa, dsT, dhtT, dhtRH);
//
//        // TYPE=0x02: BMP180 + DHT22
//        {
//            int16_t t_x100 = isnan(dhtT) ? (int16_t)0x8000 : (int16_t)lroundf(dhtT * 100.0f);
//            uint16_t rh_x100 = isnan(dhtRH) ? (uint16_t)0xFFFF : (uint16_t)lroundf(dhtRH * 100.0f);
//
//            uint8_t pl[8];
//            pl[0] = (uint8_t)(p_pa & 0xFF);
//            pl[1] = (uint8_t)((p_pa >> 8) & 0xFF);
//            pl[2] = (uint8_t)((p_pa >> 16) & 0xFF);
//            pl[3] = (uint8_t)((p_pa >> 24) & 0xFF);
//            pl[4] = (uint8_t)(t_x100 & 0xFF);
//            pl[5] = (uint8_t)((t_x100 >> 8) & 0xFF);
//            pl[6] = (uint8_t)(rh_x100 & 0xFF);
//            pl[7] = (uint8_t)((rh_x100 >> 8) & 0xFF);
//
//            Serial.println(F("SEND SENS TYPE=0x02 (BMP180 + DHT22)"));
//            sendPacket(0x02, pl, sizeof(pl), USE_RS485, USE_LORA);
//        }
//
//        // TYPE=0x03: DS18B20 + DHT22 + flags
//        {
//            int16_t ds_x100 = isnan(dsT) ? (int16_t)0x8000 : (int16_t)lroundf(dsT * 100.0f);
//            int16_t dht_t_x100 = isnan(dhtT) ? (int16_t)0x8000 : (int16_t)lroundf(dhtT * 100.0f);
//            uint16_t dht_rh_x100 = isnan(dhtRH) ? (uint16_t)0xFFFF : (uint16_t)lroundf(dhtRH * 100.0f);
//
//            uint8_t flags = 0;
//            if (!isnan(dsT)) flags |= 0x01;
//            if (!isnan(dhtT) && !isnan(dhtRH)) flags |= 0x02;
//
//            uint8_t pl[8];
//            pl[0] = (uint8_t)(ds_x100 & 0xFF);
//            pl[1] = (uint8_t)((ds_x100 >> 8) & 0xFF);
//            pl[2] = (uint8_t)(dht_t_x100 & 0xFF);
//            pl[3] = (uint8_t)((dht_t_x100 >> 8) & 0xFF);
//            pl[4] = (uint8_t)(dht_rh_x100 & 0xFF);
//            pl[5] = (uint8_t)((dht_rh_x100 >> 8) & 0xFF);
//            pl[6] = flags;
//            pl[7] = 0;
//
//            Serial.println(F("SEND SENS TYPE=0x03 (DS18B20 + DHT22)"));
//            sendPacket(0x03, pl, sizeof(pl), USE_RS485, USE_LORA);
//        }
//    }
//}
//
//
//
//
//
///*
// Sensor node:STM32F103CBT6 (Arduino IDE / STM32duino)
//
// RS485 (UART2):
// RO -> PA3 (RX2)
// DI -> PA2 (TX2)
// RE/DE -> PC14 (HIGH=TX,LOW=RX)
//
// LoRa RFM95 868:
// NSS -> PA4
// RST -> PB0
// DIO0 -> PB13
// SCK -> PA5
// MOSI -> PA7
// MISO -> PA6
//
// I2C sensors:
//// SCL -> PB6
//// SDA -> PB7
//// BMP180 (pressure) 1 min
////
//// Extra sensors:
//// DS18B20 -> PA8 (1-Wire)  <-- FIXED
//// DHT22 -> PB12
////
//// GPS:
//// USART1 RX -> PA10
//// send TIME every 1 sec
////*/
////
////#include <Arduino.h>
////#include <Wire.h>
////#include <SPI.h>
////
////#include <Adafruit_BMP085.h>
////#include <TinyGPSPlus.h>
////
////#include <LoRa.h>
////
////#include <OneWire.h>
////#include <DallasTemperature.h>
////#include <DHT.h>
////
////// ---------------- Pins ----------------
////static const uint8_t RS485_DE_RE = PC14;
////
////static const uint8_t LORA_NSS = PA4;
////static const uint8_t LORA_RST = PB0;
////static const uint8_t LORA_DIO0 = PB13;
////
////static const uint8_t ONEWIRE_PIN = PA8;  // DS18B20
////static const uint8_t DHT_PIN = PB12;     // DHT22
////static const uint8_t DHT_TYPE = DHT22;
////
////// ---------------- UARTs ----------------
////// STM32duino:HardwareSerial(rx,tx)
////HardwareSerial RS485(PA3, PA2); // PA3 RX,PA2 TX
////HardwareSerial GPS(PA10, PA9);  // PA10 RX,PA9 TX (TX not required)
////
////// ---------------- Sensors ----------------
////Adafruit_BMP085 bmp;
////TinyGPSPlus gps;
////
////OneWire oneWire(ONEWIRE_PIN);
////DallasTemperature ds18(&oneWire);
////DHT dht(DHT_PIN, DHT_TYPE);
////
////// ---------------- Protocol ----------------
////static uint16_t crc16_modbus(const uint8_t* data, size_t len)
////{
////	uint16_t crc = 0xFFFF;
////	for (size_t i = 0; i < len; i++) {
////		crc ^= data[i];
////		for (uint8_t b = 0; b < 8; b++) {
////			if (crc & 1) crc = (crc >> 1) ^ 0xA001;
////			else crc >>= 1;
////		}
////	}
////	return crc;
////}
////
////static void printHexByte(uint8_t b)
////{
////	static const char* H = "0123456789ABCDEF";
////	Serial.write(H[(b >> 4) & 0x0F]);
////	Serial.write(H[b & 0x0F]);
////}
////
////static void printFrameHex(const uint8_t* frame, size_t n)
////{
////	for (size_t i = 0; i < n; i++) {
////		printHexByte(frame[i]);
////		if (i + 1 < n) Serial.write(' ');
////	}
////}
////
////static void rs485SendFrame(const uint8_t* frame, size_t n)
////{
////	digitalWrite(RS485_DE_RE, HIGH);
////	delayMicroseconds(20);
////	RS485.write(frame, n);
////	RS485.flush();
////	delayMicroseconds(20);
////	digitalWrite(RS485_DE_RE, LOW);
////}
////
////static bool loraSendFrame(const uint8_t* frame, size_t n)
////{
////	LoRa.beginPacket();
////	LoRa.write(frame, n);
////	return (LoRa.endPacket() == 1); // blocking
////}
////
////static uint8_t gSeq = 0;
////
////// Build and send:AA 55 LEN TYPE SEQ PAYLOAD CRClo CRChi
////static void sendPacket(uint8_t type, const uint8_t* payload, uint8_t payloadLen, bool viaRs485, bool viaLora)
////{
////	const uint8_t LEN = (uint8_t)(2 + payloadLen); // TYPE+SEQ+PAYLOAD
////	uint8_t buf[64];
////	size_t p = 0;
////
////	buf[p++] = 0xAA;
////	buf[p++] = 0x55;
////	buf[p++] = LEN;
////	buf[p++] = type;
////	buf[p++] = gSeq++;
////
////	for (uint8_t i = 0; i < payloadLen; i++) buf[p++] = payload[i];
////
////	uint16_t crc = crc16_modbus(&buf[2], (size_t)(1 + LEN)); // LEN + body
////	buf[p++] = (uint8_t)(crc & 0xFF);
////	buf[p++] = (uint8_t)(crc >> 8);
////
////	// DEBUG:print frame
////	Serial.print(F("TX type=0x"));
////	printHexByte(type);
////	Serial.print(F(" len="));
////	Serial.print((unsigned)p);
////	Serial.print(F(" seq="));
////	Serial.print((unsigned)(buf[4]));
////	Serial.print(F(" frame=["));
////	printFrameHex(buf, p);
////	Serial.println(F("]"));
////
////	if (viaRs485) {
////		rs485SendFrame(buf, p);
////		Serial.println(F(" -> RS485 sent"));
////	}
////	if (viaLora) {
////		bool ok = loraSendFrame(buf, p);
////		Serial.print(F(" -> LoRa sent="));
////		Serial.println(ok ? F("OK") : F("FAIL"));
////	}
////}
////
////// ---------------- Unix UTC from GPS ----------------
////static bool gpsToUnixUtc(uint32_t& outUnixUtc, bool& outFix, uint8_t& outSats)
////{
////	outFix = gps.location.isValid() && gps.location.age() < 5000;
////	outSats = gps.satellites.isValid() ? (uint8_t)gps.satellites.value() : 0;
////
////	if (!gps.date.isValid() || !gps.time.isValid()) return false;
////
////	int y = gps.date.year();
////	int mo = gps.date.month();
////	int d = gps.date.day();
////	int h = gps.time.hour();
////	int mi = gps.time.minute();
////	int s = gps.time.second();
////
////	auto daysFromCivil = [](int y, int m, int d)->int32_t {
////		y -= (m <= 2);
////		const int era = (y >= 0 ? y : y - 399) / 400;
////		const unsigned yoe = (unsigned)(y - era * 400);
////		const unsigned doy = (153u * (unsigned)(m + (m > 2 ? -3 : 9)) + 2u) / 5u + (unsigned)d - 1u;
////		const unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
////		return (int32_t)(era * 146097 + (int)doe - 719468);
////	};
////
////	int32_t days = daysFromCivil(y, mo, d);
////	int64_t epoch = (int64_t)days * 86400LL + (int64_t)h * 3600LL + (int64_t)mi * 60LL + (int64_t)s;
////	if (epoch < 0) epoch = 0;
////	outUnixUtc = (uint32_t)epoch;
////	return true;
////}
////
////// ---------------- Scheduler ----------------
////static uint32_t tLastTimeTx = 0;
////static uint32_t tLastSensTx = 0;
////
////static const bool USE_RS485 = true;
////static const bool USE_LORA = true;
////
////static void printSensorsOncePerMin(int32_t p_pa, float dsT, float dhtT, float dhtRH)
////{
////	Serial.println(F("SENS:"));
////	Serial.print(F(" BMP180 Pressure:")); Serial.print((long)p_pa); Serial.println(F(" Pa"));
////
////	Serial.print(F(" DS18B20 Temp:"));
////	if (isnan(dsT)) Serial.println(F("NaN"));
////	else { Serial.print(dsT, 2); Serial.println(F(" C")); }
////
////	Serial.print(F(" DHT22 Temp:"));
////	if (isnan(dhtT)) Serial.println(F("NaN"));
////	else { Serial.print(dhtT, 2); Serial.println(F(" C")); }
////
////	Serial.print(F(" DHT22 RH:"));
////	if (isnan(dhtRH)) Serial.println(F("NaN"));
////	else { Serial.print(dhtRH, 2); Serial.println(F(" %")); }
////}
////
////void setup()
////{
////	Serial.begin(115200);
////	delay(1500);
////
////	// версия
////	String ver_soft = __FILE__;
////	int val_srt = ver_soft.lastIndexOf('\\');
////	if (val_srt >= 0) ver_soft.remove(0, val_srt + 1);
////	val_srt = ver_soft.lastIndexOf('.');
////	if (val_srt >= 0) ver_soft.remove(val_srt);
////	Serial.print("************ ");
////	Serial.print(ver_soft);
////	Serial.println(" ************");
////
////	// RS485
////	pinMode(RS485_DE_RE, OUTPUT);
////	digitalWrite(RS485_DE_RE, LOW);
////	RS485.begin(115200);
////
////	// GPS
////	GPS.begin(9600);
////
////	// BMP180 I2C
////	Wire.setSCL(PB6);
////	Wire.setSDA(PB7);
////	Wire.begin();
////	Wire.setClock(100000);
////	bool okBmp = bmp.begin();
////	Serial.print(F("BMP180:")); Serial.println(okBmp ? F("OK") : F("FAIL"));
////
////	// DS18B20
////	pinMode(ONEWIRE_PIN, INPUT_PULLUP); // additional pullup
////	ds18.begin();
////	ds18.setWaitForConversion(true);
////	ds18.setResolution(9);
////
////	Serial.println(F("DS18B20:init"));
////	Serial.print(F("DS18B20 devices:"));
////	Serial.println(ds18.getDeviceCount());
////
////	DeviceAddress a;
////	if (ds18.getAddress(a, 0)) {
////		Serial.print(F("DS18 addr:"));
////		for (uint8_t i = 0; i < 8; i++) { if (a[i] < 16) Serial.print('0'); Serial.print(a[i], HEX); }
////		Serial.println();
////	}
////	else {
////		Serial.println(F("DS18 addr:NOT FOUND"));
////	}
////
////	// DHT22
////	dht.begin();
////	Serial.println(F("DHT22:init"));
////
////	// SPI pins
////	SPI.setSCLK(PA5);
////	SPI.setMOSI(PA7);
////	SPI.setMISO(PA6);
////	SPI.begin();
////
////	// LoRa
////	LoRa.setPins(LORA_NSS, LORA_RST, LORA_DIO0);
////	bool okLora = LoRa.begin(868E6);
////	Serial.print(F("LoRa:")); Serial.println(okLora ? F("OK") : F("FAIL"));
////}
////
////void loop()
////{
////	while (GPS.available()) gps.encode(GPS.read());
////
////	uint32_t now = millis();
////
////	// TIME every 1s
////	if (now - tLastTimeTx >= 60000UL) 
////	{ 
////		tLastTimeTx = now;
////
////		uint32_t unixUtc = 0;
////		bool fix = false;
////		uint8_t sats = 0;
////
////		if (gpsToUnixUtc(unixUtc, fix, sats)) {
////			Serial.print(F("GPS TIME:unixUtc="));
////			Serial.print((unsigned long)unixUtc);
////			Serial.print(F(" fix="));
////			Serial.print(fix ? F("1") : F("0"));
////			Serial.print(F(" sats="));
////			Serial.println((unsigned)sats);
////
////			uint8_t pl[6];
////			pl[0] = (uint8_t)(unixUtc & 0xFF);
////			pl[1] = (uint8_t)((unixUtc >> 8) & 0xFF);
////			pl[2] = (uint8_t)((unixUtc >> 16) & 0xFF);
////			pl[3] = (uint8_t)((unixUtc >> 24) & 0xFF);
////			pl[4] = (uint8_t)(fix ? 1 : 0);
////			pl[5] = sats;
////
////			sendPacket(0x01, pl, sizeof(pl), USE_RS485, USE_LORA);
////		}
////		else {
////			Serial.print(F("GPS TIME:not valid. sats="));
////			Serial.println(gps.satellites.isValid() ? gps.satellites.value() : 0);
////		}
////	}
////
////	// SENS every 60s
////	if (now - tLastSensTx >= 10000UL) {
////		tLastSensTx = now;
////
////		// BMP180
////		int32_t p_pa = (int32_t)bmp.readPressure();
////
////		// DS18B20
////		ds18.requestTemperatures(); // blocks because waitForConversion=true
////		float dsT = ds18.getTempCByIndex(0);
////
////		// -127.00C = typical "disconnected" code in DallasTemperature
////		if (dsT <= -126.0f || dsT < -55.0f || dsT > 125.0f) 
////		{
////			dsT = NAN;
////		}
////		// DHT22
////		float dhtRH = dht.readHumidity();
////		float dhtT = dht.readTemperature(); // C
////
////		printSensorsOncePerMin(p_pa, dsT, dhtT, dhtRH);
////
////		// TYPE=0x02:pressure + temp + rh (using DHT22 for temp/rh)
////		{
////			int16_t t_x100 = isnan(dhtT) ? (int16_t)0x8000 : (int16_t)lroundf(dhtT * 100.0f);
////			uint16_t rh_x100 = isnan(dhtRH) ? (uint16_t)0xFFFF : (uint16_t)lroundf(dhtRH * 100.0f);
////
////			uint8_t pl[8];
////			pl[0] = (uint8_t)(p_pa & 0xFF);
////			pl[1] = (uint8_t)((p_pa >> 8) & 0xFF);
////			pl[2] = (uint8_t)((p_pa >> 16) & 0xFF);
////			pl[3] = (uint8_t)((p_pa >> 24) & 0xFF);
////			pl[4] = (uint8_t)(t_x100 & 0xFF);
////			pl[5] = (uint8_t)((t_x100 >> 8) & 0xFF);
////			pl[6] = (uint8_t)(rh_x100 & 0xFF);
////			pl[7] = (uint8_t)((rh_x100 >> 8) & 0xFF);
////
////			Serial.println(F("SEND SENS TYPE=0x02 (BMP180 + DHT22)"));
////			sendPacket(0x02, pl, sizeof(pl), USE_RS485, USE_LORA);
////		}
////
////		// TYPE=0x03:DS18B20 + DHT22 + flags
////		{
////			int16_t ds_x100 = (isnan(dsT) ? (int16_t)0x8000 : (int16_t)lroundf(dsT * 100.0f));
////			int16_t dht_t_x100 = (isnan(dhtT) ? (int16_t)0x8000 : (int16_t)lroundf(dhtT * 100.0f));
////			uint16_t dht_rh_x100 = (isnan(dhtRH) ? (uint16_t)0xFFFF : (uint16_t)lroundf(dhtRH * 100.0f));
////
////			uint8_t flags = 0;
////			if (!isnan(dsT)) flags |= 0x01;
////			if (!isnan(dhtT) && !isnan(dhtRH)) flags |= 0x02;
////
////			uint8_t pl[8];
////			pl[0] = (uint8_t)(ds_x100 & 0xFF);
////			pl[1] = (uint8_t)((ds_x100 >> 8) & 0xFF);
////			pl[2] = (uint8_t)(dht_t_x100 & 0xFF);
////			pl[3] = (uint8_t)((dht_t_x100 >> 8) & 0xFF);
////			pl[4] = (uint8_t)(dht_rh_x100 & 0xFF);
////			pl[5] = (uint8_t)((dht_rh_x100 >> 8) & 0xFF);
////			pl[6] = flags;
////			pl[7] = 0;
////
////			Serial.println(F("SEND SENS TYPE=0x03 (DS18B20 + DHT22)"));
////			sendPacket(0x03, pl, sizeof(pl), USE_RS485, USE_LORA);
////		}
////	}
////}
////
////
////
////
