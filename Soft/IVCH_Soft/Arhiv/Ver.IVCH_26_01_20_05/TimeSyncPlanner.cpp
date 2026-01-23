#include "TimeSyncPlanner.h"

TimeSyncPlanner::TimeSyncPlanner(SIM800TimeAsync& sim,
	GPSNmeaParser& gps,
	RealtimeClock& rtc,
	AT24C128Settings::Config& cfg,
	SyncSourcesStore::Data& src)
	:_sim(sim), _gps(gps), _rtc(rtc), _cfg(cfg), _src(src) {}

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
	_nextNetMs = now + periodMsFromIdx(_src.netPeriodIdx);
	_nextGsmMs = now + periodMsFromIdx(_src.gsmPeriodIdx);
}

void TimeSyncPlanner::onSettingsChanged() {
	uint32_t now = millis();
	_nextGpsMs = catchUpNext(now, _nextGpsMs, periodMsFromIdx(_src.gpsPeriodIdx));
	_nextNetMs = catchUpNext(now, _nextNetMs, periodMsFromIdx(_src.netPeriodIdx));
	_nextGsmMs = catchUpNext(now, _nextGsmMs, periodMsFromIdx(_src.gsmPeriodIdx));
}

void TimeSyncPlanner::onTimeUpdated() {
	// пока ничего (точка расширени€)
}

void TimeSyncPlanner::triggerImmediate() { _immediatePending = true; }

void TimeSyncPlanner::tick() {
	uint32_t now = millis();

	// Ќемедленный запуск (один раз) по приоритету
	if (_immediatePending) {
		if (tryStartGps(now, true) || tryStartNet(now, true) || tryStartGsm(now, true)) {
			_immediatePending = false;
		}
		return;
	}

	// ќбычное расписание:GPS -> NET -> GSM
	(void)tryStartGps(now, false);
	if (tryStartNet(now, false)) return;
	(void)tryStartGsm(now, false);
}

// ---- GPS:ставит RTC сам (UTC->local target) ----
bool TimeSyncPlanner::tryStartGps(uint32_t now, bool immediate) {
	if (!_src.gpsEnable) return false;

	uint32_t period = periodMsFromIdx(_src.gpsPeriodIdx);
	if (!immediate && (int32_t)(now - _nextGpsMs) < 0) return false;

	// Удогон€емФ
	_nextGpsMs = immediate ? (now + period) : catchUpNext(now, _nextGpsMs, period);

	// Ќужно валидное UTC
	int y, mo, d, h, mi, s;
	if (!_gps.getUtc(y, mo, d, h, mi, s)) return false;

	// “ребуем фиксацию,если она есть (иначе можно разрешить по RMC:на ваше усмотрение)
	if (!_gps.hasFix()) return false;

	// UTC->local target
	int tzq_target = (int)_cfg.tzTargetHours * 4;
	utcToLocal(y, mo, d, h, mi, s, tzq_target);

	uint8_t dsDow = ds3231DowFromDow0(dow0_sun(y, mo, d));
	_rtc.setTime((uint8_t)s, (uint8_t)mi, (uint8_t)h, dsDow, (uint8_t)d, (uint8_t)mo, (uint16_t)y);

	return true;
}

// ---- Internet:запускаем SIM NTP (RTC ставите в вашем sim.hasNewTime()) ----
bool TimeSyncPlanner::tryStartNet(uint32_t now, bool immediate) {
	if (!_src.netEnable) return false;
	if (_sim.isBusy()) return false;

	uint32_t period = periodMsFromIdx(_src.netPeriodIdx);
	if (!immediate && (int32_t)(now - _nextNetMs) < 0) return false;

	_nextNetMs = immediate ? (now + period) : catchUpNext(now, _nextNetMs, period);

	_sim.requestNtpNow();
	return true;
}

// ---- GSM:запускаем SIM CCLK (RTC ставите в вашем sim.hasNewTime()) ----
bool TimeSyncPlanner::tryStartGsm(uint32_t now, bool immediate) {
	if (!_src.gsmEnable) return false;
	if (_sim.isBusy()) return false;

	uint32_t period = periodMsFromIdx(_src.gsmPeriodIdx);
	if (!immediate && (int32_t)(now - _nextGsmMs) < 0) return false;

	_nextGsmMs = immediate ? (now + period) : catchUpNext(now, _nextGsmMs, period);

	_sim.requestTimeNow();
	return true;
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