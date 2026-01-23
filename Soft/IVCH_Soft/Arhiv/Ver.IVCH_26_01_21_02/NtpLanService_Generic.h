#pragma once
#include <Arduino.h>
#include <IPAddress.h>

#include "RTCSupport.h"
#include "AT24C128Settings.h"
#include "EthConfig.h"

class NtpLanService_Generic {
public:
	struct NetConfig {
		uint16_t macIndex = 0;
		bool useDhcp = true;

		IPAddress ip = IPAddress(192, 168, 1, 50);
		IPAddress dns = IPAddress(192, 168, 1, 1);
		IPAddress gw = IPAddress(192, 168, 1, 1);
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

private:
	struct Impl;
	Impl* _impl = nullptr;
};