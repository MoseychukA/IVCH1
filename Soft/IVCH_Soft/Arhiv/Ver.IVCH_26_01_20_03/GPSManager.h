#pragma once
#include <Arduino.h>
#include "RTCSupport.h"
#include "AT24C128Settings.h"
#include "SyncSourcesStore.h"

// Глобальные показатели для меню (объявления; определения в GPSManager.cpp)
extern uint8_t gGpsSatsUsed; // used in fix (GGA)
extern uint8_t gGpsSatsView; // in view (GSV)
extern bool gGpsFix; // fix valid (RMC A + GGA fixq>0)
extern uint16_t gGpsHdop_x100; // HDOP*100 (GGA)
extern uint8_t gGpsSnrAvg; // average SNR (0..99) from GSV (условно "сила сигнала")
extern uint8_t gGpsSnrMax; // max SNR
extern uint32_t gGpsLastRmcMs; // когда последний раз пришёл RMC
extern uint32_t gGpsLastGgaMs; // когда последний раз пришёл GGA
extern uint32_t gGpsLastGsvMs; // когда последний раз пришёл GSV

class GPSManager {
public:
	GPSManager(HardwareSerial& gpsSerial, RealtimeClock& rtc,
		AT24C128Settings::Config& cfg,
		SyncSourcesStore::Data& syncData);

	void begin(uint32_t baud = 9600);
	void tick(); // вызывать часто

	// Немедленная синхронизация (если есть валидное время от GPS)
	void triggerImmediate();

	// Последняя попытка синхронизации успешна
	bool tookSync() const { return _lastSyncOk; }
	uint32_t lastSyncMs() const { return _lastSyncMs; }

private:
	HardwareSerial& _gps;
	RealtimeClock& _rtc;
	AT24C128Settings::Config& _cfg;
	SyncSourcesStore::Data& _sync;

	// NMEA line buffer
	static const size_t LINE_MAX = 128;
	char _line[LINE_MAX];
	size_t _len = 0;

	// parsed UTC time/date from RMC
	bool _rmcValid = false;
	int _utcYear = 0, _utcMonth = 0, _utcDay = 0, _utcHour = 0, _utcMin = 0, _utcSec = 0;

	// from GGA
	uint8_t _ggaFixQ = 0;

	// schedule
	bool _immediate = false;
	uint32_t _nextSyncMs = 0;
	bool _lastSyncOk = false;
	uint32_t _lastSyncMs = 0;

	// GSV aggregation
	uint16_t _snrSum = 0;
	uint8_t _snrCnt = 0;
	uint8_t _snrMax = 0;

private:
	void applySettings();
	uint32_t periodMsFromIdx(uint8_t idx) const;

	void onLine(const char* s);

	// NMEA helpers
	static bool checksumOK(const char* s);
	static uint8_t hex2u4(char c);
	static uint8_t hex2u8(char hi, char lo);

	static bool parse_hhmmss(const char* s, int& hh, int& mm, int& ss);
	static bool parse_ddmmyy(const char* s, int& dd, int& mo, int& yy);

	// Parsers
	void parseRMC(char** f, int nf);
	void parseGGA(char** f, int nf);
	void parseGSV(char** f, int nf);

	// RTC helpers
	static uint8_t dow0_sun(int y, int m, int d); // 0=Sun..6=Sat
	static uint8_t ds3231DowFromDow0(uint8_t dow0); // Sun->7,Mon..Sat->1..6
	static int64_t toEpochSeconds(int y, int mo, int d, int h, int mi, int s);
	static void fromEpochSeconds(int64_t t, int& y, int& mo, int& d, int& h, int& mi, int& s);
	static void utcToLocal(int& y, int& mo, int& d, int& h, int& mi, int& s, int tzQuarterHours);
};