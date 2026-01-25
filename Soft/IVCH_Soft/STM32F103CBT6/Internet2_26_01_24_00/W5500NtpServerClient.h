#pragma once
#include <Arduino.h>

class W5500NtpServerClient {
public:
	struct NetCfg {
		uint8_t dhcp = 1;
		uint8_t ip[4] = { 192,168,75,232 };
		uint8_t mask[4] = { 255,255,255,0 };
		uint8_t gw[4] = { 192,168,75,1 };
		uint8_t dns[4] = { 8,8,8,8 };

		uint8_t upstream[4] = { 162,159,200,123 }; // внешний NTP IP
		uint32_t periodMs = 3600000UL;
	};

	struct Status {
		bool linkUp = false;
		bool dhcp = true;
		bool haveMasterTime = false;
		bool syncInProgress = false;

		uint8_t ip[4] = { 0,0,0,0 };

		bool lastSyncOk = false;
		uint32_t lastNtpUtc = 0;
		int32_t lastOffsetSec = 0;
		uint32_t lastSyncMs = 0;

		// --- NEW:for supervision/debug over I2C (packed into status bytes) ---
		uint8_t netReinitCount = 0; // how many times we re-inited Ethernet/UDP
		uint8_t lastErr = 0; // 0=ok,1=no-ip-too-long,2=dhcp-fail,3=manual-reinit,4=link-flap
	};

	void begin();
	void tick();

	void applyNetCfg(const NetCfg& cfg);
	void requestSyncNow();

	// from master:unix utc + ms (0..999)
	void setMasterTimeUtc(uint32_t unixUtc, uint16_t ms);

	// NEW:master can request network re-init (actual heavy work happens in tick())
	void requestNetReinit();

	// for status/debug
	uint32_t nowUtcSeconds() const;

	Status status() const { return _st; }

	// UART clients
	void printNowTo(Stream& s) const;

private:
	NetCfg _cfg{};
	mutable Status _st{};

	// base time from master
	uint32_t _baseUnix = 0; // seconds
	uint32_t _baseMillis = 0; // millis() at base
	uint16_t _baseMs = 0; // 0..999 ms inside current second at baseMillis moment

	uint32_t _nextSyncMs = 0;
	bool _syncReq = false;

	// internals
	void ethBegin();
	bool linkUp() const;

	void tickNtpServer();
	void tickNtpClient();

	uint32_t syncUpstreamOnce(); // returns unix utc or 0

	// --- точное время:секунды + дробь ---
	void nowUtcSecFrac(uint32_t& unixSec, uint32_t& ntpFrac) const;
	static uint32_t msToNtpFrac(uint16_t ms);

	// --- Non-blocking upstream NTP sync FSM ---
	enum SyncState :uint8_t { SYNC_IDLE = 0, SYNC_WAIT = 1 };
	SyncState _syncState = SYNC_IDLE;
	uint32_t _syncDeadlineMs = 0;
	uint8_t _syncRetriesLeft = 0;

	void startUpstreamSync(); // отправить запрос (не блокирует)
	void pollUpstreamSync(); // опросить ответ/таймаут (не блокирует)
	void finishUpstreamSync(bool ok, uint32_t ntpUtc);

	// --- NEW:self-heal supervision ---
	bool _wantNetReinit = false;
	uint32_t _linkUpSinceMs = 0;
	uint32_t _lastGoodIpMs = 0;
	uint32_t _lastReinitMs = 0;
	uint8_t _dhcpFailStreak = 0;

	static bool ipValid(const uint8_t ip[4]);
	void doNetReinit(uint8_t errCode);
};