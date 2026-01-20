#include "SIM800Time.h"

SIM800TimeClient::SIM800TimeClient(HardwareSerial& serial, uint8_t pwrkeyPin, bool pwrkeyActiveHigh)
	:_sim(serial), _pwrkeyPin(pwrkeyPin), _pwrkeyActiveHigh(pwrkeyActiveHigh) {}

void SIM800TimeClient::setDebug(Stream* dbg, bool logAT) {
	_dbg = dbg;
	_logAT = logAT;
}

void SIM800TimeClient::_dbgln(const String& s) {
	if (_dbg) { _dbg->print('['); _dbg->print(millis()); _dbg->print("] "); _dbg->println(s); }
}
void SIM800TimeClient::_dbgln(const __FlashStringHelper* s) {
	if (_dbg) { _dbg->print('['); _dbg->print(millis()); _dbg->print("] "); _dbg->println(s); }
}
void SIM800TimeClient::_dbgKV(const __FlashStringHelper* k, const String& v) {
	if (_dbg) { _dbg->print('['); _dbg->print(millis()); _dbg->print("] "); _dbg->print(k); _dbg->println(v); }
}

void SIM800TimeClient::begin(uint32_t baud) {
	pinMode(_pwrkeyPin, OUTPUT);
	_setPWRKEYIdle();
	_sim.begin(baud);

	_dbgln(F("SIM800:begin()"));
	_dbgKV(F("UART baud="), String(baud));
}

void SIM800TimeClient::_setPWRKEYIdle() 
{
	// Отпущено:транзистор точно закрыт
	pinMode(_pwrkeyPin, INPUT_PULLDOWN); // если INPUT_PULLDOWN не поддерживается вашим core -> INPUT
}

void SIM800TimeClient::_setPWRKEYPressed() 
{
	// Нажато:открыть транзистор
	pinMode(_pwrkeyPin, OUTPUT);
	digitalWrite(_pwrkeyPin, HIGH);
}

void SIM800TimeClient::_pulsePWRKEY(uint16_t pressMs, uint16_t preDelayMs) {
	_dbgln(F("SIM800:PWRKEY pulse"));
	_dbgKV(F(" preDelayMs="), String(preDelayMs));
	_dbgKV(F(" pressMs="), String(pressMs));

	_setPWRKEYIdle();
	delay(preDelayMs);
	_setPWRKEYPressed();
	delay(pressMs);
	_setPWRKEYIdle();
}

String SIM800TimeClient::_readAll(uint32_t timeoutMs) {
	String s;
	uint32_t t0 = millis();
	while (millis() - t0 < timeoutMs) {
		while (_sim.available()) s += (char)_sim.read();
		delay(2);
	}
	return s;
}

bool SIM800TimeClient::_sendAT(const char* cmd, const char* expect, uint32_t timeoutMs, String* out) {
	while (_sim.available()) _sim.read();

	if (_dbg && _logAT) {
		_dbg->print('['); _dbg->print(millis()); _dbg->print("] ");
		_dbg->print(F(">> "));
		_dbg->println(cmd);
	}

	_sim.print(cmd);
	_sim.print("\r");

	String resp = _readAll(timeoutMs);

	if (_dbg && _logAT) {
		_dbg->print('['); _dbg->print(millis()); _dbg->print("] ");
		_dbg->println(F("<<"));
		_dbg->println(resp);
	}

	if (out) *out = resp;

	if (!expect) return true;
	return resp.indexOf(expect) >= 0;
}

bool SIM800TimeClient::_waitATReady(uint32_t timeoutMs) {
	_dbgln(F("SIM800:wait AT ready..."));
	uint32_t t0 = millis();
	while (millis() - t0 < timeoutMs) {
		if (_sendAT("AT", "OK", 700)) {
			_dbgln(F("SIM800:AT OK"));
			return true;
		}
		delay(300);
	}
	_dbgln(F("SIM800:AT timeout"));
	return false;
}

// ---------- URC ----------
void SIM800TimeClient::_resetURCFlags() {
	_seenRDY = _seenCallReady = _seenSMSReady = false;
}

void SIM800TimeClient::_drainInput(uint32_t ms) {
	uint32_t t0 = millis();
	while (millis() - t0 < ms) {
		while (_sim.available()) (void)_sim.read();
		delay(2);
	}
}

void SIM800TimeClient::_pollURC(uint32_t ms) {
	_dbgKV(F("SIM800:wait URC ms="), String(ms));
	String line;
	uint32_t t0 = millis();
	while (millis() - t0 < ms) {
		while (_sim.available()) {
			char c = (char)_sim.read();
			if (c == '\r') continue;

			if (c == '\n') {
				if (line.length()) {
					if (_dbg) {
						_dbg->print('['); _dbg->print(millis()); _dbg->print("] ");
						_dbg->print(F("URC:"));
						_dbg->println(line);
					}
					if (line.indexOf("RDY") >= 0) _seenRDY = true;
					if (line.indexOf("Call Ready") >= 0) _seenCallReady = true;
					if (line.indexOf("SMS Ready") >= 0) _seenSMSReady = true;
					line = "";
				}
			}
			else {
				if (line.length() < 96) line += c;
			}
		}
		delay(2);
	}
	if (_dbg) {
		_dbgKV(F("URC seen RDY="), String(_seenRDY ? "1" : "0"));
		_dbgKV(F("URC seen CallReady="), String(_seenCallReady ? "1" : "0"));
		_dbgKV(F("URC seen SMSReady="), String(_seenSMSReady ? "1" : "0"));
	}
}

bool SIM800TimeClient::ensureOnSmart(uint32_t atProbeTimeoutMs,
	uint16_t pwrkeyPressMs,
	uint32_t urcWaitMs,
	uint32_t atReadyTimeoutMs) {
	_dbgln(F("SIM800:ensureOnSmart()"));

	// 1) Если уже отвечает — не жмем PWRKEY (чтобы не выключить)
	if (_sendAT("AT", "OK", atProbeTimeoutMs)) {
		_dbgln(F("SIM800:already ON (AT OK)"));
		return true;
	}

	_dbgln(F("SIM800:no AT response -> press PWRKEY"));
	_resetURCFlags();
	_drainInput(200);

	// 2) Включаем
	_pulsePWRKEY(pwrkeyPressMs, 200);

	// 3) Ждем URC
	_pollURC(urcWaitMs);

	// 4) Финальный контроль AT
	bool ok = _waitATReady(atReadyTimeoutMs);
	_dbgKV(F("SIM800:ensureOnSmart result="), ok ? "OK" : "FAIL");
	return ok;
}

bool SIM800TimeClient::init(uint32_t atReadyTimeoutMs) {
	_dbgln(F("SIM800:init()"));

	if (!_waitATReady(atReadyTimeoutMs)) return false;

	_sendAT("ATE0", "OK", 800);
	_sendAT("AT+CMEE=2", "OK", 800);

	// NITZ
	_sendAT("AT+CLTS=1", "OK", 1200);
	_sendAT("AT&W", "OK", 1200);

	_dbgln(F("SIM800:init done"));
	return true;
}

bool SIM800TimeClient::waitForNetwork(uint32_t timeoutMs) {
	_dbgln(F("SIM800:waitForNetwork()"));
	uint32_t t0 = millis();
	while (millis() - t0 < timeoutMs) {
		String r;
		if (_sendAT("AT+CREG?", "+CREG:", 1200, &r)) {
			// печать статуса прямо в лог
			if (_dbg) _dbgKV(F("CREG resp="), r);

			int idx = r.indexOf("+CREG:");
			if (idx >= 0) {
				if (r.indexOf(",1", idx) >= 0 || r.indexOf(",5", idx) >= 0) {
					_dbgln(F("SIM800:network registered"));
					return true;
				}
			}
		}
		delay(1000);
	}
	_dbgln(F("SIM800:network wait timeout"));
	return false;
}

bool SIM800TimeClient::getCCLK(String& rawResp, uint32_t timeoutMs) {
	_dbgln(F("SIM800:get CCLK"));
	return _sendAT("AT+CCLK?", "+CCLK:", timeoutMs, &rawResp);
}

bool SIM800TimeClient::parseCCLK(const String& resp,
	int& year, int& month, int& day,
	int& hour, int& minute, int& second,
	int& tzQuarterHours) {
	int q1 = resp.indexOf('\"');
	int q2 = resp.indexOf('\"', q1 + 1);
	if (q1 < 0 || q2 < 0) return false;

	String v = resp.substring(q1 + 1, q2);
	if (v.length() < 17) return false;

	int yy = v.substring(0, 2).toInt();
	month = v.substring(3, 5).toInt();
	day = v.substring(6, 8).toInt();
	hour = v.substring(9, 11).toInt();
	minute = v.substring(12, 14).toInt();
	second = v.substring(15, 17).toInt();
	year = 2000 + yy;

	tzQuarterHours = 0;
	if (v.length() >= 20) {
		char sign = v.charAt(17);
		int zz = v.substring(18).toInt();
		tzQuarterHours = (sign == '-') ? -zz : zz;
	}
	return true;
}

bool SIM800TimeClient::isCCLKValid(const String& resp, int minYear) {
	int y, mo, d, h, mi, s, tzq;
	if (!parseCCLK(resp, y, mo, d, h, mi, s, tzq)) return false;
	if (y < minYear) return false;
	if (mo < 1 || mo > 12) return false;
	if (d < 1 || d > 31) return false;
	if (h < 0 || h > 23) return false;
	if (mi < 0 || mi > 59) return false;
	if (s < 0 || s > 59) return false;
	return true;
}

// ---------- GPRS / NTP ----------
bool SIM800TimeClient::_waitCGATT(uint32_t timeoutMs) {
	_dbgln(F("SIM800:wait CGATT"));
	uint32_t t0 = millis();
	while (millis() - t0 < timeoutMs) {
		String r;
		if (_sendAT("AT+CGATT?", "+CGATT:", 1200, &r)) {
			if (_dbg) _dbgKV(F("CGATT resp="), r);
			if (r.indexOf("+CGATT:1") >= 0) return true;
		}
		_sendAT("AT+CGATT=1", "OK", 3000);
		delay(1000);
	}
	return false;
}

bool SIM800TimeClient::_bearerClose() {
	_dbgln(F("SIM800:SAPBR close"));
	_sendAT("AT+SAPBR=0,1", "OK", 5000);
	return true;
}

bool SIM800TimeClient::_bearerOpen(const char* apn, const char* user, const char* pass, uint32_t timeoutMs) {
	_dbgln(F("SIM800:SAPBR open"));

	_bearerClose();

	if (!_sendAT("AT+SAPBR=3,1,\"Contype\",\"GPRS\"", "OK", 2000)) return false;

	{
		String cmd = String("AT+SAPBR=3,1,\"APN\",\"") + apn + "\"";
		if (!_sendAT(cmd.c_str(), "OK", 2000)) return false;
	}

	if (user && user[0]) {
		String cmd = String("AT+SAPBR=3,1,\"USER\",\"") + user + "\"";
		_sendAT(cmd.c_str(), "OK", 2000);
	}
	if (pass && pass[0]) {
		String cmd = String("AT+SAPBR=3,1,\"PWD\",\"") + pass + "\"";
		_sendAT(cmd.c_str(), "OK", 2000);
	}

	if (!_sendAT("AT+SAPBR=1,1", "OK", timeoutMs)) return false;

	String r;
	if (_sendAT("AT+SAPBR=2,1", "+SAPBR:", 3000, &r)) {
		if (_dbg) _dbgKV(F("SAPBR2="), r);
	}
	return true;
}

bool SIM800TimeClient::_ntpRequest(const char* server, int tzHours, uint32_t timeoutMs) {
	_dbgln(F("SIM800:NTP request"));

	if (!_sendAT("AT+CNTPCID=1", "OK", 2000)) return false;

	{
		String cmd = String("AT+CNTP=\"") + server + "\"," + String(tzHours);
		if (!_sendAT(cmd.c_str(), "OK", 3000)) return false;
	}

	while (_sim.available()) _sim.read();

	if (_dbg && _logAT) _dbgln(F(">> AT+CNTP"));
	_sim.print("AT+CNTP\r");

	String resp = _readAll(timeoutMs);

	if (_dbg && _logAT) {
		_dbgln(F("<<"));
		if (_dbg) _dbg->println(resp);
	}

	int p = resp.indexOf("+CNTP:");
	if (p < 0) return false;

	int colon = resp.indexOf(':', p);
	if (colon < 0) return false;
	int end = resp.indexOf('\n', colon);
	if (end < 0) end = resp.length();

	String tail = resp.substring(colon + 1, end);
	tail.trim();
	int code = tail.toInt();

	if (_dbg) _dbgKV(F("CNTP code="), String(code));

	return (code == 1);
}

bool SIM800TimeClient::syncTimeWithNTP(const char* apn,
	const char* user,
	const char* pass,
	const char* ntpServer,
	int tzHours,
	uint32_t bearerOpenTimeoutMs,
	uint32_t ntpTimeoutMs) {
	_dbgln(F("SIM800:syncTimeWithNTP()"));

	if (!_waitCGATT(30000)) {
		_dbgln(F("SIM800:CGATT failed"));
		return false;
	}

	if (!_bearerOpen(apn, user, pass, bearerOpenTimeoutMs)) {
		_dbgln(F("SIM800:bearer open failed"));
		_bearerClose();
		return false;
	}

	bool ok = _ntpRequest(ntpServer, tzHours, ntpTimeoutMs);

	_bearerClose();

	_dbgKV(F("SIM800:NTP sync result="), ok ? "OK" : "FAIL");
	return ok;
}

bool SIM800TimeClient::getTimeSmart(String& rawResp,
	const char* apn,
	const char* user,
	const char* pass,
	const char* ntpServer,
	int tzHours,
	bool forceNtpIfInvalid) {
	_dbgln(F("SIM800:getTimeSmart()"));

	if (!waitForNetwork(60000)) {
		rawResp = "No network registration (+CREG)";
		return false;
	}

	delay(1500);

	if (!getCCLK(rawResp, 1500)) return false;

	if (_dbg) _dbgKV(F("CCLK raw="), rawResp);

	if (isCCLKValid(rawResp)) {
		_dbgln(F("SIM800:time valid via NITZ/RTC"));
		return true;
	}

	_dbgln(F("SIM800:CCLK invalid"));
	if (!forceNtpIfInvalid) return true;

	_dbgln(F("SIM800:try NTP fallback"));
	if (!syncTimeWithNTP(apn, user, pass, ntpServer, tzHours)) {
		_dbgln(F("SIM800:NTP fallback failed"));
		return true; // вернем невалидный CCLK как есть
	}

	delay(500);
	bool ok2 = getCCLK(rawResp, 1500);
	if (_dbg) _dbgKV(F("CCLK after NTP="), rawResp);
	return ok2;
}