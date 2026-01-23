#pragma once
#include <Arduino.h>

#include "SIM800TimeAsync.h"
#include "AT24C128Settings.h"
#include "SyncSourcesStore.h"
#include "RTCSupport.h"
#include "GPSNmeaParser.h"

class TimeSyncPlanner {
public:
	TimeSyncPlanner(SIM800TimeAsync& sim,
		GPSNmeaParser& gps,
		RealtimeClock& rtc,
		AT24C128Settings::Config& cfg,
		SyncSourcesStore::Data& src);

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

	uint32_t _nextGpsMs = 0;
	uint32_t _nextNetMs = 0;
	uint32_t _nextGsmMs = 0;

	bool _immediatePending = false;

private:
	static uint32_t periodMsFromIdx(uint8_t idx);
	static uint32_t catchUpNext(uint32_t now, uint32_t next, uint32_t period);

	static uint8_t dow0_sun(int y, int m, int d);
	static uint8_t ds3231DowFromDow0(uint8_t dow0);

	// UTC->Local по tzq (четверти часа)
	static int64_t toEpochSeconds(int y, int mo, int d, int h, int mi, int s);
	static void fromEpochSeconds(int64_t t, int& y, int& mo, int& d, int& h, int& mi, int& s);
	static void utcToLocal(int& y, int& mo, int& d, int& h, int& mi, int& s, int tzQuarterHours);

	bool tryStartGps(uint32_t now, bool immediate);
	bool tryStartNet(uint32_t now, bool immediate);
	bool tryStartGsm(uint32_t now, bool immediate);
};