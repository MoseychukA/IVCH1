#pragma once
#include <Arduino.h>

// Единая карта EEPROM (AT24C128,I2C 0x50)
// Используется прошивкой и внешними программами.
namespace EepromMap {

	static constexpr uint8_t I2C_ADDR = 0x50;
	static constexpr uint16_t EEPROM_SIZE_BYTES = 16u * 1024u; // 0x0000..0x3FFF

	// -------------------- AT24C128Settings --------------------
	namespace Settings {
		static constexpr uint16_t ADDR_MAGIC = 0x0000; // u32 'S8CF' LE
		static constexpr uint16_t ADDR_VER = 0x0004; // u8
		static constexpr uint16_t ADDR_ENFALL = 0x0005; // u8 0/1
		static constexpr uint16_t ADDR_TZ_TARGET = 0x0006; // i8 (часовой пояс для RTC/экрана)
		static constexpr uint16_t ADDR_TZ_NTP = 0x0007; // i8 (TZ для AT+CNTP,обычно 0)
		static constexpr uint16_t ADDR_PERIOD = 0x0008; // u32 LE

		static constexpr uint16_t ADDR_APN = 0x0100; // 32
		static constexpr uint16_t ADDR_USER = 0x0120; // 32
		static constexpr uint16_t ADDR_PASS = 0x0140; // 32
		static constexpr uint16_t ADDR_SERVER = 0x0160; // 48

		static constexpr uint16_t END = ADDR_SERVER + 48; // 0x0190
	} // namespace Settings

	// -------------------- LanIfStore --------------------
	namespace LanIf {
		static constexpr uint16_t BLOCK_SIZE = 0x0040; // 64 bytes
		static constexpr uint16_t BASE_IF1 = 0x0200; // 0x0200..0x023F
		static constexpr uint16_t BASE_IF2 = 0x0240; // 0x0240..0x027F
		static constexpr uint16_t END = BASE_IF2 + BLOCK_SIZE; // 0x0280
	} // namespace LanIf

	// -------------------- SyncSourcesStore --------------------
	namespace SyncSources {
		static constexpr uint16_t BLOCK_SIZE = 0x0040; // 64 bytes
		static constexpr uint16_t BASE = 0x0280; // 0x0280..0x02BF
		static constexpr uint16_t END = BASE + BLOCK_SIZE; // 0x02C0
	} // namespace SyncSources

	// -------------------- Sanity checks --------------------
	static_assert(Settings::END <= LanIf::BASE_IF1, "EEPROM map overlap:Settings overlaps LanIf IF1");
	static_assert(LanIf::END <= SyncSources::BASE, "EEPROM map overlap:LanIf overlaps SyncSources");
	static_assert(SyncSources::END <= EEPROM_SIZE_BYTES, "EEPROM map exceeds AT24C128 size");

} // namespace EepromMap