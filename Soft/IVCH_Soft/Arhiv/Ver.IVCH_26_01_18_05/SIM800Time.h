#pragma once
#include <Arduino.h>

class SIM800TimeClient {
public:
	SIM800TimeClient(HardwareSerial& serial, uint8_t pwrkeyPin, bool pwrkeyActiveHigh = true);

	void begin(uint32_t baud);

	// ¬ключить вывод логов:
	// dbg = &Serial (или другой Stream)
	// logAT=true печатает все AT и ответы
	void setDebug(Stream* dbg, bool logAT = true);

	bool ensureOnSmart(uint32_t atProbeTimeoutMs = 1500,
		uint16_t pwrkeyPressMs = 1200,
		uint32_t urcWaitMs = 8000,
		uint32_t atReadyTimeoutMs = 12000);

	bool init(uint32_t atReadyTimeoutMs = 12000);
	bool waitForNetwork(uint32_t timeoutMs = 60000);

	bool getCCLK(String& rawResp, uint32_t timeoutMs = 1500);

	static bool parseCCLK(const String& resp,
		int& year, int& month, int& day,
		int& hour, int& minute, int& second,
		int& tzQuarterHours);

	static bool isCCLKValid(const String& resp, int minYear = 2020);

	bool syncTimeWithNTP(const char* apn,
		const char* user,
		const char* pass,
		const char* ntpServer,
		int tzHours,
		uint32_t bearerOpenTimeoutMs = 45000,
		uint32_t ntpTimeoutMs = 20000);

	bool getTimeSmart(String& rawResp,
		const char* apn,
		const char* user,
		const char* pass,
		const char* ntpServer,
		int tzHours,
		bool forceNtpIfInvalid = true);

private:
	HardwareSerial& _sim;
	uint8_t _pwrkeyPin;
	bool _pwrkeyActiveHigh;

	Stream* _dbg = nullptr;
	bool _logAT = true;

	bool _seenRDY = false;
	bool _seenCallReady = false;
	bool _seenSMSReady = false;

	void _dbgln(const String& s);
	void _dbgln(const __FlashStringHelper* s);
	void _dbgKV(const __FlashStringHelper* k, const String& v);

	void _setPWRKEYIdle();
	void _setPWRKEYPressed();
	void _pulsePWRKEY(uint16_t pressMs, uint16_t preDelayMs);

	void _resetURCFlags();
	void _drainInput(uint32_t ms);
	void _pollURC(uint32_t ms);

	String _readAll(uint32_t timeoutMs);
	bool _sendAT(const char* cmd, const char* expect, uint32_t timeoutMs, String* out = nullptr);
	bool _waitATReady(uint32_t timeoutMs);

	bool _bearerClose();
	bool _bearerOpen(const char* apn, const char* user, const char* pass, uint32_t timeoutMs);
	bool _waitCGATT(uint32_t timeoutMs = 30000);
	bool _ntpRequest(const char* server, int tzHours, uint32_t timeoutMs);
};