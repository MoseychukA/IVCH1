#pragma once
#include <Arduino.h>

#include "SIM800TimeAsync.h"
#include "AT24C128Settings.h"
#include "SyncSourcesStore.h"

// ѕланировщик синхронизации времени по источникам (GPS > Internet > GSM).
// - GPS пока не реализован:просто "пропускаетс€",но расписание ведетс€.
// - Internet:принудительный NTP (AT+CNTP),затем CCLK и запись в RTC (у вас в .ino).
// - GSM:только CCLK (NITZ/RTC модема).
//
// "ƒогон€ть пропущенное":если врем€ запуска уже прошло,следующий запуск переноситс€ вперед,
// и выполн€етс€ не сери€ запусков,а один.
class TimeSyncPlanner {
public:
	TimeSyncPlanner(SIM800TimeAsync& sim,
		AT24C128Settings& eeCfg,
		AT24C128Settings::Config& cfg,
		SyncSourcesStore& srcStore,
		SyncSourcesStore::Data& src);

	void begin();

	// ¬ызывать в loop() часто
	void tick();

	// ¬ызывать после изменени€ настроек из меню (после SAVE)
	void onSettingsChanged();

	// ¬ызывать при получении нового времени от SIM800 (после sim.hasNewTime())
	void onTimeUpdated();

	const char* lastAction() const { return _lastAction; }
	uint32_t lastActionMs() const { return _lastActionMs; }

private:
	SIM800TimeAsync& _sim;
	AT24C128Settings& _eeCfg;
	AT24C128Settings::Config& _cfg;
	SyncSourcesStore& _srcStore;
	SyncSourcesStore::Data& _src;

	uint32_t _nextGpsMs = 0;
	uint32_t _nextNetMs = 0;
	uint32_t _nextGsmMs = 0;

	bool _netInFlight = false;
	bool _gsmInFlight = false;

	// GPS пока не реализован
	bool _gpsImplemented = false;

	const char* _lastAction = "none";
	uint32_t _lastActionMs = 0;

private:
	static uint32_t periodMsFromIdx(uint8_t idx);

	// переносит next в будущее (>now) с учетом period
	static uint32_t catchUpNext(uint32_t now, uint32_t next, uint32_t period);

	void applyNtpConfig(bool enableFallback);

	bool due(uint32_t now, uint32_t next) const { return (int32_t)(now - next) >= 0; }

	bool tryStartGps(uint32_t now);
	bool tryStartNet(uint32_t now);
	bool tryStartGsm(uint32_t now);
};