#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <IPAddress.h>

class Internet2Client {
public:
	struct Status {
		uint8_t protoVer = 0;

		// raw status byte[1] from slave:
		// low nibble:flags (bit0 linkUp,bit1 dhcp,bit2 haveMasterTime,bit3 syncInProgress)
		// high nibble:lastErr (0..15)
		uint8_t raw1 = 0;

		// decoded
		uint8_t flags = 0; // 0..15 (low nibble)
		uint8_t lastErr = 0; // 0..15 (high nibble)

		IPAddress ip = IPAddress(0, 0, 0, 0);

		int32_t lastOffsetSec = 0;
		uint32_t lastNtpUtc = 0;
		bool lastSyncOk = false;

		// NEW (if slave supports it; else will be 0)
		uint8_t netReinitCount = 0;

		// helpers
		bool linkUp() const { return (flags & (1 << 0)) != 0; }
		bool dhcp() const { return (flags & (1 << 1)) != 0; }
		bool haveMasterTime() const { return (flags & (1 << 2)) != 0; }
		bool syncInProgress() const { return (flags & (1 << 3)) != 0; }
	};

	explicit Internet2Client(TwoWire& w = Wire, uint8_t addr = 0x42);

	bool begin();

	bool setTimeUnixUtc(uint32_t unixUtc, uint16_t ms = 0);

	bool applyNetCfg(bool dhcp,
		IPAddress ip,
		IPAddress mask,
		IPAddress gw,
		IPAddress dns,
		IPAddress ntpUpstream,
		uint32_t periodMs);

	bool requestSyncNow();

	// NEW:soft "reconnect" on INTERNET2 module (Ethernet+UDP restart)
	bool requestNetReinit();

	bool readStatus(Status& out);

private:
	TwoWire* _w;
	uint8_t _addr;

	bool writeReg(uint8_t reg, const uint8_t* data, size_t len);
	bool readReg(uint8_t reg, uint8_t* data, size_t len);
};