#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <IPAddress.h>

class Internet2Client {
public:
	struct Status {
		uint8_t protoVer = 0;
		uint8_t flags = 0;
		IPAddress ip = IPAddress(0, 0, 0, 0);
		int32_t lastOffsetSec = 0;
		uint32_t lastNtpUtc = 0;
		bool lastSyncOk = false;
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
	bool readStatus(Status& out);

private:
	TwoWire* _w;
	uint8_t _addr;

	bool writeReg(uint8_t reg, const uint8_t* data, size_t len);
	bool readReg(uint8_t reg, uint8_t* data, size_t len);
};