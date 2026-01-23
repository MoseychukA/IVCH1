#pragma once
#include <Arduino.h>
#include <Wire.h>

class SyncSourcesStore {
public:
	static constexpr uint8_t I2C_ADDR = 0x50;

	struct Data {
		uint8_t gpsEnable; // 0/1
		uint8_t gpsPeriodIdx; // 0..5

		uint8_t netEnable; // 0/1
		uint8_t netProviderIdx;// 0..4 (NTP server index)
		uint8_t netPeriodIdx; // 0..5

		uint8_t gsmEnable; // 0/1
		uint8_t gsmProviderIdx;// 0..4 (оператор)
		uint8_t gsmPeriodIdx; // 0..5
	};

	explicit SyncSourcesStore(TwoWire& w = Wire, uint8_t addr = I2C_ADDR);

	bool begin();
	void defaults(Data& d);
	bool load(Data& d); // strict (magic/ver)
	bool loadLoose(Data& d);// best effort (если magic нет Ч defaults)
	bool save(const Data& d);

	// фикс. адреса (дл€ внешних программ)
	static constexpr uint16_t BASE = 0x0200;
	static constexpr uint16_t ADDR_MAGIC = BASE + 0; // u16
	static constexpr uint16_t ADDR_VER = BASE + 2; // u8
	static constexpr uint16_t ADDR_DATA = BASE + 4; // Data bytes

private:
	TwoWire* _w;
	uint8_t _addr;

	static constexpr uint16_t MAGIC = 0x5359; // 'SY'
	static constexpr uint8_t VERSION = 1;
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