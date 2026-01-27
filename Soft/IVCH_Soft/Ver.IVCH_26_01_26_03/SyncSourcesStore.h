#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "EepromMap.h"

class SyncSourcesStore {
public:
	static constexpr uint8_t I2C_ADDR = EepromMap::I2C_ADDR;

	struct Data {
		uint8_t gpsEnable;
		uint8_t gpsPeriodIdx;

		uint8_t netEnable;
		uint8_t netProviderIdx;
		uint8_t netPeriodIdx;

		uint8_t gsmEnable;
		uint8_t gsmProviderIdx;
		uint8_t gsmPeriodIdx;

		uint8_t net2Enable;
		uint8_t net2PeriodIdx;
	};

	explicit SyncSourcesStore(TwoWire& w = Wire, uint8_t addr = I2C_ADDR);

	bool begin();
	void defaults(Data& d);

	bool load(Data& d); // strict:только BASE
	bool save(const Data& d); // strict:только BASE

	// EEPROM MAP (фикс. для внешних программ)
	static constexpr uint16_t BASE = EepromMap::SyncSources::BASE;

	static constexpr uint16_t ADDR_MAGIC = BASE + 0; // u16
	static constexpr uint16_t ADDR_VER = BASE + 2; // u8
	static constexpr uint16_t ADDR_DATA = BASE + 4; // Data bytes

	static constexpr uint16_t ADDR_gpsEnable = ADDR_DATA + 0;
	static constexpr uint16_t ADDR_gpsPeriodIdx = ADDR_DATA + 1;
	static constexpr uint16_t ADDR_netEnable = ADDR_DATA + 2;
	static constexpr uint16_t ADDR_netProviderIdx = ADDR_DATA + 3;
	static constexpr uint16_t ADDR_netPeriodIdx = ADDR_DATA + 4;
	static constexpr uint16_t ADDR_gsmEnable = ADDR_DATA + 5;
	static constexpr uint16_t ADDR_gsmProviderIdx = ADDR_DATA + 6;
	static constexpr uint16_t ADDR_gsmPeriodIdx = ADDR_DATA + 7;
	static constexpr uint16_t ADDR_net2Enable = ADDR_DATA + 8;
	static constexpr uint16_t ADDR_net2PeriodIdx = ADDR_DATA + 9;

private:
	TwoWire* _w;
	uint8_t _addr;

	static constexpr uint16_t MAGIC = 0x5359; // 'SY'
	static constexpr uint8_t VERSION = 2;
	static constexpr uint8_t PAGE_SIZE = 64;

	bool readBytes(uint16_t memAddr, uint8_t* out, size_t len);
	bool writeBytes(uint16_t memAddr, const uint8_t* data, size_t len);
	bool writePage(uint16_t memAddr, const uint8_t* data, size_t len);
	bool waitReady(uint32_t timeoutMs = 50);

	bool readU8(uint16_t memAddr, uint8_t& v);
	bool writeU8(uint16_t memAddr, uint8_t v);
	bool readU16(uint16_t memAddr, uint16_t& v);
	bool writeU16(uint16_t memAddr, uint16_t v);
};