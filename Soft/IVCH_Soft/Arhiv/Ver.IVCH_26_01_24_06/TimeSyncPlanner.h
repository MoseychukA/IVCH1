#pragma once
#include <Arduino.h>

#include "SIM800TimeAsync.h"
#include "AT24C128Settings.h"
#include "SyncSourcesStore.h"
#include "RTCSupport.h"
#include "GPSNmeaParser.h"
#include "Internet2Client.h" // <-- ДОБАВЛЕНО

class TimeSyncPlanner {
public:
	// Новый конструктор:с поддержкой Internet2
	TimeSyncPlanner(SIM800TimeAsync& sim,
		GPSNmeaParser& gps,
		RealtimeClock& rtc,
		AT24C128Settings::Config& cfg,
		SyncSourcesStore::Data& src,
		Internet2Client* internet2 = nullptr);

	void begin();
	void tick();

	void onSettingsChanged();
	void onTimeUpdated(); // вызывайте после установки времени от SIM800 (как у вас)

	void triggerImmediate();

private:
	SIM800TimeAsync& _sim;
	GPSNmeaParser& _gps;
	RealtimeClock& _rtc;
	AT24C128Settings::Config& _cfg;
	SyncSourcesStore::Data& _src;
	Internet2Client* _internet2 = nullptr;

	uint32_t _nextGpsMs = 0;
	uint32_t _nextNetMs = 0; // SIM800 NTP
	uint32_t _nextGsmMs = 0; // SIM800 CCLK
	uint32_t _nextNet2Ms = 0; // Internet2 over I2C

	bool _immediatePending = false;

	// Internet2 state
	bool _net2Pending = false;
	uint32_t _net2ReqStartedMs = 0;
	uint32_t _net2LastSeenNtpUtc = 0;

	// push DS3231 time to Internet2
	uint32_t _nextPushNet2Ms = 0;

private:
	static uint32_t periodMsFromIdx(uint8_t idx);
	static uint32_t catchUpNext(uint32_t now, uint32_t next, uint32_t period);

	static uint8_t dow0_sun(int y, int m, int d);
	static uint8_t ds3231DowFromDow0(uint8_t dow0);

	// UTC<->Local helpers
	static int64_t toEpochSeconds(int y, int mo, int d, int h, int mi, int s);
	static void fromEpochSeconds(int64_t t, int& y, int& mo, int& d, int& h, int& mi, int& s);
	static void utcToLocal(int& y, int& mo, int& d, int& h, int& mi, int& s, int tzQuarterHours);

	// DS3231(local) -> unix UTC using cfg.tzTargetHours
	uint32_t rtcLocalToUnixUtc(const RTCTime& t) const;

	// unix UTC -> DS3231(local) using cfg.tzTargetHours
	void setRtcFromUnixUtc(uint32_t unixUtc);

	// Periodic send DS3231 time to Internet2 (so it can serve NTP locally)
	void tickPushTimeToInternet2(uint32_t now);

	bool tryStartGps(uint32_t now, bool immediate);
	bool tryStartNet(uint32_t now, bool immediate); // SIM800 NTP
	bool tryStartGsm(uint32_t now, bool immediate); // SIM800 CCLK
	bool tryStartNet2(uint32_t now, bool immediate); // Internet2 I2C
};