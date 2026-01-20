#pragma once
#include <Arduino.h>

class SIM800TimeAsync {
public:
	struct NtpConfig {
		const char* apn = nullptr;
		const char* user = "";
		const char* pass = "";
		const char* server = "pool.ntp.org";
		int tzHours = 0; // для AT+CNTP в часах
		bool enableFallback = false;
	};

	SIM800TimeAsync(HardwareSerial& sim, uint8_t pwrkeyPin,
		bool pwrkeyActiveHigh = true);

	void begin(uint32_t baud);
	void setDebug(Stream* dbg, bool logAT = true);

	// Запустить процесс (не блокирует):проверка AT -> при необходимости PWRKEY -> init
	void start();

	// Вызывать часто из loop() (хоть каждую итерацию)
	void tick();

	// Настроить период запроса времени
	void setPeriodMs(uint32_t periodMs);

	// Настроить NTP fallback (опционально)
	void setNtpConfig(const NtpConfig& cfg);

	// Состояние
	bool isReady() const; // модем инициализирован (AT/ATE0/CLTS)
	bool isBusy() const; // сейчас идет транзакция/процедура
	const char* stateName() const;

	// Результат последней успешной выдачи времени
	bool hasNewTime(); // true один раз после обновления
	String lastCCLKRaw() const; // сырой ответ +CCLK
	bool lastTimeValid(int minYear = 2020) const;

	// Принудительно запросить время (не блокирует)
	void requestTimeNow();

	// Парсер +CCLK:"yy/MM/dd,hh:mm:ss±zz"
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

		// NTP fallback states
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

	// планировщик
	uint32_t _periodMs = 60000;
	uint32_t _nextPollMs = 0;
	bool _forcePoll = false;

	// ntp cfg
	NtpConfig _ntp;

	// состояние
	State _st = State::STOPPED;
	uint32_t _stEnterMs = 0;

	// URC flags
	bool _seenRDY = false;
	bool _seenCallReady = false;
	bool _seenSMSReady = false;

	// AT транзакция
	bool _atPending = false;
	const char* _atExpect = nullptr;
	uint32_t _atDeadlineMs = 0;
	String _atResp; // собранные строки ответа
	String _lineBuf; // текущая строка из UART

	// время
	String _lastCCLK;
	bool _newTime = false;

private:
	// debug
	void _log(const __FlashStringHelper* s);
	void _logKV(const __FlashStringHelper* k, const String& v);
	void _logATLine(const __FlashStringHelper* prefix, const String& s);

	// state helpers
	void _enter(State s);
	void _serviceState();

	// UART parsing
	void _readUartNonBlocking();
	void _handleLine(const String& line);

	// AT helpers (не блокируют)
	void _startAT(const char* cmd, const char* expect, uint32_t timeoutMs);
	bool _atDoneOK() const;
	bool _atDoneTimeout() const;
	bool _atMatchedExpect() const;

	// PWRKEY helpers
	void _pwrkeyIdle();
	void _pwrkeyPress();
	void _pwrkeyRelease();

	// logic
	static bool _isCCLKValid(const String& resp, int minYear);
};