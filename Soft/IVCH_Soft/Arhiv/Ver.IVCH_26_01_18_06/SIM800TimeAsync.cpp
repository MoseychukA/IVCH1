#include "SIM800TimeAsync.h"

SIM800TimeAsync::SIM800TimeAsync(HardwareSerial& sim, uint8_t pwrkeyPin, bool pwrkeyActiveHigh)
	:_sim(sim), _pwrkeyPin(pwrkeyPin), _pwrkeyActiveHigh(pwrkeyActiveHigh) {}

void SIM800TimeAsync::begin(uint32_t baud) {
	pinMode(_pwrkeyPin, OUTPUT);
	_pwrkeyIdle(); // не "держать кнопку"
	_sim.begin(baud);

	_nextPollMs = millis() + _periodMs;
}

void SIM800TimeAsync::setDebug(Stream* dbg, bool logAT) {
	_dbg = dbg;
	_logAT = logAT;
}

void SIM800TimeAsync::setPeriodMs(uint32_t periodMs) {
	_periodMs = periodMs;
	_nextPollMs = millis() + _periodMs;
}

void SIM800TimeAsync::setNtpConfig(const NtpConfig& cfg) {
	_ntp = cfg;
}

void SIM800TimeAsync::start() {
	_enter(State::PROBE_AT);
}

void SIM800TimeAsync::requestTimeNow() {
	_forcePoll = true;
}

bool SIM800TimeAsync::isReady() const {
	return _st == State::IDLE || _st == State::WAIT_NETWORK || _st == State::REQ_CCLK
		|| (uint8_t)_st > (uint8_t)State::INIT_SAVE; // после init
}

bool SIM800TimeAsync::isBusy() const {
	return _st != State::IDLE && _st != State::STOPPED;
}

const char* SIM800TimeAsync::stateName() const {
	switch (_st) {
	case State::STOPPED:return "STOPPED";
	case State::PROBE_AT:return "PROBE_AT";
	case State::PWRKEY_PULSE:return "PWRKEY_PULSE";
	case State::WAIT_URC:return "WAIT_URC";
	case State::WAIT_AT_READY:return "WAIT_AT_READY";
	case State::INIT_ATE0:return "INIT_ATE0";
	case State::INIT_CMEE:return "INIT_CMEE";
	case State::INIT_CLTS:return "INIT_CLTS";
	case State::INIT_SAVE:return "INIT_SAVE";
	case State::IDLE:return "IDLE";
	case State::WAIT_NETWORK:return "WAIT_NETWORK";
	case State::REQ_CCLK:return "REQ_CCLK";
	case State::NTP_CGATT:return "NTP_CGATT";
	case State::NTP_SAPBR_CFG1:return "NTP_SAPBR_CFG1";
	case State::NTP_SAPBR_CFG_APN:return "NTP_SAPBR_CFG_APN";
	case State::NTP_SAPBR_CFG_USER:return "NTP_SAPBR_CFG_USER";
	case State::NTP_SAPBR_CFG_PWD:return "NTP_SAPBR_CFG_PWD";
	case State::NTP_SAPBR_OPEN:return "NTP_SAPBR_OPEN";
	case State::NTP_CNTPCID:return "NTP_CNTPCID";
	case State::NTP_CNTP_SET:return "NTP_CNTP_SET";
	case State::NTP_CNTP_START:return "NTP_CNTP_START";
	case State::NTP_SAPBR_CLOSE:return "NTP_SAPBR_CLOSE";
	case State::REQ_CCLK_AFTER_NTP:return "REQ_CCLK_AFTER_NTP";
	}
	return "?";
}

bool SIM800TimeAsync::hasNewTime() {
	bool v = _newTime;
	_newTime = false;
	return v;
}

String SIM800TimeAsync::lastCCLKRaw() const {
	return _lastCCLK;
}

bool SIM800TimeAsync::lastTimeValid(int minYear) const {
	return _isCCLKValid(_lastCCLK, minYear);
}

void SIM800TimeAsync::tick() {
	_readUartNonBlocking();
	_serviceState();
}

// ---------------- debug ----------------
void SIM800TimeAsync::_log(const __FlashStringHelper* s) {
	if (!_dbg) return;
	_dbg->print('['); _dbg->print(millis()); _dbg->print("] ");
	_dbg->println(s);
}
void SIM800TimeAsync::_logKV(const __FlashStringHelper* k, const String& v) {
	if (!_dbg) return;
	_dbg->print('['); _dbg->print(millis()); _dbg->print("] ");
	_dbg->print(k); _dbg->println(v);
}
void SIM800TimeAsync::_logATLine(const __FlashStringHelper* prefix, const String& s) {
	if (!_dbg || !_logAT) return;
	_dbg->print('['); _dbg->print(millis()); _dbg->print("] ");
	_dbg->print(prefix); _dbg->println(s);
}

// ---------------- state ----------------
void SIM800TimeAsync::_enter(State s) {
	_st = s;
	_stEnterMs = millis();
	if (_dbg) {
		_dbg->print('['); _dbg->print(_stEnterMs); _dbg->print("] ");
		_dbg->print(F("STATE -> "));
		_dbg->println(stateName());
	}
}

void SIM800TimeAsync::_serviceState() {
	const uint32_t now = millis();

	// планировщик (только в IDLE)
	if (_st == State::IDLE) {
		if (_forcePoll || (int32_t)(now - _nextPollMs) >= 0) {
			_forcePoll = false;
			_nextPollMs = now + _periodMs;
			_enter(State::WAIT_NETWORK);
			return;
		}
		return;
	}

	switch (_st) {
	case State::STOPPED:
		return;

	case State::PROBE_AT:
		// не блокируем:просто стартуем AT и ждём
		if (!_atPending) _startAT("AT", "OK", 1500);
		else {
			if (_atDoneOK()) {
				_enter(State::INIT_ATE0);
			}
			else if (_atDoneTimeout()) {
				_enter(State::PWRKEY_PULSE);
			}
		}
		return;

	case State::PWRKEY_PULSE:
		// короткая “нажатие-отпускание” без delay:делаем по времени
		if (now - _stEnterMs < 50) {
			_pwrkeyIdle();
		}
		else if (now - _stEnterMs < 1300) {
			_pwrkeyPress();
		}
		else {
			_pwrkeyRelease();
			_seenRDY = _seenCallReady = _seenSMSReady = false;
			_enter(State::WAIT_URC);
		}
		return;

	case State::WAIT_URC:
		// ждём URC до 8 сек,но даже если не увидим - потом проверим AT
		if (_seenRDY || _seenCallReady || _seenSMSReady || (now - _stEnterMs) > 8000) {
			_enter(State::WAIT_AT_READY);
		}
		return;

	case State::WAIT_AT_READY:
		if (!_atPending) _startAT("AT", "OK", 12000);
		else {
			if (_atDoneOK()) _enter(State::INIT_ATE0);
			else if (_atDoneTimeout()) _enter(State::STOPPED);
		}
		return;

	case State::INIT_ATE0:
		if (!_atPending) _startAT("ATE0", "OK", 1500);
		else if (_atDoneOK() || _atDoneTimeout()) _enter(State::INIT_CMEE);
		return;

	case State::INIT_CMEE:
		if (!_atPending) _startAT("AT+CMEE=2", "OK", 2000);
		else if (_atDoneOK() || _atDoneTimeout()) _enter(State::INIT_CLTS);
		return;

	case State::INIT_CLTS:
		if (!_atPending) _startAT("AT+CLTS=1", "OK", 2000);
		else if (_atDoneOK() || _atDoneTimeout()) _enter(State::INIT_SAVE);
		return;

	case State::INIT_SAVE:
		if (!_atPending) _startAT("AT&W", "OK", 3000);
		else if (_atDoneOK() || _atDoneTimeout()) _enter(State::IDLE);
		return;

	case State::WAIT_NETWORK:
		if (!_atPending) _startAT("AT+CREG?", "+CREG:", 1500);
		else {
			if (_atDoneTimeout()) {
				// повторим через секунду,не блокируя
				if (now - _stEnterMs > 60000) { // общий таймаут 60 сек
					_enter(State::IDLE);
				}
				else {
					_atPending = false;
					_atResp = "";
					// подождём 1 сек перед следующим CREG?
					if (now - _stEnterMs > 1000) {
						// просто продолжим,_startAT вызовется в следующем tick()
					}
				}
			}
			else if (_atMatchedExpect()) {
				// ищем ,1 или ,5
				if (_atResp.indexOf(",1") >= 0 || _atResp.indexOf(",5") >= 0) {
					_enter(State::REQ_CCLK);
				}
				else {
					// ещё не зарегистрирован
					_atPending = false;
					_atResp = "";
				}
			}
		}
		return;

	case State::REQ_CCLK:
		if (!_atPending) _startAT("AT+CCLK?", "+CCLK:", 1500);
		else {
			if (_atDoneOK() || _atMatchedExpect()) {
				// сохраним сырой ответ (весь буфер)
				_lastCCLK = _atResp;
				_newTime = true;

				if (!_isCCLKValid(_lastCCLK, 2020) && _ntp.enableFallback && _ntp.apn && _ntp.apn[0]) {
					_enter(State::NTP_CGATT);
				}
				else {
					_enter(State::IDLE);
				}
			}
			else if (_atDoneTimeout()) {
				_enter(State::IDLE);
			}
		}
		return;

		// -------- NTP fallback (не блокирует) --------
	case State::NTP_CGATT:
		if (!_atPending) _startAT("AT+CGATT?", "+CGATT:", 2000);
		else {
			if (_atDoneTimeout()) { _enter(State::NTP_SAPBR_CLOSE); return; }
			if (_atMatchedExpect()) {
				if (_atResp.indexOf("+CGATT:1") >= 0) _enter(State::NTP_SAPBR_CFG1);
				else { _atPending = false; _atResp = ""; _startAT("AT+CGATT=1", "OK", 5000); }
			}
		}
		return;

	case State::NTP_SAPBR_CFG1:
		if (!_atPending) _startAT("AT+SAPBR=3,1,\"Contype\",\"GPRS\"", "OK", 2000);
		else if (_atDoneOK() || _atDoneTimeout()) _enter(State::NTP_SAPBR_CFG_APN);
		return;

	case State::NTP_SAPBR_CFG_APN: {
		if (!_atPending) {
			String cmd = String("AT+SAPBR=3,1,\"APN\",\"") + _ntp.apn + "\"";
			_startAT(cmd.c_str(), "OK", 2000);
		}
		else if (_atDoneOK() || _atDoneTimeout()) {
			_enter((_ntp.user && _ntp.user[0]) ? State::NTP_SAPBR_CFG_USER
				: ((_ntp.pass && _ntp.pass[0]) ? State::NTP_SAPBR_CFG_PWD : State::NTP_SAPBR_OPEN));
		}
		return;
	}

	case State::NTP_SAPBR_CFG_USER: {
		if (!_atPending) {
			String cmd = String("AT+SAPBR=3,1,\"USER\",\"") + _ntp.user + "\"";
			_startAT(cmd.c_str(), "OK", 2000);
		}
		else if (_atDoneOK() || _atDoneTimeout()) {
			_enter((_ntp.pass && _ntp.pass[0]) ? State::NTP_SAPBR_CFG_PWD : State::NTP_SAPBR_OPEN);
		}
		return;
	}

	case State::NTP_SAPBR_CFG_PWD: {
		if (!_atPending) {
			String cmd = String("AT+SAPBR=3,1,\"PWD\",\"") + _ntp.pass + "\"";
			_startAT(cmd.c_str(), "OK", 2000);
		}
		else if (_atDoneOK() || _atDoneTimeout()) {
			_enter(State::NTP_SAPBR_OPEN);
		}
		return;
	}

	case State::NTP_SAPBR_OPEN:
		if (!_atPending) _startAT("AT+SAPBR=1,1", "OK", 45000);
		else if (_atDoneOK() || _atDoneTimeout()) _enter(State::NTP_CNTPCID);
		return;

	case State::NTP_CNTPCID:
		if (!_atPending) _startAT("AT+CNTPCID=1", "OK", 2000);
		else if (_atDoneOK() || _atDoneTimeout()) _enter(State::NTP_CNTP_SET);
		return;

	case State::NTP_CNTP_SET: {
		if (!_atPending) {
			String cmd = String("AT+CNTP=\"") + (_ntp.server ? _ntp.server : "pool.ntp.org") + "\"," + String(_ntp.tzHours);
			_startAT(cmd.c_str(), "OK", 3000);
		}
		else if (_atDoneOK() || _atDoneTimeout()) {
			_enter(State::NTP_CNTP_START);
		}
		return;
	}

	case State::NTP_CNTP_START:
		// CNTP возвращает +CNTP:<code> асинхронно,ждём его в _handleLine
		if (!_atPending) _startAT("AT+CNTP", "+CNTP:", 20000);
		else {
			if (_atMatchedExpect()) {
				// успех обычно +CNTP:1
				if (_atResp.indexOf("+CNTP:1") >= 0 || _atResp.indexOf("+CNTP:1") >= 0) {
					_enter(State::NTP_SAPBR_CLOSE);
				}
				else {
					_enter(State::NTP_SAPBR_CLOSE);
				}
			}
			else if (_atDoneTimeout()) {
				_enter(State::NTP_SAPBR_CLOSE);
			}
		}
		return;

	case State::NTP_SAPBR_CLOSE:
		if (!_atPending) _startAT("AT+SAPBR=0,1", "OK", 8000);
		else if (_atDoneOK() || _atDoneTimeout()) _enter(State::REQ_CCLK_AFTER_NTP);
		return;

	case State::REQ_CCLK_AFTER_NTP:
		if (!_atPending) _startAT("AT+CCLK?", "+CCLK:", 1500);
		else {
			if (_atDoneOK() || _atMatchedExpect()) {
				_lastCCLK = _atResp;
				_newTime = true;
			}
			_enter(State::IDLE);
		}
		return;
	}
}

// ---------------- UART + URC ----------------
void SIM800TimeAsync::_readUartNonBlocking() {
	while (_sim.available()) {
		char c = (char)_sim.read();
		if (c == '\r') continue;

		if (c == '\n') {
			if (_lineBuf.length()) {
				_handleLine(_lineBuf);
				_lineBuf = "";
			}
		}
		else {
			if (_lineBuf.length() < 160) _lineBuf += c;
		}
	}
}

void SIM800TimeAsync::_handleLine(const String& line) {
	// URC
	if (line.indexOf("RDY") >= 0) _seenRDY = true;
	if (line.indexOf("Call Ready") >= 0) _seenCallReady = true;
	if (line.indexOf("SMS Ready") >= 0) _seenSMSReady = true;

	// Лог входящих строк
	if (_dbg && _logAT) {
		_dbg->print('['); _dbg->print(millis()); _dbg->print("] ");
		_dbg->print(F("<< "));
		_dbg->println(line);
	}

	// Если идет AT-транзакция — копим ответ
	if (_atPending) {
		_atResp += line;
		_atResp += "\n";

		// быстрый финиш по OK/ERROR/expect — окончательно обработаем в _serviceState()
		// (здесь лишь накапливаем)
	}
}

// ---------------- AT helpers ----------------
void SIM800TimeAsync::_startAT(const char* cmd, const char* expect, uint32_t timeoutMs) {
	// очистить "хвост" от предыдущего
	_atResp = "";
	_atExpect = expect;
	_atPending = true;
	_atDeadlineMs = millis() + timeoutMs;

	if (_dbg && _logAT) {
		_dbg->print('['); _dbg->print(millis()); _dbg->print("] ");
		_dbg->print(F(">> "));
		_dbg->println(cmd);
	}

	_sim.print(cmd);
	_sim.print("\r");
}

bool SIM800TimeAsync::_atDoneOK() const {
	if (!_atPending) return true;
	if ((int32_t)(millis() - _atDeadlineMs) >= 0) return false;
	return (_atResp.indexOf("\nOK") >= 0) || (_atResp.startsWith("OK")) || (_atResp.indexOf("OK\n") >= 0);
}

bool SIM800TimeAsync::_atDoneTimeout() const {
	if (!_atPending) return false;
	return (int32_t)(millis() - _atDeadlineMs) >= 0;
}

bool SIM800TimeAsync::_atMatchedExpect() const {
	if (!_atPending || !_atExpect) return false;
	return _atResp.indexOf(_atExpect) >= 0;
}

// ---------------- PWRKEY helpers ----------------
// Для вашей схемы (NPN тянет к GND):лучше НЕ держать кнопку.
// Если ваш core не поддерживает INPUT_PULLDOWN,используйте INPUT и внешний pulldown на базе NPN.
void SIM800TimeAsync::_pwrkeyIdle() {
	// отпущено
#if defined(INPUT_PULLDOWN)
	pinMode(_pwrkeyPin, INPUT_PULLDOWN);
#else
	pinMode(_pwrkeyPin, OUTPUT);
	digitalWrite(_pwrkeyPin, _pwrkeyActiveHigh ? LOW : HIGH);
#endif
}

void SIM800TimeAsync::_pwrkeyPress() {
	pinMode(_pwrkeyPin, OUTPUT);
	digitalWrite(_pwrkeyPin, _pwrkeyActiveHigh ? HIGH : LOW);
}

void SIM800TimeAsync::_pwrkeyRelease() {
	_pwrkeyIdle();
}

// ---------------- time parse/valid ----------------
bool SIM800TimeAsync::parseCCLK(const String& resp,
	int& year, int& month, int& day,
	int& hour, int& minute, int& second,
	int& tzQuarterHours) {
	int q1 = resp.indexOf('\"');
	int q2 = resp.indexOf('\"', q1 + 1);
	if (q1 < 0 || q2 < 0) return false;

	String v = resp.substring(q1 + 1, q2); // yy/MM/dd,hh:mm:ss±zz
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

bool SIM800TimeAsync::_isCCLKValid(const String& resp, int minYear) {
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