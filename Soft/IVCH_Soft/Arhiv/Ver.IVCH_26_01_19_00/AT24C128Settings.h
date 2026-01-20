#pragma once
#include <Arduino.h>
#include <Wire.h>

class AT24C128Settings {
public:
	static constexpr uint8_t I2C_ADDR = 0x50;

	struct Config {
		char apn[32];
		char user[32];
		char pass[32];
		char server[48];

		int8_t tzTargetHours; // TZ для RTC/экрана (например 2)
		int8_t tzNtpHours; // TZ для AT+CNTP (рекомендую 0=UTC)

		bool enableFallback;
		uint32_t periodMs;
	};

	explicit AT24C128Settings(TwoWire& w = Wire, uint8_t addr = I2C_ADDR);

	bool begin();

	// strict=true:требует MAGIC+VERSION
	// strict=false:читает поля по фикс. адресам даже если MAGIC не задан
	bool load(Config& cfg, bool strict = false);

	bool save(const Config& cfg, bool writeMagic = true);

	static void defaults(Config& cfg);

	// --- Низкоуровневые:поля по отдельности ---
	bool writeAPN(const char* s);
	bool writeUSER(const char* s);
	bool writePASS(const char* s);
	bool writeSERVER(const char* s);

	bool writeTzTargetHours(int8_t tz);
	bool writeTzNtpHours(int8_t tz);

	bool writeEnableFallback(bool en);
	bool writePeriodMs(uint32_t ms);

	bool readAPN(char* out, size_t outSize);
	bool readUSER(char* out, size_t outSize);
	bool readPASS(char* out, size_t outSize);
	bool readSERVER(char* out, size_t outSize);

	bool readTzTargetHours(int8_t& tz);
	bool readTzNtpHours(int8_t& tz);

	bool readEnableFallback(bool& en);
	bool readPeriodMs(uint32_t& ms);

	// --- Фиксированные адреса (для внешних программ) ---
	static constexpr uint16_t ADDR_MAGIC = 0x0000; // u32 'S8CF'
	static constexpr uint16_t ADDR_VER = 0x0004; // u8 version
	static constexpr uint16_t ADDR_ENFALL = 0x0005; // u8 0/1
	static constexpr uint16_t ADDR_TZ_TARGET = 0x0006; // i8
	static constexpr uint16_t ADDR_TZ_NTP = 0x0007; // i8 (новое)
	static constexpr uint16_t ADDR_PERIOD = 0x0008; // u32 LE

	static constexpr uint16_t ADDR_APN = 0x0100; // 32 bytes
	static constexpr uint16_t ADDR_USER = 0x0120; // 32 bytes
	static constexpr uint16_t ADDR_PASS = 0x0140; // 32 bytes
	static constexpr uint16_t ADDR_SERVER = 0x0160; // 48 bytes

private:
	TwoWire* _w;
	uint8_t _addr;

	static constexpr uint8_t PAGE_SIZE = 64;
	static constexpr uint32_t MAGIC = 0x46433853; // 'S''8''C''F' в LE
	static constexpr uint8_t VERSION = 2; // было 1,теперь 2

	bool readBytes(uint16_t memAddr, uint8_t* out, size_t len);
	bool writeBytes(uint16_t memAddr, const uint8_t* data, size_t len);
	bool writePage(uint16_t memAddr, const uint8_t* data, size_t len);
	bool waitReady(uint32_t timeoutMs = 50);

	bool readU32(uint16_t memAddr, uint32_t& v);
	bool writeU32(uint16_t memAddr, uint32_t v);

	bool readI8(uint16_t memAddr, int8_t& v);
	bool writeI8(uint16_t memAddr, int8_t v);

	bool readU8(uint16_t memAddr, uint8_t& v);
	bool writeU8(uint16_t memAddr, uint8_t v);

	bool readFixedString(uint16_t memAddr, char* out, size_t outSize, size_t maxFieldSize);
	bool writeFixedString(uint16_t memAddr, const char* s, size_t maxFieldSize);

	static bool isPrintableAscii(char c);
	static bool isValidTextString(const char* s);
};