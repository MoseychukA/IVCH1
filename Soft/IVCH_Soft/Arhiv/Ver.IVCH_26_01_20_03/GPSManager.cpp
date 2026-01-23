#include "GPSManager.h"

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

GPSManager::GPSManager(HardwareSerial& gpsSerial, RealtimeClock& rtc,
	AT24C128Settings::Config& cfg,
	SyncSourcesStore::Data& syncData)
	:_gps(gpsSerial), _rtc(rtc), _cfg(cfg), _sync(syncData) {}

void GPSManager::begin(uint32_t baud) {
	_gps.begin(baud);
	_nextSyncMs = millis(); // чтобы при включЄнном GPS мог быстро синхронизироватьс€
}

void GPSManager::triggerImmediate() {
	_immediate = true;
}

uint32_t GPSManager::periodMsFromIdx(uint8_t idx) const {
	static const uint32_t kPeriodsMs[6] = {
	60000UL,600000UL,1800000UL,3600000UL,21600000UL,43200000UL
	};
	return kPeriodsMs[idx % 6];
}

void GPSManager::applySettings() {
	// если GPS выключен Ч ничего не делаем
	// период берЄм из меню GPS (syncData.gpsPeriodIdx)
}

void GPSManager::tick() {
	applySettings();

	// 1) читаем UART неблокирующе
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

	// 2) синхронизаци€ по расписанию (только если включено)
	if (!_sync.gpsEnable) return;

	const uint32_t now = millis();
	const uint32_t period = periodMsFromIdx(_sync.gpsPeriodIdx);

	bool due = _immediate || (int32_t)(now - _nextSyncMs) >= 0;
	if (!due) return;

	// Ќужно валидное UTC врем€ из RMC
	if (!_rmcValid) {
		// нет времени Ч не двигаем next,просто подождЄм следующий тик
		_lastSyncOk = false;
		return;
	}

	// —читаем УfixФ достаточным,если RMC valid и GGA fixq>0 (если GGA ещЄ не было,разрешим только по RMC)
	bool fixOk = (_ggaFixQ > 0) || (millis() - gGpsLastGgaMs > 10000);
	if (!fixOk) {
		_lastSyncOk = false;
		return;
	}

	// UTC -> local(target TZ)
	int y = _utcYear, mo = _utcMonth, d = _utcDay, h = _utcHour, mi = _utcMin, s = _utcSec;
	int tzq_target = (int)_cfg.tzTargetHours * 4;
	utcToLocal(y, mo, d, h, mi, s, tzq_target);

	uint8_t dow0 = dow0_sun(y, mo, d);
	uint8_t dsDow = ds3231DowFromDow0(dow0);

	_rtc.setTime((uint8_t)s, (uint8_t)mi, (uint8_t)h, dsDow, (uint8_t)d, (uint8_t)mo, (uint16_t)y);

	_lastSyncOk = true;
	_lastSyncMs = now;
	_immediate = false;
	_nextSyncMs = now + period;
}

void GPSManager::onLine(const char* s) {
	if (!s || s[0] != '$') return;
	if (!checksumOK(s)) return;

	// выделим talker+type до первой зап€той
	// пример:$GPRMC,...
	const char* comma = strchr(s, ',');
	if (!comma) return;

	char type[6] = { 0 }; // "GPRMC"
	size_t n = (size_t)(comma - (s + 1));
	if (n > 5) n = 5;
	memcpy(type, s + 1, n);
	type[n] = 0;

	// копируем строку без '$' и без checksum части в локальный буфер дл€ strtok
	char buf[LINE_MAX];
	strncpy(buf, s + 1, sizeof(buf) - 1);
	buf[sizeof(buf) - 1] = 0;
	char* star = strchr(buf, '*');
	if (star) *star = 0;

	// токенизаци€
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

	// type sits in fields[0] like "GPRMC"
	if (strcmp(fields[0], "GPRMC") == 0 || strcmp(fields[0], "GNRMC") == 0) parseRMC(fields, nf);
	else if (strcmp(fields[0], "GPGGA") == 0 || strcmp(fields[0], "GNGGA") == 0) parseGGA(fields, nf);
	else if (strcmp(fields[0], "GPGSV") == 0 || strcmp(fields[0], "GNGSV") == 0) parseGSV(fields, nf);
}

bool GPSManager::checksumOK(const char* s) {
	const char* star = strchr(s, '*');
	if (!star || star[1] == 0 || star[2] == 0) return false;

	uint8_t want = hex2u8(star[1], star[2]);
	uint8_t got = 0;
	for (const char* p = s + 1; p < star; p++) got ^= (uint8_t)(*p);
	return got == want;
}

uint8_t GPSManager::hex2u4(char c) {
	if (c >= '0' && c <= '9') return (uint8_t)(c - '0');
	if (c >= 'A' && c <= 'F') return (uint8_t)(c - 'A' + 10);
	if (c >= 'a' && c <= 'f') return (uint8_t)(c - 'a' + 10);
	return 0;
}
uint8_t GPSManager::hex2u8(char hi, char lo) { return (uint8_t)((hex2u4(hi) << 4) | hex2u4(lo)); }

bool GPSManager::parse_hhmmss(const char* s, int& hh, int& mm, int& ss) {
	if (!s || strlen(s) < 6) return false;
	hh = (s[0] - '0') * 10 + (s[1] - '0');
	mm = (s[2] - '0') * 10 + (s[3] - '0');
	ss = (s[4] - '0') * 10 + (s[5] - '0');
	if (hh < 0 || hh>23 || mm < 0 || mm>59 || ss < 0 || ss>59) return false;
	return true;
}

bool GPSManager::parse_ddmmyy(const char* s, int& dd, int& mo, int& yy) {
	if (!s || strlen(s) < 6) return false;
	dd = (s[0] - '0') * 10 + (s[1] - '0');
	mo = (s[2] - '0') * 10 + (s[3] - '0');
	yy = (s[4] - '0') * 10 + (s[5] - '0');
	if (dd < 1 || dd>31 || mo < 1 || mo>12) return false;
	return true;
}

void GPSManager::parseRMC(char** f, int nf) {
	// RMC:[1]=time,[2]=status,[9]=date
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

	// fix flag общий
	gGpsFix = (_rmcValid && (_ggaFixQ > 0));
}

void GPSManager::parseGGA(char** f, int nf) {
	// GGA:[6]=fix quality,[7]=sats,[8]=hdop
	if (nf < 9) return;

	_ggaFixQ = (uint8_t)atoi(f[6]);
	gGpsSatsUsed = (uint8_t)atoi(f[7]);

	// hdop может быть float,берЄм *100
	if (f[8] && f[8][0]) {
		float hd = (float)atof(f[8]);
		if (hd < 0) hd = 0;
		if (hd > 99.99f) hd = 99.99f;
		gGpsHdop_x100 = (uint16_t)(hd * 100.0f + 0.5f);
	}

	gGpsLastGgaMs = millis();
	gGpsFix = (_rmcValid && (_ggaFixQ > 0));
}

void GPSManager::parseGSV(char** f, int nf) {
	// GSV:[3]=total sats in view,затем группы по 4 пол€,SNR в каждом 4-м
	if (nf < 4) return;

	gGpsSatsView = (uint8_t)atoi(f[3]);

	// пересобираем SNR из этого предложени€
	_snrSum = 0;
	_snrCnt = 0;
	_snrMax = 0;

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

	if (_snrCnt > 0) gGpsSnrAvg = (uint8_t)(_snrSum / _snrCnt);
	else gGpsSnrAvg = 0;

	gGpsSnrMax = _snrMax;
	gGpsLastGsvMs = millis();
}

// ---- RTC helpers ----
uint8_t GPSManager::dow0_sun(int y, int m, int d) {
	static int t[] = { 0,3,2,5,0,3,5,1,4,6,2,4 };
	if (m < 3) y -= 1;
	return (uint8_t)((y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7);
}
uint8_t GPSManager::ds3231DowFromDow0(uint8_t dow0) {
	return (dow0 == 0) ? 7 : dow0;
}

int64_t GPSManager::toEpochSeconds(int y, int mo, int d, int h, int mi, int s) {
	// same civil algorithm used earlier (Howard Hinnant),embedded here for locality
	auto daysFromCivil = [](int y, int m, int d)->int32_t {
		y -= (m <= 2);
		const int era = (y >= 0 ? y : y - 399) / 400;
		const unsigned yoe = (unsigned)(y - era * 400);
		const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + (unsigned)d - 1;
		const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
		return (int32_t)(era * 146097 + (int)doe - 719468);
	};
	int32_t days = daysFromCivil(y, mo, d);
	return (int64_t)days * 86400LL + (int64_t)h * 3600LL + (int64_t)mi * 60LL + (int64_t)s;
}

void GPSManager::fromEpochSeconds(int64_t t, int& y, int& mo, int& d, int& h, int& mi, int& s) {
	auto civilFromDays = [](int32_t z, int& y, int& m, int& d) {
		z += 719468;
		const int era = (z >= 0 ? z : z - 146096) / 146097;
		const unsigned doe = (unsigned)(z - era * 146097);
		const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
		y = (int)yoe + era * 400;
		const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
		const unsigned mp = (5 * doy + 2) / 153;
		d = (int)(doy - (153 * mp + 2) / 5 + 1);
		m = (int)(mp + (mp < 10 ? 3 : -9));
		y += (m <= 2);
	};
	int64_t days = t / 86400LL;
	int64_t rem = t % 86400LL;
	if (rem < 0) { rem += 86400LL; days -= 1; }
	civilFromDays((int32_t)days, y, mo, d);
	h = (int)(rem / 3600LL); rem %= 3600LL;
	mi = (int)(rem / 60LL);
	s = (int)(rem % 60LL);
}

void GPSManager::utcToLocal(int& y, int& mo, int& d, int& h, int& mi, int& s, int tzQuarterHours) {
	const int offsetMin = tzQuarterHours * 15;
	int64_t epochUTC = toEpochSeconds(y, mo, d, h, mi, s);
	int64_t epochLocal = epochUTC + (int64_t)offsetMin * 60LL;
	fromEpochSeconds(epochLocal, y, mo, d, h, mi, s);
}