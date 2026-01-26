#pragma once

#include <Arduino.h>
#include <IPAddress.h>

#include "RTCSupport.h"
#include "AT24C128Settings.h"
#include "EthConfig.h"

class NtpLanService_Generic
{
public:
	struct NetConfig
	{
		uint16_t macIndex = 0;
		bool useDhcp = true;

		IPAddress ip = IPAddress(192, 168, 75, 106);
		IPAddress dns = IPAddress(8, 8, 8, 8);
		IPAddress gw = IPAddress(192, 168, 75, 1);
		IPAddress mask = IPAddress(255, 255, 255, 0);

		// Внешний NTP (лучше IP)
		IPAddress upstreamIp = IPAddress(162, 159, 200, 123);
		uint16_t upstreamPort = 123;

		uint32_t syncPeriodMs = 3600000UL;
	};

	NtpLanService_Generic(RealtimeClock& rtc, AT24C128Settings::Config& cfg);
	~NtpLanService_Generic();

	bool begin(const NetConfig& net);
	void tick();
	void forceSyncNow();

	// статус
	bool lastSyncOk() const;
	uint32_t lastAttemptMs() const;
	uint32_t lastSuccessMs() const;

	void printStatus1Hz();

	// live re-config
	void applyUpstream(IPAddress ip, uint32_t periodMs);
	void applyLanConfig(bool dhcp, IPAddress ip, IPAddress dns, IPAddress gw, IPAddress mask);

	// для NetFeed (чтобы .ino не включал Ethernet_Generic)
	IPAddress localIP() const;

private:
	struct Impl;
	Impl* _impl = nullptr;
};