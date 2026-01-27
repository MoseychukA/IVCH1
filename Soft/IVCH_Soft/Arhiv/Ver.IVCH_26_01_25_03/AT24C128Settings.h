#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "EepromMap.h"

class AT24C128Settings {
public:
	static constexpr uint8_t I2C_ADDR = EepromMap::I2C_ADDR;

	struct Config {
		char apn[32];
		char user[32];
		char pass[32];
		char server[48];

		int8_t tzTargetHours; // ЧАСОВОЙ ПОЯС для RTC/экрана (например 3)
		int8_t tzNtpHours; // TZ для AT+CNTP (рекомендую 0=UTC)

		bool enableFallback;
		uint32_t periodMs;
	};

	explicit AT24C128Settings(TwoWire& w = Wire, uint8_t addr = I2C_ADDR);

	bool begin();

	// strict=true:требует MAGIC+VERSION
	// strict=false:читает поля по фикс. адресам даже если MAGIC не задан
	bool load(Config& cfg, bool strict = false);

	// writeMagic=true:пишет VER+MAGIC (MAGIC в конце как commit)
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
	static constexpr uint16_t ADDR_MAGIC = EepromMap::Settings::ADDR_MAGIC;
	static constexpr uint16_t ADDR_VER = EepromMap::Settings::ADDR_VER;
	static constexpr uint16_t ADDR_ENFALL = EepromMap::Settings::ADDR_ENFALL;
	static constexpr uint16_t ADDR_TZ_TARGET = EepromMap::Settings::ADDR_TZ_TARGET;
	static constexpr uint16_t ADDR_TZ_NTP = EepromMap::Settings::ADDR_TZ_NTP;
	static constexpr uint16_t ADDR_PERIOD = EepromMap::Settings::ADDR_PERIOD;

	static constexpr uint16_t ADDR_APN = EepromMap::Settings::ADDR_APN;
	static constexpr uint16_t ADDR_USER = EepromMap::Settings::ADDR_USER;
	static constexpr uint16_t ADDR_PASS = EepromMap::Settings::ADDR_PASS;
	static constexpr uint16_t ADDR_SERVER = EepromMap::Settings::ADDR_SERVER;

private:
	TwoWire* _w;
	uint8_t _addr;

	static constexpr uint8_t PAGE_SIZE = 64;
	static constexpr uint32_t MAGIC = 0x46433853; // 'S''8''C''F' в LE
	static constexpr uint8_t VERSION = 2;

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