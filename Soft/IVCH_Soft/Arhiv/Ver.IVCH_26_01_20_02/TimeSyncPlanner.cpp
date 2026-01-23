#include "TimeSyncPlanner.h"

TimeSyncPlanner::TimeSyncPlanner(SIM800TimeAsync& sim,
	AT24C128Settings& eeCfg,
	AT24C128Settings::Config& cfg,
	SyncSourcesStore& srcStore,
	SyncSourcesStore::Data& src)
	:_sim(sim), _eeCfg(eeCfg), _cfg(cfg), _srcStore(srcStore), _src(src) {}

uint32_t TimeSyncPlanner::periodMsFromIdx(uint8_t idx)
{
	// индексы должны совпадать с меню:1м,10м,30м,1ч,6ч,12ч
	static const uint32_t kPeriodsMs[6] = {
	60000UL,
	600000UL,
	1800000UL,
	3600000UL,
	21600000UL,
	43200000UL
	};
	return kPeriodsMs[idx % 6];
}

uint32_t TimeSyncPlanner::catchUpNext(uint32_t now, uint32_t next, uint32_t period)
{
	if (period == 0) return now + 60000UL;
	if ((int32_t)(now - next) < 0) return next;

	// next уже в прошлом:пересчитать так,чтобы next стало > now
	uint32_t diff = now - next;
	uint32_t steps = diff / period + 1;
	return next + steps * period;
}

void TimeSyncPlanner::applyNtpConfig(bool enableFallback)
{
	// NTP параметры берём из cfg (server/apn/user/pass/tzNtpHours)
	SIM800TimeAsync::NtpConfig ntp;
	ntp.apn = _cfg.apn;
	ntp.user = _cfg.user;
	ntp.pass = _cfg.pass;
	ntp.server = _cfg.server;
	ntp.tzHours = _cfg.tzNtpHours; // обычно 0 (UTC)
	ntp.enableFallback = enableFallback;
	_sim.setNtpConfig(ntp);
}

void TimeSyncPlanner::begin()
{
	const uint32_t now = millis();

	_nextGpsMs = now + periodMsFromIdx(_src.gpsPeriodIdx);
	_nextNetMs = now + periodMsFromIdx(_src.netPeriodIdx);
	_nextGsmMs = now + periodMsFromIdx(_src.gsmPeriodIdx);

	// Рекомендация:чтобы SIM800TimeAsync сам не запускал периодический опрос времени,
	// можно "задвинуть" его период далеко:
	// _sim.setPeriodMs(86400000UL); // 1 сутки
}

void TimeSyncPlanner::onSettingsChanged()
{
	// когда пользователь меняет периоды,важно не получить "лавину" запусков
	const uint32_t now = millis();
	_nextGpsMs = catchUpNext(now, _nextGpsMs, periodMsFromIdx(_src.gpsPeriodIdx));
	_nextNetMs = catchUpNext(now, _nextNetMs, periodMsFromIdx(_src.netPeriodIdx));
	_nextGsmMs = catchUpNext(now, _nextGsmMs, periodMsFromIdx(_src.gsmPeriodIdx));
}

void TimeSyncPlanner::onTimeUpdated()
{
	_netInFlight = false;
	_gsmInFlight = false;
}

bool TimeSyncPlanner::tryStartGps(uint32_t now)
{
	if (!_src.gpsEnable) return false;
	if (!due(now, _nextGpsMs)) return false;

	const uint32_t p = periodMsFromIdx(_src.gpsPeriodIdx);
	_nextGpsMs = catchUpNext(now, _nextGpsMs, p);

	if (!_gpsImplemented) {
		_lastAction = "gps(skip)";
		_lastActionMs = now;
		return false; // не блокируем,даём Internet/GSM продолжить
	}

	// Когда появится GPS:запуск тут.
	return false;
}

bool TimeSyncPlanner::tryStartNet(uint32_t now)
{
	if (!_src.netEnable) return false;
	if (!due(now, _nextNetMs)) return false;
	if (_sim.isBusy()) return false;

	const uint32_t p = periodMsFromIdx(_src.netPeriodIdx);
	_nextNetMs = catchUpNext(now, _nextNetMs, p);

	applyNtpConfig(true);

	// Internet источник = принудительный NTP
	_sim.requestNtpNow();
	_netInFlight = true;

	_lastAction = "internet(ntp)";
	_lastActionMs = now;
	return true;
}

bool TimeSyncPlanner::tryStartGsm(uint32_t now)
{
	if (!_src.gsmEnable) return false;
	if (!due(now, _nextGsmMs)) return false;
	if (_sim.isBusy()) return false;

	const uint32_t p = periodMsFromIdx(_src.gsmPeriodIdx);
	_nextGsmMs = catchUpNext(now, _nextGsmMs, p);

	// GSM источник = CCLK (без NTP)
	applyNtpConfig(false);

	_sim.requestTimeNow();
	_gsmInFlight = true;

	_lastAction = "gsm(cclk)";
	_lastActionMs = now;
	return true;
}

void TimeSyncPlanner::tick()
{
	const uint32_t now = millis();

	// Приоритет:GPS > Internet > GSM
	(void)tryStartGps(now);

	if (tryStartNet(now)) return;
	(void)tryStartGsm(now);
}