#include "GPSNmeaParser.h"
#include <string.h>
#include <stdlib.h>

// ---- globals for меню ----
uint8_t gGpsSatsUsed = 0;
uint8_t gGpsSatsView = 0;
bool gGpsFix = false;
uint16_t gGpsHdop_x100 = 0;
uint8_t gGpsSnrAvg = 0;
uint8_t gGpsSnrMax = 0;
uint32_t gGpsLastRmcMs = 0;
uint32_t gGpsLastGgaMs = 0;
uint32_t gGpsLastGsvMs = 0;

GPSNmeaParser::GPSNmeaParser(HardwareSerial& port) :_gps(port) {}

void GPSNmeaParser::begin(uint32_t baud) {
	_gps.begin(baud);
}

void GPSNmeaParser::tick() {
	while (_gps.available()) {
		char c = (char)_gps.read();
		if (c == '\r') continue;

		if (c == '\n') {
			if (_len > 0) {
				_line[_len] = 0;
				onLine(_line);
				_len = 0;
			}
		}
		else {
			if (_len + 1 < LINE_MAX) _line[_len++] = c;
		}
	}
}

bool GPSNmeaParser::getUtc(int& year, int& month, int& day, int& hour, int& minute, int& second) const {
	if (!_rmcValid) return false;
	year = _utcYear; month = _utcMonth; day = _utcDay;
	hour = _utcHour; minute = _utcMin; second = _utcSec;
	return true;
}

void GPSNmeaParser::onLine(const char* s) {
	if (!s || s[0] != '$') return;
	if (!checksumOK(s)) return;

	const char* comma = strchr(s, ',');
	if (!comma) return;

	char buf[LINE_MAX];
	strncpy(buf, s + 1, sizeof(buf) - 1);
	buf[sizeof(buf) - 1] = 0;
	char* star = strchr(buf, '*');
	if (star) *star = 0;

	char* fields[32];
	int nf = 0;
	char* p = buf;
	while (p && nf < 32) {
		fields[nf++] = p;
		char* c = strchr(p, ',');
		if (!c) break;
		*c = 0;
		p = c + 1;
	}

	if (nf < 1) return;

	if (strcmp(fields[0], "GPRMC") == 0 || strcmp(fields[0], "GNRMC") == 0) parseRMC(fields, nf);
	else if (strcmp(fields[0], "GPGGA") == 0 || strcmp(fields[0], "GNGGA") == 0) parseGGA(fields, nf);
	else if (strcmp(fields[0], "GPGSV") == 0 || strcmp(fields[0], "GNGSV") == 0) parseGSV(fields, nf);
}

bool GPSNmeaParser::checksumOK(const char* s) {
	const char* star = strchr(s, '*');
	if (!star || star[1] == 0 || star[2] == 0) return false;

	uint8_t want = hex2u8(star[1], star[2]);
	uint8_t got = 0;
	for (const char* p = s + 1; p < star; p++) got ^= (uint8_t)(*p);
	return got == want;
}

uint8_t GPSNmeaParser::hex2u4(char c) {
	if (c >= '0' && c <= '9') return (uint8_t)(c - '0');
	if (c >= 'A' && c <= 'F') return (uint8_t)(c - 'A' + 10);
	if (c >= 'a' && c <= 'f') return (uint8_t)(c - 'a' + 10);
	return 0;
}
uint8_t GPSNmeaParser::hex2u8(char hi, char lo) { return (uint8_t)((hex2u4(hi) << 4) | hex2u4(lo)); }

bool GPSNmeaParser::parse_hhmmss(const char* s, int& hh, int& mm, int& ss) {
	if (!s || strlen(s) < 6) return false;
	hh = (s[0] - '0') * 10 + (s[1] - '0');
	mm = (s[2] - '0') * 10 + (s[3] - '0');
	ss = (s[4] - '0') * 10 + (s[5] - '0');
	if (hh < 0 || hh>23 || mm < 0 || mm>59 || ss < 0 || ss>59) return false;
	return true;
}

bool GPSNmeaParser::parse_ddmmyy(const char* s, int& dd, int& mo, int& yy) {
	if (!s || strlen(s) < 6) return false;
	dd = (s[0] - '0') * 10 + (s[1] - '0');
	mo = (s[2] - '0') * 10 + (s[3] - '0');
	yy = (s[4] - '0') * 10 + (s[5] - '0');
	if (dd < 1 || dd>31 || mo < 1 || mo>12) return false;
	return true;
}

void GPSNmeaParser::parseRMC(char** f, int nf) {
	// [1]=time [2]=status [9]=date
	if (nf < 10) return;

	int hh, mm, ss, dd, mo, yy;
	if (!parse_hhmmss(f[1], hh, mm, ss)) return;

	bool ok = (f[2] && f[2][0] == 'A'); // A=valid
	if (!ok) { _rmcValid = false; gGpsFix = false; return; }

	if (!parse_ddmmyy(f[9], dd, mo, yy)) return;

	_utcYear = 2000 + yy;
	_utcMonth = mo;
	_utcDay = dd;
	_utcHour = hh;
	_utcMin = mm;
	_utcSec = ss;

	_rmcValid = true;
	gGpsLastRmcMs = millis();

	gGpsFix = (_rmcValid && (_ggaFixQ > 0));
}

void GPSNmeaParser::parseGGA(char** f, int nf) {
	// [6]=fixq [7]=sats [8]=hdop
	if (nf < 9) return;

	_ggaFixQ = (uint8_t)atoi(f[6]);
	gGpsSatsUsed = (uint8_t)atoi(f[7]);

	if (f[8] && f[8][0]) {
		float hd = (float)atof(f[8]);
		if (hd < 0) hd = 0;
		if (hd > 99.99f) hd = 99.99f;
		gGpsHdop_x100 = (uint16_t)(hd * 100.0f + 0.5f);
	}

	gGpsLastGgaMs = millis();
	gGpsFix = (_rmcValid && (_ggaFixQ > 0));
}

void GPSNmeaParser::parseGSV(char** f, int nf) {
	// [3]=sats in view; groups,SNR at each 4th in group
	if (nf < 4) return;
	gGpsSatsView = (uint8_t)atoi(f[3]);

	_snrSum = 0; _snrCnt = 0; _snrMax = 0;

	for (int i = 4; i + 3 < nf; i += 4) {
		const char* snr = f[i + 3];
		if (snr && snr[0]) {
			int v = atoi(snr);
			if (v >= 0 && v <= 99) {
				_snrSum += (uint16_t)v;
				_snrCnt++;
				if ((uint8_t)v > _snrMax) _snrMax = (uint8_t)v;
			}
		}
	}

	gGpsSnrAvg = (_snrCnt > 0) ? (uint8_t)(_snrSum / _snrCnt) : 0;
	gGpsSnrMax = _snrMax;
	gGpsLastGsvMs = millis();
}