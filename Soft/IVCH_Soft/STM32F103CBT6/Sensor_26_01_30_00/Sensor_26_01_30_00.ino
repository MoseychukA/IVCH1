
/*ваш полный.ino(STM32F103CBT6), в который добавлено :

Вывод в Serial :
показания всех датчиков,
отправляемые пакеты(тип / длина / CRC) и байты кадра в HEX,
результат отправки RS485 / LoRa.
Подключены датчики :
DS18B20 на PB14(OneWire + DallasTemperature)
DHT22 на PB12(DHT)
Для совместимости с базовым модулем :
как раньше отправляется TYPE = 0x02 (BMP180 + Si7021) 1 раз / мин
дополнительно отправляется TYPE = 0x03 (DS18B20 + DHT22) 1 раз / мин Это не ломает старый 
приёмник(он просто может игнорировать неизвестный 0x03).Позже можно добавить обработку 0x03 на базовом.
Библиотеки(Arduino IDE Library Manager) :

	LoRa(Sandeep Mistry)
	Adafruit BMP085
	TinyGPSPlus
	OneWire
	DallasTemperature
	DHT sensor library(Adafruit DHT)

	 Sensor node:STM32F103CBT6 (Arduino IDE / STM32duino)

	 RS485 (UART2):
	 RO -> PA3 (RX2)
	 DI -> PA2 (TX2)
	 RE/DE -> PC14 (HIGH=TX,LOW=RX)

	 LoRa RFM95 868:
	 NSS -> PA4
	 RST -> PB0
	 DIO0 -> PB13
	 SCK -> PA5 
	 MISO -> PA6

	 I2C sensors:
	 SCL -> PB6
	 SDA -> PB7
	 BMP180 (pressure) 1 min
	 Si7021 (temp/rh) 1 min

	 Extra sensors:
	 DS18B20 -> PB14 (1-Wire)
	 DHT22 -> PB12

	 GPS:
	 USART1 RX -> PA10
	 send TIME every 1 sec
	*/

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>

#include <Adafruit_BMP085.h>
#include <TinyGPSPlus.h>

#include <LoRa.h>

#include <OneWire.h>
#include <DallasTemperature.h>
#include <DHT.h>

	// ---------------- Pins ----------------
static const uint8_t RS485_DE_RE = PC14;

static const uint8_t LORA_NSS = PA4;
static const uint8_t LORA_RST = PB0;
static const uint8_t LORA_DIO0 = PB13;

static const uint8_t ONEWIRE_PIN = PB14; // DS18B20
static const uint8_t DHT_PIN = PB12; // DHT22
static const uint8_t DHT_TYPE = DHT22;

static const uint8_t LED = PC13;

// ---------------- UARTs ----------------
// STM32duino:HardwareSerial(rx,tx)
HardwareSerial RS485(PA3, PA2); // PA3 RX,PA2 TX
HardwareSerial GPS(PA10, PA9); // PA10 RX,PA9 TX (TX not required)

// ---------------- Sensors ----------------
Adafruit_BMP085 bmp;
//Adafruit_Si7021 si = Adafruit_Si7021();
TinyGPSPlus gps;

OneWire oneWire(ONEWIRE_PIN);
DallasTemperature ds18(&oneWire);
DHT dht(DHT_PIN, DHT_TYPE);

// ---------------- Protocol ----------------
static uint16_t crc16_modbus(const uint8_t* data, size_t len)
{
	uint16_t crc = 0xFFFF;
	for (size_t i = 0; i < len; i++) {
		crc ^= data[i];
		for (uint8_t b = 0; b < 8; b++) {
			if (crc & 1) crc = (crc >> 1) ^ 0xA001;
			else crc >>= 1;
		}
	}
	return crc;
}

static void printHexByte(uint8_t b)
{
	static const char* H = "0123456789ABCDEF";
	Serial.write(H[(b >> 4) & 0x0F]);
	Serial.write(H[b & 0x0F]);
}

static void printFrameHex(const uint8_t* frame, size_t n)
{
	for (size_t i = 0; i < n; i++) {
		printHexByte(frame[i]);
		if (i + 1 < n) Serial.write(' ');
	}
}

static void rs485SendFrame(const uint8_t* frame, size_t n)
{
	digitalWrite(RS485_DE_RE, HIGH);
	delayMicroseconds(20);
	RS485.write(frame, n);
	RS485.flush();
	delayMicroseconds(20);
	digitalWrite(RS485_DE_RE, LOW);
}

static bool loraSendFrame(const uint8_t* frame, size_t n)
{
	LoRa.beginPacket();
	LoRa.write(frame, n);
	return (LoRa.endPacket() == 1); // blocking
}

static uint8_t gSeq = 0;

// Build and send:AA 55 LEN TYPE SEQ PAYLOAD CRClo CRChi
static void sendPacket(uint8_t type, const uint8_t* payload, uint8_t payloadLen, bool viaRs485, bool viaLora)
{
	digitalWrite(LED, HIGH);
	const uint8_t LEN = (uint8_t)(2 + payloadLen); // TYPE+SEQ+PAYLOAD
	uint8_t buf[64];
	size_t p = 0;

	buf[p++] = 0xAA;
	buf[p++] = 0x55;
	buf[p++] = LEN;
	buf[p++] = type;
	buf[p++] = gSeq++;

	for (uint8_t i = 0; i < payloadLen; i++) buf[p++] = payload[i];

	uint16_t crc = crc16_modbus(&buf[2], (size_t)(1 + LEN)); // LEN + body
	buf[p++] = (uint8_t)(crc & 0xFF);
	buf[p++] = (uint8_t)(crc >> 8);

	// DEBUG:print frame
	Serial.print(F("TX type=0x"));
	printHexByte(type);
	Serial.print(F(" len="));
	Serial.print((unsigned)p);
	Serial.print(F(" seq="));
	Serial.print((unsigned)(buf[4]));
	Serial.print(F(" frame=["));
	printFrameHex(buf, p);
	Serial.println(F("]"));

	if (viaRs485) {
		rs485SendFrame(buf, p);
		Serial.println(F(" -> RS485 sent"));
	}
	if (viaLora) {
		bool ok = loraSendFrame(buf, p);
		Serial.print(F(" -> LoRa sent="));
		Serial.println(ok ? F("OK") : F("FAIL"));
	}
	digitalWrite(LED, LOW);
}

// ---------------- Unix UTC from GPS ----------------
static bool gpsToUnixUtc(uint32_t& outUnixUtc, bool& outFix, uint8_t& outSats)
{
	outFix = gps.location.isValid() && gps.location.age() < 5000;
	outSats = gps.satellites.isValid() ? (uint8_t)gps.satellites.value() : 0;

	if (!gps.date.isValid() || !gps.time.isValid()) return false;

	int y = gps.date.year();
	int mo = gps.date.month();
	int d = gps.date.day();
	int h = gps.time.hour();
	int mi = gps.time.minute();
	int s = gps.time.second();

	auto daysFromCivil = [](int y, int m, int d)->int32_t {
		y -= (m <= 2);
		const int era = (y >= 0 ? y : y - 399) / 400;
		const unsigned yoe = (unsigned)(y - era * 400);
		const unsigned doy = (153u * (unsigned)(m + (m > 2 ? -3 : 9)) + 2u) / 5u + (unsigned)d - 1u;
		const unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
		return (int32_t)(era * 146097 + (int)doe - 719468);
	};

	int32_t days = daysFromCivil(y, mo, d);
	int64_t epoch = (int64_t)days * 86400LL + (int64_t)h * 3600LL + (int64_t)mi * 60LL + (int64_t)s;
	if (epoch < 0) epoch = 0;
	outUnixUtc = (uint32_t)epoch;
	return true;
}

// ---------------- Scheduler ----------------
static uint32_t tLastTimeTx = 0;
static uint32_t tLastSensTx = 0;

static const bool USE_RS485 = true;
static const bool USE_LORA = true;

static void i2cScan()
{
	Serial.println(F("I2C scan..."));
	uint8_t found = 0;
	for (uint8_t a = 1; a < 127; a++) {
		Wire.beginTransmission(a);
		if (Wire.endTransmission() == 0) {
			Serial.print(F(" Found 0x"));
			if (a < 16) Serial.print('0');
			Serial.println(a, HEX);
			found++;
		}
	}
	Serial.print(F("I2C found:"));
	Serial.println(found);
}


void setup()
{
	Serial.begin(115200);
	delay(2000);

	// версия
	String ver_soft = __FILE__;
	int val_srt = ver_soft.lastIndexOf('\\');
	if (val_srt >= 0) ver_soft.remove(0, val_srt + 1);
	val_srt = ver_soft.lastIndexOf('.');
	if (val_srt >= 0) ver_soft.remove(val_srt);
	Serial.print("************ ");
	Serial.print(ver_soft);
	Serial.println(" ************");

	pinMode(RS485_DE_RE, OUTPUT);
	digitalWrite(RS485_DE_RE, LOW);

	pinMode(LED, OUTPUT);
	digitalWrite(LED, LOW);

	RS485.begin(115200);
	GPS.begin(9600);

	// I2C pins
	Wire.setSCL(PB6);
	Wire.setSDA(PB7);

	Wire.begin();

	delay(50);
	i2cScan();


	bool okBmp = bmp.begin();
	Serial.print(F("BMP180:")); Serial.println(okBmp ? F("OK") : F("FAIL"));
	
	// DS18B20
	ds18.begin();
	Serial.println(F("DS18B20:init"));

	// DHT22
	dht.begin();
	Serial.println(F("DHT22:init"));

	// SPI pins
	SPI.setSCLK(PA5);
	SPI.setMOSI(PA7);
	SPI.setMISO(PA6);
	SPI.begin();

	// LoRa pins
	LoRa.setPins(LORA_NSS, LORA_RST, LORA_DIO0);
	bool okLora = LoRa.begin(868E6);
	Serial.print(F("LoRa:")); Serial.println(okLora ? F("OK") : F("FAIL"));

	// Optional tuning:
	// LoRa.setSpreadingFactor(7);
	// LoRa.setSignalBandwidth(125E3);
	// LoRa.setCodingRate4(5);
	// LoRa.setTxPower(13);
}

static void printSensorsOncePerMin(int32_t p_pa, float dsT, float dhtT, float dhtRH)
{
	Serial.println(F("SENS:"));
	Serial.print(F(" BMP180 Pressure:")); Serial.print((long)p_pa); Serial.println(F(" Pa"));

	Serial.print(F(" DS18B20 Temp:"));
	if (isnan(dsT)) Serial.println(F("NaN"));
	else { Serial.print(dsT, 2); Serial.println(F(" C")); }

	Serial.print(F(" DHT22 Temp:"));
	if (isnan(dhtT)) Serial.println(F("NaN"));
	else { Serial.print(dhtT, 2); Serial.println(F(" C")); }

	Serial.print(F(" DHT22 RH:"));
	if (isnan(dhtRH)) Serial.println(F("NaN"));
	else { Serial.print(dhtRH, 2); Serial.println(F(" %")); }
}

void loop()
{
	while (GPS.available()) gps.encode(GPS.read());

	uint32_t now = millis();

	// TIME every 1s
	if (now - tLastTimeTx >= 60000UL) 
	{
		tLastTimeTx = now;

		uint32_t unixUtc = 0;
		bool fix = false;
		uint8_t sats = 0;

		if (gpsToUnixUtc(unixUtc, fix, sats)) {
			Serial.print(F("GPS TIME:unixUtc="));
			Serial.print((unsigned long)unixUtc);
			Serial.print(F(" fix="));
			Serial.print(fix ? F("1") : F("0"));
			Serial.print(F(" sats="));
			Serial.println((unsigned)sats);

			uint8_t pl[6];
			pl[0] = (uint8_t)(unixUtc & 0xFF);
			pl[1] = (uint8_t)((unixUtc >> 8) & 0xFF);
			pl[2] = (uint8_t)((unixUtc >> 16) & 0xFF);
			pl[3] = (uint8_t)((unixUtc >> 24) & 0xFF);
			pl[4] = (uint8_t)(fix ? 1 : 0);
			pl[5] = sats;

			sendPacket(0x01, pl, sizeof(pl), USE_RS485, USE_LORA);
		}
		else 
		{
			// show minimal debug once per second
			Serial.print(F("GPS TIME:not valid (date/time missing). fix="));
			Serial.print(gps.location.isValid() ? F("Y") : F("N"));
			Serial.print(F(" sats="));
			Serial.println(gps.satellites.isValid() ? gps.satellites.value() : 0);
		}
	}

	// SENS every 60s
	if (now - tLastSensTx >= 10000UL) 
	{
		tLastSensTx = now;

		// I2C sensors
		int32_t p_pa = (int32_t)bmp.readPressure();

		// DS18B20
		ds18.requestTemperatures();
		float dsT = ds18.getTempCByIndex(0);
		if (dsT <= -126.0f || dsT >= 125.0f) {
			// Dallas lib uses -127 as error
			if (dsT < -100.0f) dsT = NAN;
		}

		// DHT22
		float dhtRH = dht.readHumidity();
		float dhtT = dht.readTemperature(); // C

		// Print to Serial
		printSensorsOncePerMin(p_pa,dsT, dhtT, dhtRH);

		// Packet TYPE=0x02 (legacy:BMP180 + Si7021)
		{
			int16_t t_x100 = 0/*(int16_t)lroundf(siT * 100.0f)*/;
			uint16_t rh_x100 = 0/*(uint16_t)lroundf(siRH * 100.0f)*/;

			uint8_t pl[8];
			pl[0] = (uint8_t)(p_pa & 0xFF);
			pl[1] = (uint8_t)((p_pa >> 8) & 0xFF);
			pl[2] = (uint8_t)((p_pa >> 16) & 0xFF);
			pl[3] = (uint8_t)((p_pa >> 24) & 0xFF);
			pl[4] = (uint8_t)(t_x100 & 0xFF);
			pl[5] = (uint8_t)((t_x100 >> 8) & 0xFF);
			pl[6] = (uint8_t)(rh_x100 & 0xFF);
			pl[7] = (uint8_t)((rh_x100 >> 8) & 0xFF);

			Serial.println(F("SEND SENS TYPE=0x02 (BMP180)"));
			sendPacket(0x02, pl, sizeof(pl), USE_RS485, USE_LORA);
		}

		// Packet TYPE=0x03 (new:DS18B20 + DHT22)
		{
			int16_t ds_x100 = (isnan(dsT) ? (int16_t)0x8000 : (int16_t)lroundf(dsT * 100.0f));
			int16_t dht_t_x100 = (isnan(dhtT) ? (int16_t)0x8000 : (int16_t)lroundf(dhtT * 100.0f));
			uint16_t dht_rh_x100 = (isnan(dhtRH) ? (uint16_t)0xFFFF : (uint16_t)lroundf(dhtRH * 100.0f));

			// flags bit0:ds valid,bit1:dht valid
			uint8_t flags = 0;
			if (!isnan(dsT)) flags |= 0x01;
			if (!isnan(dhtT) && !isnan(dhtRH)) flags |= 0x02;

			uint8_t pl[8];
			pl[0] = (uint8_t)(ds_x100 & 0xFF);
			pl[1] = (uint8_t)((ds_x100 >> 8) & 0xFF);
			pl[2] = (uint8_t)(dht_t_x100 & 0xFF);
			pl[3] = (uint8_t)((dht_t_x100 >> 8) & 0xFF);
			pl[4] = (uint8_t)(dht_rh_x100 & 0xFF);
			pl[5] = (uint8_t)((dht_rh_x100 >> 8) & 0xFF);
			pl[6] = flags;
			pl[7] = 0; // reserved

			Serial.println(F("SEND SENS TYPE=0x03 (DS18B20+DHT22)"));
			sendPacket(0x03, pl, sizeof(pl), USE_RS485, USE_LORA);
		}
	}
}

