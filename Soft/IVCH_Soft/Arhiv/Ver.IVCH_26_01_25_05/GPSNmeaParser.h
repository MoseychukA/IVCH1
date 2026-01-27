#pragma once
#include <Arduino.h>

// Глобальные показатели для меню (определены в .cpp)
extern uint8_t gGpsSatsUsed; // used in fix (GGA)
extern uint8_t gGpsSatsView; // in view (GSV)
extern bool gGpsFix; // RMC valid + GGA fixq>0
extern uint16_t gGpsHdop_x100; // HDOP*100 (GGA)
extern uint8_t gGpsSnrAvg; // средний SNR (условная "сила сигнала")
extern uint8_t gGpsSnrMax; // max SNR
extern uint32_t gGpsLastRmcMs;
extern uint32_t gGpsLastGgaMs;
extern uint32_t gGpsLastGsvMs;

class GPSNmeaParser {
public:
	explicit GPSNmeaParser(HardwareSerial& port);

	void begin(uint32_t baud = 9600);
	void tick(); // вызывать часто

	// Есть валидные дата/время UTC от GPS (RMC status A)
	bool hasUtc() const { return _rmcValid; }

	// Есть фиксация (RMC valid + GGA fixq>0)
	bool hasFix() const { return _rmcValid && (_ggaFixQ > 0); }

	// Получить UTC (если hasUtc()==true)
	bool getUtc(int& year, int& month, int& day, int& hour, int& minute, int& second) const;

private:
	HardwareSerial& _gps;

	static const size_t LINE_MAX = 128;
	char _line[LINE_MAX];
	size_t _len = 0;

	// UTC from RMC
	bool _rmcValid = false;
	int _utcYear = 0, _utcMonth = 0, _utcDay = 0, _utcHour = 0, _utcMin = 0, _utcSec = 0;

	// from GGA
	uint8_t _ggaFixQ = 0;

	// GSV aggregation for SNR
	uint16_t _snrSum = 0;
	uint8_t _snrCnt = 0;
	uint8_t _snrMax = 0;

private:
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
};