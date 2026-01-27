#include "TimeSyncPlanner.h"

#include <IPAddress.h>
#include <Ethernet_Generic.hpp>
#include <EthernetUdp.h>

extern void pumpUiButtons();

// SD logger (adds timestamp itself in SdLogLine)
extern void SdLogLine(const char* s);

// ---------- LED pulse (реализовать в .ino) ----------
// src:1=GPS (LED1 PE13),2=INTERNET1 (LED2 PE12),3=INTERNET2 (LED3 PE11),4=GSM (LED4 PB0)
extern void pulseSyncLed(uint8_t src);
static constexpr uint8_t SYNC_LED_GPS = 1;
static constexpr uint8_t SYNC_LED_NET1 = 2;
static constexpr uint8_t SYNC_LED_NET2 = 3;
// GSM LED (4) зажигается в .ino там,где вы делаете rtc.setTime() по SIM800

// ---------- TIME LOG (unified format) ----------
// Format examples:
// TIME GPS REQ
// TIME GPS OK
// TIME GPS FAIL no_fix
// TIME GSM SKIP disabled
static inline void timeLog(const char* src, const char* evt, const char* detail = nullptr)
{
	char line[96];
	if (detail && detail[0])
		snprintf(line, sizeof(line), "TIME %s %s %s", src, evt, detail);
	else
		snprintf(line, sizeof(line), "TIME %s %s", src, evt);
	SdLogLine(line);
}

// Rate limit for chatty SKIP states,to avoid SD spam
static inline bool timeLogAllowEvery(uint32_t& lastMs, uint32_t now, uint32_t periodMs)
{
	if (lastMs == 0 || (now - lastMs) >= periodMs) {
		lastMs = now;
		return true;
	}
	return false;
}

// ---------- NTP over Ethernet (INTERNET1 / W5500 local) ----------
static const IPAddress kNtpServers[5] = {
	IPAddress(162,159,200,123),
	IPAddress(162,159,200,1),
	IPAddress(129,6,15,28),
	IPAddress(132,163,96,1),
	IPAddress(216,239,35,0)
};

static bool isZeroIP(const IPAddress& ip) {
	return (ip[0] == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] == 0);
}

// Блокирующий NTP-запрос (редко вызывается,по расписанию),таймаут ~250мс.
static bool ntpQueryUnixUtc(uint8_t ntpIdx, uint32_t& outUnixUtc, uint16_t localPort = 8888, uint32_t timeoutMs = 250)
{
	if (ntpIdx >= 5) ntpIdx %= 5;
	const IPAddress server = kNtpServers[ntpIdx];

	// Если Ethernet ещё не поднят — даже не пробуем
	IPAddress lip = Ethernet.localIP();
	if (isZeroIP(lip)) return false;

	static EthernetUDP udp;
	static bool udpBegun = false;
	if (!udpBegun) {
		udpBegun = udp.begin(localPort);
		if (!udpBegun) return false;
	}

	uint8_t pkt[48];
	memset(pkt, 0, sizeof(pkt));
	// LI=3 (unknown),VN=4,Mode=3 (client) => 0xE3 (типовой)
	pkt[0] = 0xE3;
	pkt[1] = 0; // stratum
	pkt[2] = 6; // poll
	pkt[3] = 0xEC; // precision

	// отправка
	if (udp.beginPacket(server, 123) != 1) return false;
	udp.write(pkt, sizeof(pkt));
	if (udp.endPacket() != 1) return false;

	// ожидание ответа
	const uint32_t t0 = millis();
	while ((millis() - t0) < timeoutMs)
	{
		int psize = udp.parsePacket();
		if (psize >= 48) {
			uint8_t r[48];
			int n = udp.read(r, 48);
			if (n < 48) return false;

			// Transmit Timestamp (seconds since 1900) at byte 40..43
			uint32_t secs1900 =
				((uint32_t)r[40] << 24) |
				((uint32_t)r[41] << 16) |
				((uint32_t)r[42] << 8) |
				((uint32_t)r[43]);

			// 1900->1970 offset
			if (secs1900 < 2208988800UL) return false;
			outUnixUtc = secs1900 - 2208988800UL;
			return true;
		}

		pumpUiButtons();
		//delay(2);
	}
	return false;
}

// ================== ctor ==================
TimeSyncPlanner::TimeSyncPlanner(SIM800TimeAsync& sim,
	GPSNmeaParser& gps,
	RealtimeClock& rtc,
	AT24C128Settings::Config& cfg,
	SyncSourcesStore::Data& src,
	Internet2Client* internet2)
	:_sim(sim), _gps(gps), _rtc(rtc), _cfg(cfg), _src(src), _internet2(internet2)
{}

uint32_t TimeSyncPlanner::periodMsFromIdx(uint8_t idx) {
	static const uint32_t kPeriodsMs[6] = {
		60000UL,600000UL,1800000UL,3600000UL,21600000UL,43200000UL
	};
	return kPeriodsMs[idx % 6];
}

uint32_t TimeSyncPlanner::catchUpNext(uint32_t now, uint32_t next, uint32_t period) {
	if (period == 0) return now + 60000UL;
	if ((int32_t)(now - next) < 0) return next;
	uint32_t diff = now - next;
	uint32_t steps = diff / period + 1;
	return next + steps * period;
}

void TimeSyncPlanner::begin() {
	uint32_t now = millis();
	_nextGpsMs = now + periodMsFromIdx(_src.gpsPeriodIdx);

	// INTERNET1 (local W5500 NTP)
	_nextNetMs = now + periodMsFromIdx(_src.netPeriodIdx);

	// GSM CCLK
	_nextGsmMs = now + periodMsFromIdx(_src.gsmPeriodIdx);

	// INTERNET2 (I2C module)
	_nextNet2Ms = now + periodMsFromIdx(_src.net2PeriodIdx);

	_nextPushNet2Ms = now + 2000;
}

void TimeSyncPlanner::onSettingsChanged() {
	uint32_t now = millis();
	_nextGpsMs = catchUpNext(now, _nextGpsMs, periodMsFromIdx(_src.gpsPeriodIdx));
	_nextNetMs = catchUpNext(now, _nextNetMs, periodMsFromIdx(_src.netPeriodIdx));
	_nextGsmMs = catchUpNext(now, _nextGsmMs, periodMsFromIdx(_src.gsmPeriodIdx));
	_nextNet2Ms = catchUpNext(now, _nextNet2Ms, periodMsFromIdx(_src.net2PeriodIdx));
}

void TimeSyncPlanner::onTimeUpdated() {
	_nextPushNet2Ms = millis();
}

void TimeSyncPlanner::triggerImmediate() { _immediatePending = true; }

// ---------- main tick ----------
void TimeSyncPlanner::tick() {
	uint32_t now = millis();

	// push DS3231 time to Internet2 every 2s
	tickPushTimeToInternet2(now);

	// Если ждём результат Internet2 — опрашиваем статус
	if (_internet2 && _net2Pending) {
		Internet2Client::Status st;
		if (_internet2->readStatus(st)) {
			if (st.lastSyncOk && st.lastNtpUtc != 0 && st.lastNtpUtc != _net2LastSeenNtpUtc) {
				_net2LastSeenNtpUtc = st.lastNtpUtc;

				// Запись времени в DS3231 (INTERNET2)
				setRtcFromUnixUtc(st.lastNtpUtc);
				pulseSyncLed(SYNC_LED_NET2); // LED3 на 500 мс

				timeLog("NET2", "OK");

				_net2Pending = false;
				onTimeUpdated();
				return;
			}
		}
		if (now - _net2ReqStartedMs > 3000) {
			timeLog("NET2", "FAIL", "timeout");
			_net2Pending = false;
		}
	}

	// Немедленный запуск (один раз) по приоритету:
	// GPS -> INTERNET1(W5500) -> INTERNET2(I2C) -> GSM
	if (_immediatePending) {
		if (tryStartGps(now, true) || tryStartNet(now, true) || tryStartNet2(now, true) || tryStartGsm(now, true)) {
			_immediatePending = false;
		}
		return;
	}

	// Обычное расписание (тот же приоритет):
	(void)tryStartGps(now, false);
	if (tryStartNet(now, false)) return; // INTERNET1 (W5500 local)
	if (tryStartNet2(now, false)) return; // INTERNET2 (I2C)
	(void)tryStartGsm(now, false); // GSM CCLK (rtc.setTime делается у вас в .ino по событию SIM)
}

// ---------- GPS ----------
bool TimeSyncPlanner::tryStartGps(uint32_t now, bool immediate) {
	// rate-limited SKIP state
	static uint32_t sSkipDisabledMs = 0;

	if (!_src.gpsEnable) {
		if (timeLogAllowEvery(sSkipDisabledMs, now, 10000UL)) timeLog("GPS", "SKIP", "disabled");
		return false;
	}

	uint32_t period = periodMsFromIdx(_src.gpsPeriodIdx);
	if (!immediate && (int32_t)(now - _nextGpsMs) < 0) {
		if (immediate) timeLog("GPS", "SKIP", "not_due");
		return false;
	}

	_nextGpsMs = immediate ? (now + period) : catchUpNext(now, _nextGpsMs, period);

	timeLog("GPS", "REQ");

	int y, mo, d, h, mi, s;
	if (!_gps.getUtc(y, mo, d, h, mi, s)) { timeLog("GPS", "FAIL", "no_utc"); return false; }
	if (!_gps.hasFix()) { timeLog("GPS", "FAIL", "no_fix"); return false; }

	int tzq_target = (int)_cfg.tzTargetHours * 4;
	utcToLocal(y, mo, d, h, mi, s, tzq_target);

	uint8_t dsDow = ds3231DowFromDow0(dow0_sun(y, mo, d));
	_rtc.setTime((uint8_t)s, (uint8_t)mi, (uint8_t)h, dsDow, (uint8_t)d, (uint8_t)mo, (uint16_t)y);

	// LED1 на 500 мс (GPS + запись DS3231)
	pulseSyncLed(SYNC_LED_GPS);

	timeLog("GPS", "OK");
	return true;
}

// ---------- INTERNET1 (W5500 local NTP) ----------
// ВНИМАНИЕ:функция называется tryStartNet() по историческим причинам,
// но теперь это INTERNET1,а не SIM800 NTP.
bool TimeSyncPlanner::tryStartNet(uint32_t now, bool immediate) {
	static uint32_t sSkipDisabledMs = 0;

	if (!_src.netEnable) {
		if (timeLogAllowEvery(sSkipDisabledMs, now, 10000UL)) timeLog("NET1", "SKIP", "disabled");
		return false;
	}

	uint32_t period = periodMsFromIdx(_src.netPeriodIdx);
	if (!immediate && (int32_t)(now - _nextNetMs) < 0) {
		if (immediate) timeLog("NET1", "SKIP", "not_due");
		return false;
	}

	// заранее сдвигаем расписание,чтобы не долбить в каждом tick
	_nextNetMs = immediate ? (now + period) : catchUpNext(now, _nextNetMs, period);

	timeLog("NET1", "REQ");

	uint32_t unixUtc = 0;
	bool ok = ntpQueryUnixUtc(_src.netProviderIdx, unixUtc, 8888, 250);
	if (!ok || unixUtc == 0) {
		timeLog("NET1", "FAIL", "query");
		// быстрый повтор через минуту (иначе при period=1 час ждать очень долго)
		_nextNetMs = now + 60000UL;
		return false;
	}

	// Запись времени в DS3231 (INTERNET1)
	setRtcFromUnixUtc(unixUtc);
	pulseSyncLed(SYNC_LED_NET1); // LED2 на 500 мс

	timeLog("NET1", "OK");

	onTimeUpdated();
	return true;
}

// ---------- GSM (SIM800 CCLK) ----------
bool TimeSyncPlanner::tryStartGsm(uint32_t now, bool immediate) {
	static uint32_t sSkipDisabledMs = 0;

	if (!_src.gsmEnable) {
		if (timeLogAllowEvery(sSkipDisabledMs, now, 10000UL)) timeLog("GSM", "SKIP", "disabled");
		return false;
	}

	// EXPLICIT:do NOT log TIME GSM SKIP busy
	if (_sim.isBusy()) return false;

	uint32_t period = periodMsFromIdx(_src.gsmPeriodIdx);
	if (!immediate && (int32_t)(now - _nextGsmMs) < 0) {
		if (immediate) timeLog("GSM", "SKIP", "not_due");
		return false;
	}

	_nextGsmMs = immediate ? (now + period) : catchUpNext(now, _nextGsmMs, period);

	timeLog("GSM", "REQ");
	_sim.requestTimeNow();
	return true;
}

// ---------- INTERNET2 (I2C module) ----------
bool TimeSyncPlanner::tryStartNet2(uint32_t now, bool immediate) {
	static uint32_t sSkipNoDevMs = 0;
	static uint32_t sSkipDisabledMs = 0;
	static uint32_t sSkipPendingMs = 0;

	if (!_internet2) {
		if (timeLogAllowEvery(sSkipNoDevMs, now, 10000UL)) timeLog("NET2", "SKIP", "no_dev");
		return false;
	}
	if (!_src.net2Enable) {
		if (timeLogAllowEvery(sSkipDisabledMs, now, 10000UL)) timeLog("NET2", "SKIP", "disabled");
		return false;
	}

	if (_net2Pending) {
		if (timeLogAllowEvery(sSkipPendingMs, now, 10000UL)) timeLog("NET2", "SKIP", "pending");
		return false;
	}

	uint32_t period = periodMsFromIdx(_src.net2PeriodIdx);
	if (!immediate && (int32_t)(now - _nextNet2Ms) < 0) {
		if (immediate) timeLog("NET2", "SKIP", "not_due");
		return false;
	}

	_nextNet2Ms = immediate ? (now + period) : catchUpNext(now, _nextNet2Ms, period);

	timeLog("NET2", "REQ");
	_internet2->requestSyncNow();

	Internet2Client::Status st;
	if (_internet2->readStatus(st)) _net2LastSeenNtpUtc = st.lastNtpUtc;

	_net2Pending = true;
	_net2ReqStartedMs = now;
	return true;
}

// ---------- DS3231(local) <-> unix UTC ----------
uint32_t TimeSyncPlanner::rtcLocalToUnixUtc(const RTCTime& t) const {
	int64_t local = toEpochSeconds((int)t.year, (int)t.month, (int)t.dayOfMonth, (int)t.hour, (int)t.minute, (int)t.second);
	int64_t utc = local - (int64_t)_cfg.tzTargetHours * 3600LL;
	if (utc < 0) utc = 0;
	return (uint32_t)utc;
}

void TimeSyncPlanner::setRtcFromUnixUtc(uint32_t unixUtc) {
	int64_t local = (int64_t)unixUtc + (int64_t)_cfg.tzTargetHours * 3600LL;

	int y, mo, d, h, mi, s;
	fromEpochSeconds(local, y, mo, d, h, mi, s);

	uint8_t dsDow = ds3231DowFromDow0(dow0_sun(y, mo, d));
	_rtc.setTime((uint8_t)s, (uint8_t)mi, (uint8_t)h, dsDow, (uint8_t)d, (uint8_t)mo, (uint16_t)y);
}

void TimeSyncPlanner::tickPushTimeToInternet2(uint32_t now) {
	if (!_internet2) return;

	if ((int32_t)(now - _nextPushNet2Ms) < 0) return;
	_nextPushNet2Ms = now + 2000;

	RTCTime tLocal = _rtc.getTime();
	uint32_t unixUtc = rtcLocalToUnixUtc(tLocal);

	_internet2->setTimeUnixUtc(unixUtc, 0);
}

// ---- date/time helpers ----
uint8_t TimeSyncPlanner::dow0_sun(int y, int m, int d) {
	static int t[] = { 0,3,2,5,0,3,5,1,4,6,2,4 };
	if (m < 3) y -= 1;
	return (uint8_t)((y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7);
}
uint8_t TimeSyncPlanner::ds3231DowFromDow0(uint8_t dow0) { return (dow0 == 0) ? 7 : dow0; }

int64_t TimeSyncPlanner::toEpochSeconds(int y, int mo, int d, int h, int mi, int s) {
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

void TimeSyncPlanner::fromEpochSeconds(int64_t t, int& y, int& mo, int& d, int& h, int& mi, int& s) {
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

void TimeSyncPlanner::utcToLocal(int& y, int& mo, int& d, int& h, int& mi, int& s, int tzQuarterHours) {
	const int offsetMin = tzQuarterHours * 15;
	int64_t epochUTC = toEpochSeconds(y, mo, d, h, mi, s);
	int64_t epochLocal = epochUTC + (int64_t)offsetMin * 60LL;
	fromEpochSeconds(epochLocal, y, mo, d, h, mi, s);
}

