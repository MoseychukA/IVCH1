#pragma once
#include <Arduino.h>

class SIM800TimeAsync {
public:
	struct NtpConfig {
		const char* apn = nullptr;
		const char* user = "";
		const char* pass = "";
		const char* server = "pool.ntp.org";
		int tzHours = 0; // для AT+CNTP (в часах)
		bool enableFallback = false;
	};

	SIM800TimeAsync(HardwareSerial& sim, uint8_t pwrkeyPin, bool pwrkeyActiveHigh = true);

	void begin(uint32_t baud);
	void setDebug(Stream* dbg, bool logAT = true);

	void start(); // запустить FSM (не блокирует)
	void tick(); // вызывать часто в loop()

	// Период запроса времени (+CREG/+CCLK)
	void setPeriodMs(uint32_t periodMs);
	void setNtpConfig(const NtpConfig& cfg);
	void requestTimeNow();

	// --- AT ping / recovery ---
	void setHealthPingMs(uint32_t periodMs); // 0=выкл
	void setMaxRecoveryAttempts(uint8_t n); // попытки PWRKEY
	uint32_t lastAtOkMs() const;
	uint8_t recoveryAttemptsUsed() const;

	// --- Status polling for display (CREG/CSQ) ---
	void setStatusPollMs(uint32_t periodMs); // 0=выкл
	bool networkRegistered() const; // true если CREG=1 или 5
	uint8_t cregStat() const; // 0..5
	int8_t csqRssi() const; // 0..31,99 unknown
	uint8_t csqBer() const; // 0..7,99 unknown
	int16_t rssiDbm() const; // 0 если unknown
	uint8_t signalBars() const; // 0..5

	bool isReady() const;
	bool isBusy() const;
	const char* stateName() const;

	bool hasNewTime(); // true один раз после обновления
	String lastCCLKRaw() const; // строка +CCLK:...
	bool lastTimeValid(int minYear = 2020) const;

	// +CCLK:"yy/MM/dd,hh:mm:ss±zz"
	static bool parseCCLK(const String& resp,
		int& year, int& month, int& day,
		int& hour, int& minute, int& second,
		int& tzQuarterHours);

private:
	enum class State :uint8_t {
		STOPPED,

		// health
		AT_PING,

		// status
		STAT_CREG,
		STAT_CSQ,

		// bring-up / init
		PROBE_AT,
		PWRKEY_PULSE,
		WAIT_URC,
		WAIT_AT_READY,
		INIT_ATE0,
		INIT_CMEE,
		INIT_CLTS,
		INIT_SAVE,

		// normal
		IDLE,
		WAIT_NETWORK,
		REQ_CCLK,

		// NTP fallback
		NTP_CGATT,
		NTP_SAPBR_CFG1,
		NTP_SAPBR_CFG_APN,
		NTP_SAPBR_CFG_USER,
		NTP_SAPBR_CFG_PWD,
		NTP_SAPBR_OPEN,
		NTP_CNTPCID,
		NTP_CNTP_SET,
		NTP_CNTP_START,
		NTP_SAPBR_CLOSE,
		REQ_CCLK_AFTER_NTP
	};

	HardwareSerial& _sim;
	uint8_t _pwrkeyPin;
	bool _pwrkeyActiveHigh;

	Stream* _dbg = nullptr;
	bool _logAT = true;

	// schedulers
	uint32_t _periodMs = 60000;
	uint32_t _nextPollMs = 0;
	bool _forcePoll = false;

	uint32_t _healthPingMs = 60000;
	uint32_t _nextPingMs = 0;

	uint32_t _statusPollMs = 5000;
	uint32_t _nextStatusMs = 0;

	// recovery
	uint8_t _maxRecoveryAttempts = 5;
	uint8_t _recoveryUsed = 0;

	// ntp config
	NtpConfig _ntp;

	// fsm
	State _st = State::STOPPED;
	uint32_t _stEnterMs = 0;

	// URC flags
	bool _seenRDY = false;
	bool _seenCallReady = false;
	bool _seenSMSReady = false;

	// AT transaction
	bool _atPending = false;
	const char* _atExpect = nullptr;
	uint32_t _atDeadlineMs = 0;
	String _atResp;
	String _lineBuf;

	bool _atOk = false;
	bool _atErr = false;

	// time
	String _lastCCLK;
	bool _newTime = false;

	// health
	uint32_t _lastAtOkMs = 0;

	// cached status
	uint8_t _cregStat = 0; // 0..5
	int8_t _csqRssi = 99;
	uint8_t _csqBer = 99;

private:
	void _log(const __FlashStringHelper* s);
	void _logKV(const __FlashStringHelper* k, const String& v);
	void _logATLine(const __FlashStringHelper* prefix, const String& s);

	void _enter(State s);
	void _serviceState();

	void _readUartNonBlocking();
	void _handleLine(const String& line);

	void _startAT(const char* cmd, const char* expect, uint32_t timeoutMs);
	bool _atDoneTimeout() const;
	bool _atMatchedExpect() const;

	void _pwrkeyIdle();
	void _pwrkeyPress();
	void _pwrkeyRelease();

	static bool _isCCLKValid(const String& resp, int minYear);
};