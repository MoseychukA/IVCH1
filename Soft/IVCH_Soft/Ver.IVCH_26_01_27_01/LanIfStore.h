#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "EepromMap.h"

class LanIfStore {
public:
	enum IfId :uint8_t { IF1 = 0, IF2 = 1 };

	struct IfConfig {
		uint8_t dhcp; // 0/1
		uint8_t ip[4];
		uint8_t mask[4];
		uint8_t gw[4];
		uint8_t dns[4];

		uint8_t ntpIdx; // 0..4
		uint8_t periodIdx; // 0..5
	};

	explicit LanIfStore(uint8_t i2cAddr = EepromMap::I2C_ADDR, TwoWire& w = Wire);

	bool begin();

	bool load(IfId id, IfConfig& cfg, bool strict = false);
	bool save(IfId id, const IfConfig& cfg, bool writeMagic = true);

	static void defaults(IfId id, IfConfig& cfg);

	// фиксированные адреса блоков (для внешних программ)
	static constexpr uint16_t BASE_IF1 = EepromMap::LanIf::BASE_IF1;
	static constexpr uint16_t BASE_IF2 = EepromMap::LanIf::BASE_IF2;

private:
	TwoWire* _w;
	uint8_t _addr;

	static constexpr uint8_t PAGE_SIZE = 64;
	static constexpr uint8_t VERSION = 1;

	static constexpr uint16_t OFF_MAGIC = 0;
	static constexpr uint16_t OFF_VER = 4;
	static constexpr uint16_t OFF_DHCP = 5;
	static constexpr uint16_t OFF_IP = 6;
	static constexpr uint16_t OFF_MASK = 10;
	static constexpr uint16_t OFF_GW = 14;
	static constexpr uint16_t OFF_DNS = 18;
	static constexpr uint16_t OFF_NTPIDX = 22;
	static constexpr uint16_t OFF_PERIOD = 23;

	static uint16_t baseFor(IfId id) { return (id == IF1) ? BASE_IF1 : BASE_IF2; }
	static uint32_t magicFor(IfId id) { return (id == IF1) ? 0x31464E49UL : 0x32464E49UL; } // 'INF1'/'INF2'

	bool readBytes(uint16_t memAddr, uint8_t* out, size_t len);
	bool writeBytes(uint16_t memAddr, const uint8_t* data, size_t len);
	bool writePage(uint16_t memAddr, const uint8_t* data, size_t len);
	bool waitReady(uint32_t timeoutMs = 50);

	bool readU32(uint16_t memAddr, uint32_t& v);
	bool writeU32(uint16_t memAddr, uint32_t v);
	bool readU8(uint16_t memAddr, uint8_t& v);
	bool writeU8(uint16_t memAddr, uint8_t v);
};