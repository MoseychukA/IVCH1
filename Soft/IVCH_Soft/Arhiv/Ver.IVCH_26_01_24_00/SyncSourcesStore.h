#pragma once
#include <Arduino.h>
#include <Wire.h>

class SyncSourcesStore {
public:
	static constexpr uint8_t I2C_ADDR = 0x50;

	struct Data {
		uint8_t gpsEnable; // 0/1
		uint8_t gpsPeriodIdx; // 0..5

		uint8_t netEnable; // 0/1 (SIM800 NTP)
		uint8_t netProviderIdx; // 0..4 (NTP server index)
		uint8_t netPeriodIdx; // 0..5

		uint8_t gsmEnable; // 0/1 (SIM800 CCLK)
		uint8_t gsmProviderIdx; // 0..4 (оператор)
		uint8_t gsmPeriodIdx; // 0..5

		uint8_t net2Enable; // 0/1
		uint8_t net2PeriodIdx; // 0..5
	};

	explicit SyncSourcesStore(TwoWire& w = Wire, uint8_t addr = I2C_ADDR);

	bool begin();
	void defaults(Data& d);

	// strict:только нова€ область BASE,magic+version должны совпасть
	bool load(Data& d);

	// strict:пишет только в BASE
	bool save(const Data& d);

	// ================== EEPROM MAP ==================
	// EEPROM одна (0x50). Ќельз€ пересекатьс€ с LanIfStore:
	// LanIfStore::IF1 = 0x0200..0x023F
	// LanIfStore::IF2 = 0x0240..0x027F
	//
	// SyncSourcesStore:0x0280..0x02BF
	static constexpr uint16_t BASE = 0x0280;

	static constexpr uint16_t ADDR_MAGIC = BASE + 0; // u16
	static constexpr uint16_t ADDR_VER = BASE + 2; // u8
	static constexpr uint16_t ADDR_DATA = BASE + 4; // Data bytes

	// (опционально) фикс адреса отдельных полей внутри Data
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
	static constexpr uint8_t VERSION = 2; // Data = 10 bytes
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