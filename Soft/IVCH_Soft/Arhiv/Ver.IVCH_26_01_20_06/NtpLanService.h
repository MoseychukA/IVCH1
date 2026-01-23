#pragma once
#include <Arduino.h>
#include <Ethernet.h>
#include <EthernetUdp.h>

#include "RTCSupport.h"
#include "AT24C128Settings.h"

class NtpLanService {
public:
	struct NetConfig {
		byte mac[6];

		IPAddress ip;
		IPAddress dns;
		IPAddress gw;
		IPAddress mask;

		// внешний NTP (можно IP,чтобы не зависеть от DNS)
		IPAddress upstreamIp = IPAddress(129, 6, 15, 28); // time.nist.gov
		uint16_t upstreamPort = 123;

		// период синхронизации с интернетом
		uint32_t syncPeriodMs = 3600000UL; // 1 час
	};

	NtpLanService(RealtimeClock& rtc, AT24C128Settings::Config& cfg);

	// csPin = PA4 (ваш SCSn)
	bool begin(const NetConfig& net, uint8_t csPin);

	void tick(); // вызывать часто
	void forceSyncNow(); // немедленно запросить интернет NTP

	bool lastSyncOk() const { return _lastSyncOk; }
	uint32_t lastSyncMs() const { return _lastSyncMs; }

	uint32_t lastAttemptMs() const { return _lastAttemptMs; }
	uint32_t lastSuccessMs() const { return _lastSuccessMs; }

private:
	RealtimeClock& _rtc;
	AT24C128Settings::Config& _cfg;

	EthernetUDP _udpSrv;
	EthernetUDP _udpCli;

	NetConfig _net;
	uint8_t _csPin = 0;

	// NTP server port
	static const uint16_t NTP_PORT = 123;

	// sync state
	bool _syncRequested = false;
	bool _lastSyncOk = false;
	uint32_t _lastSyncMs = 0;
	uint32_t _nextSyncMs = 0;

	uint32_t _lastAttemptMs = 0;
	uint32_t _lastSuccessMs = 0;

	// helpers
	void handleNtpServer();
	void handleSyncClient();

	// time conversions
	static uint32_t unixFromRtcLocal(const RTCTime& t, int8_t tzTargetHours);
	static void rtcLocalFromUnix(uint32_t unixUtc, int8_t tzTargetHours, RTCTime& outLocal);

	// NTP packet helpers
	static void buildNtpResponse(uint8_t* out48,
		uint32_t rxSeconds1900, uint32_t rxFrac,
		uint32_t txSeconds1900, uint32_t txFrac,
		const uint8_t* clientRequest48);

	static uint32_t unixToNtp1900(uint32_t unixUtc) { return unixUtc + 2208988800UL; }
	static uint32_t ntp1900ToUnix(uint32_t ntpSec1900) { return ntpSec1900 - 2208988800UL; }

	static uint32_t millisToFrac(uint32_t ms) {
		// приближение:ms/1000 * 2^32
		return (uint32_t)(((uint64_t)ms << 32) / 1000ULL);
	}
};