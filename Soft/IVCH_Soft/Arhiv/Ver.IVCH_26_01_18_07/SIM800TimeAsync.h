#pragma once
#include <Arduino.h>

// Неблокирующее получение времени с SIM800C:
// - включение через PWRKEY (через NPN на землю)
// - периодический запрос AT+CCLK? (раз в periodMs)
// - при невалидном времени:опциональный NTP fallback (AT+CNTP) по GPRS
class SIM800TimeAsync {
public:
	struct NtpConfig {
		const char* apn = nullptr; // обязательно для fallback
		const char* user = ""; // опционально
		const char* pass = ""; // опционально
		const char* server = "pool.ntp.org";
		int tzHours = 0; // для AT+CNTP в часах (для UTC ставьте 0)
		bool enableFallback = false;
	};

	SIM800TimeAsync(HardwareSerial& sim, uint8_t pwrkeyPin, bool pwrkeyActiveHigh = true);

	void begin(uint32_t baud);
	void setDebug(Stream* dbg, bool logAT = true);

	void start(); // запустить FSM (не блокирует)
	void tick(); // вызывать часто из loop()

	void setPeriodMs(uint32_t periodMs);
	void setNtpConfig(const NtpConfig& cfg);

	void requestTimeNow(); // принудительно запросить время "вне очереди"

	bool isReady() const;
	bool isBusy() const;
	const char* stateName() const;

	bool hasNewTime(); // true один раз после обновления
	String lastCCLKRaw() const; // строка +CCLK:"..."
	bool lastTimeValid(int minYear = 2020) const;

	// Парсинг +CCLK:"yy/MM/dd,hh:mm:ss±zz"
	static bool parseCCLK(const String& resp,
		int& year, int& month, int& day,
		int& hour, int& minute, int& second,
		int& tzQuarterHours);

private:
	enum class State :uint8_t {
		STOPPED,
		PROBE_AT,
		PWRKEY_PULSE,
		WAIT_URC,
		WAIT_AT_READY,
		INIT_ATE0,
		INIT_CMEE,
		INIT_CLTS,
		INIT_SAVE,
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

private:
	HardwareSerial& _sim;
	uint8_t _pwrkeyPin;
	bool _pwrkeyActiveHigh;

	Stream* _dbg = nullptr;
	bool _logAT = true;

	// scheduler
	uint32_t _periodMs = 60000;
	uint32_t _nextPollMs = 0;
	bool _forcePoll = false;

	// ntp config
	NtpConfig _ntp;

	// state machine
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

private:
	// debug helpers
	void _log(const __FlashStringHelper* s);
	void _logKV(const __FlashStringHelper* k, const String& v);
	void _logATLine(const __FlashStringHelper* prefix, const String& s);

	// fsm helpers
	void _enter(State s);
	void _serviceState();

	// uart helpers
	void _readUartNonBlocking();
	void _handleLine(const String& line);

	// AT helpers (non-blocking)
	void _startAT(const char* cmd, const char* expect, uint32_t timeoutMs);
	bool _atDoneTimeout() const;
	bool _atMatchedExpect() const;

	// PWRKEY helpers
	void _pwrkeyIdle();
	void _pwrkeyPress();
	void _pwrkeyRelease();

	// time validity
	static bool _isCCLKValid(const String& resp, int minYear);
};
