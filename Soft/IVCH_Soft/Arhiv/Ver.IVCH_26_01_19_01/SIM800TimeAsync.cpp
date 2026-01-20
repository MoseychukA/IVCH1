#include "SIM800TimeAsync.h"

SIM800TimeAsync::SIM800TimeAsync(HardwareSerial& sim, uint8_t pwrkeyPin, bool pwrkeyActiveHigh)
	:_sim(sim), _pwrkeyPin(pwrkeyPin), _pwrkeyActiveHigh(pwrkeyActiveHigh) {}

void SIM800TimeAsync::begin(uint32_t baud) {
	pinMode(_pwrkeyPin, OUTPUT);
	_pwrkeyIdle();
	_sim.begin(baud);

	const uint32_t now = millis();
	_nextPollMs = now + _periodMs;
	_nextPingMs = now + _healthPingMs;
	_nextStatusMs = now + _statusPollMs;
}

void SIM800TimeAsync::setDebug(Stream* dbg, bool logAT) { _dbg = dbg; _logAT = logAT; }

void SIM800TimeAsync::setPeriodMs(uint32_t periodMs) {
	_periodMs = periodMs;
	_nextPollMs = millis() + _periodMs;
}

void SIM800TimeAsync::setNtpConfig(const NtpConfig& cfg) { _ntp = cfg; }

void SIM800TimeAsync::requestTimeNow() { _forcePoll = true; }

void SIM800TimeAsync::setHealthPingMs(uint32_t periodMs) {
	_healthPingMs = periodMs;
	_nextPingMs = millis() + _healthPingMs;
}

void SIM800TimeAsync::setMaxRecoveryAttempts(uint8_t n) { _maxRecoveryAttempts = n; }

uint32_t SIM800TimeAsync::lastAtOkMs() const { return _lastAtOkMs; }
uint8_t SIM800TimeAsync::recoveryAttemptsUsed() const { return _recoveryUsed; }

void SIM800TimeAsync::setStatusPollMs(uint32_t periodMs) {
	_statusPollMs = periodMs;
	_nextStatusMs = millis() + _statusPollMs;
}

bool SIM800TimeAsync::networkRegistered() const { return (_cregStat == 1 || _cregStat == 5); }
uint8_t SIM800TimeAsync::cregStat() const { return _cregStat; }
int8_t SIM800TimeAsync::csqRssi() const { return _csqRssi; }
uint8_t SIM800TimeAsync::csqBer() const { return _csqBer; }

int16_t SIM800TimeAsync::rssiDbm() const {
	if (_csqRssi == 99 || _csqRssi < 0 || _csqRssi > 31) return 0;
	return (int16_t)(-113 + 2 * _csqRssi);
}

uint8_t SIM800TimeAsync::signalBars() const {
	if (_csqRssi == 99) return 0;
	if (_csqRssi <= 2) return 0;
	if (_csqRssi <= 9) return 1;
	if (_csqRssi <= 14) return 2;
	if (_csqRssi <= 19) return 3;
	if (_csqRssi <= 25) return 4;
	return 5;
}

void SIM800TimeAsync::start() {
	_recoveryUsed = 0;
	_enter(State::PROBE_AT);
}

bool SIM800TimeAsync::isReady() const {
	return _st == State::IDLE || _st == State::WAIT_NETWORK || _st == State::REQ_CCLK
		|| (uint8_t)_st > (uint8_t)State::INIT_SAVE;
}

bool SIM800TimeAsync::isBusy() const { return _st != State::IDLE && _st != State::STOPPED; }

const char* SIM800TimeAsync::stateName() const {
	switch (_st) {
	case State::STOPPED:return "STOPPED";
	case State::AT_PING:return "AT_PING";
	case State::STAT_CREG:return "STAT_CREG";
	case State::STAT_CSQ:return "STAT_CSQ";
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

bool SIM800TimeAsync::hasNewTime() { bool v = _newTime; _newTime = false; return v; }
String SIM800TimeAsync::lastCCLKRaw() const { return _lastCCLK; }
bool SIM800TimeAsync::lastTimeValid(int minYear) const { return _isCCLKValid(_lastCCLK, minYear); }

void SIM800TimeAsync::tick() {
	_readUartNonBlocking();
	_serviceState();
}

// ---- debug ----
void SIM800TimeAsync::_log(const __FlashStringHelper* s) 
{
	if (!_dbg) return;
	_dbg->print('['); _dbg->print(millis()); _dbg->print("] ");
	_dbg->println(s);
}
void SIM800TimeAsync::_logKV(const __FlashStringHelper* k, const String& v) 
{
	if (!_dbg) return;
	_dbg->print('['); _dbg->print(millis()); _dbg->print("] ");
	_dbg->print(k); _dbg->println(v);
}
void SIM800TimeAsync::_logATLine(const __FlashStringHelper* prefix, const String& s) {
	if (!_dbg || !_logAT) return;
	_dbg->print('['); _dbg->print(millis()); _dbg->print("] ");
	_dbg->print(prefix); _dbg->println(s);
}

// ---- fsm ----
void SIM800TimeAsync::_enter(State s) {
	// reset AT transaction
	_atPending = false;
	_atExpect = nullptr;
	_atResp = "";
	_atDeadlineMs = 0;
	_atOk = false;
	_atErr = false;

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

	// IDLE scheduler
	if (_st == State::IDLE) {

		// 1) health ping
		if (_healthPingMs > 0 && (int32_t)(now - _nextPingMs) >= 0) {
			_nextPingMs = now + _healthPingMs;
			_enter(State::AT_PING);
			return;
		}

		// 2) time poll (do not starve by status polling)
		if (_forcePoll || (int32_t)(now - _nextPollMs) >= 0) {
			_forcePoll = false;
			_nextPollMs = now + _periodMs;
			_enter(State::WAIT_NETWORK);
			return;
		}

		// 3) status poll
		if (_statusPollMs > 0 && (int32_t)(now - _nextStatusMs) >= 0) {
			_nextStatusMs = now + _statusPollMs;
			_enter(State::STAT_CREG);
			return;
		}

		return;
	}

	switch (_st) {

	case State::STOPPED:
		// auto retry every 60s
		if ((now - _stEnterMs) > 60000) {
			_recoveryUsed = 0;
			_enter(State::PROBE_AT);
		}
		return;

		// ---- health ----
	case State::AT_PING:
		if (!_atPending) {
			_startAT("AT", "OK", 1500);
		}
		else {
			if (_atOk) {
				_recoveryUsed = 0;
				_enter(State::IDLE);
			}
			else if (_atDoneTimeout()) {
				if (_recoveryUsed < _maxRecoveryAttempts) {
					_recoveryUsed++;
					_enter(State::PWRKEY_PULSE);
				}
				else {
					_enter(State::STOPPED);
				}
			}
		}
		return;

		// ---- status ----
	case State::STAT_CREG:
		if (!_atPending) {
			_startAT("AT+CREG?", "+CREG:", 1500);
		}
		else {
			if (_atMatchedExpect()) {
				int p = _atResp.indexOf("+CREG:");
				if (p >= 0) {
					int comma = _atResp.indexOf(',', p);
					if (comma >= 0) {
						int stat = _atResp.substring(comma + 1).toInt();
						if (stat >= 0 && stat <= 5) _cregStat = (uint8_t)stat;
					}
				}
				_enter(State::STAT_CSQ);
			}
			else if (_atDoneTimeout()) {
				_enter(State::STAT_CSQ);
			}
		}
		return;

	case State::STAT_CSQ:
		if (!_atPending) {
			_startAT("AT+CSQ", "+CSQ:", 1500);
		}
		else {
			if (_atMatchedExpect()) {
				// +CSQ:<rssi>,<ber>
				int p = _atResp.indexOf("+CSQ:");
				if (p >= 0) {
					int colon = _atResp.indexOf(':', p);
					int comma = _atResp.indexOf(',', p);
					if (colon >= 0 && comma >= 0 && comma > colon) {
						int rssi = _atResp.substring(colon + 1, comma).toInt();
						int ber = _atResp.substring(comma + 1).toInt();
						_csqRssi = (int8_t)rssi;
						_csqBer = (uint8_t)ber;
					}
				}
				_enter(State::IDLE);
			}
			else if (_atDoneTimeout()) {
				_enter(State::IDLE);
			}
		}
		return;

		// ---- bring-up / init ----
	case State::PROBE_AT:
		if (!_atPending) {
			_startAT("AT", "OK", 1500);
		}
		else {
			if (_atOk) {
				_recoveryUsed = 0;
				_enter(State::INIT_ATE0);
			}
			else if (_atDoneTimeout()) {
				if (_recoveryUsed < _maxRecoveryAttempts) {
					_recoveryUsed++;
					_enter(State::PWRKEY_PULSE);
				}
				else {
					_enter(State::STOPPED);
				}
			}
		}
		return;

	case State::PWRKEY_PULSE:
		// 50ms idle then press up to 1.25s
		if (now - _stEnterMs < 50) _pwrkeyIdle();
		else if (now - _stEnterMs < 1300) _pwrkeyPress();
		else {
			_pwrkeyRelease();
			_seenRDY = _seenCallReady = _seenSMSReady = false;
			_enter(State::WAIT_URC);
		}
		return;

	case State::WAIT_URC:
		if (_seenRDY || _seenCallReady || _seenSMSReady || (now - _stEnterMs) > 8000) {
			_enter(State::WAIT_AT_READY);
		}
		return;

	case State::WAIT_AT_READY:
		if (!_atPending) {
			_startAT("AT", "OK", 12000);
		}
		else {
			if (_atOk) {
				_recoveryUsed = 0;
				_enter(State::INIT_ATE0);
			}
			else if (_atDoneTimeout()) {
				if (_recoveryUsed < _maxRecoveryAttempts) {
					_recoveryUsed++;
					_enter(State::PWRKEY_PULSE);
				}
				else {
					_enter(State::STOPPED);
				}
			}
		}
		return;

	case State::INIT_ATE0:
		if (!_atPending) _startAT("ATE0", "OK", 1500);
		else if (_atOk || _atDoneTimeout()) _enter(State::INIT_CMEE);
		return;

	case State::INIT_CMEE:
		if (!_atPending) _startAT("AT+CMEE=2", "OK", 2000);
		else if (_atOk || _atDoneTimeout()) _enter(State::INIT_CLTS);
		return;

	case State::INIT_CLTS:
		if (!_atPending) _startAT("AT+CLTS=1", "OK", 2000);
		else if (_atOk || _atDoneTimeout()) _enter(State::INIT_SAVE);
		return;

	case State::INIT_SAVE:
		if (!_atPending) _startAT("AT&W", "OK", 3000);
		else if (_atOk || _atDoneTimeout()) {
			_recoveryUsed = 0;
			_enter(State::IDLE);
		}
		return;

		// ---- time flow ----
	case State::WAIT_NETWORK:
		if (!_atPending) {
			_startAT("AT+CREG?", "+CREG:", 1500);
		}
		else {
			if (_atDoneTimeout()) {
				if (now - _stEnterMs > 60000) _enter(State::IDLE);
				else {
					_atPending = false;
					_atResp = "";
					_atOk = _atErr = false;
				}
			}
			else if (_atMatchedExpect()) {
				// cache CREG from this answer too
				int p = _atResp.indexOf("+CREG:");
				if (p >= 0) {
					int comma = _atResp.indexOf(',', p);
					if (comma >= 0) {
						int stat = _atResp.substring(comma + 1).toInt();
						if (stat >= 0 && stat <= 5) _cregStat = (uint8_t)stat;
					}
				}

				if (networkRegistered()) _enter(State::REQ_CCLK);
				else {
					_atPending = false;
					_atResp = "";
					_atOk = _atErr = false;
				}
			}
		}
		return;

	case State::REQ_CCLK:
		if (!_atPending) {
			_lastCCLK = "";
			_startAT("AT+CCLK?", "+CCLK:", 2000);
		}
		else {
			if (_atMatchedExpect()) {
				if (_lastCCLK.length() == 0) _lastCCLK = _atResp; // fallback
				if (_lastCCLK.indexOf("+CCLK:") >= 0) _newTime = true;

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

		// ---- NTP fallback ----
	case State::NTP_CGATT:
		if (!_atPending) _startAT("AT+CGATT?", "+CGATT:", 2000);
		else {
			if (_atDoneTimeout()) { _enter(State::NTP_SAPBR_CLOSE); return; }
			if (_atMatchedExpect()) {
				if (_atResp.indexOf("+CGATT:1") >= 0) _enter(State::NTP_SAPBR_CFG1);
				else { _atPending = false; _startAT("AT+CGATT=1", "OK", 5000); }
			}
		}
		return;

	case State::NTP_SAPBR_CFG1:
		if (!_atPending) _startAT("AT+SAPBR=3,1,\"Contype\",\"GPRS\"", "OK", 2000);
		else if (_atOk || _atDoneTimeout()) _enter(State::NTP_SAPBR_CFG_APN);
		return;

	case State::NTP_SAPBR_CFG_APN: {
		if (!_atPending) {
			String cmd = String("AT+SAPBR=3,1,\"APN\",\"") + _ntp.apn + "\"";
			_startAT(cmd.c_str(), "OK", 2000);
		}
		else if (_atOk || _atDoneTimeout()) {
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
		else if (_atOk || _atDoneTimeout()) {
			_enter((_ntp.pass && _ntp.pass[0]) ? State::NTP_SAPBR_CFG_PWD : State::NTP_SAPBR_OPEN);
		}
		return;
	}

	case State::NTP_SAPBR_CFG_PWD: {
		if (!_atPending) {
			String cmd = String("AT+SAPBR=3,1,\"PWD\",\"") + _ntp.pass + "\"";
			_startAT(cmd.c_str(), "OK", 2000);
		}
		else if (_atOk || _atDoneTimeout()) {
			_enter(State::NTP_SAPBR_OPEN);
		}
		return;
	}

	case State::NTP_SAPBR_OPEN:
		if (!_atPending) _startAT("AT+SAPBR=1,1", "OK", 45000);
		else if (_atOk || _atDoneTimeout()) _enter(State::NTP_CNTPCID);
		return;

	case State::NTP_CNTPCID:
		if (!_atPending) _startAT("AT+CNTPCID=1", "OK", 2000);
		else if (_atOk || _atDoneTimeout()) _enter(State::NTP_CNTP_SET);
		return;

	case State::NTP_CNTP_SET: {
		if (!_atPending) {
			String cmd = String("AT+CNTP=\"") + (_ntp.server ? _ntp.server : "pool.ntp.org") + "\"," + String(_ntp.tzHours);
			_startAT(cmd.c_str(), "OK", 3000);
		}
		else if (_atOk || _atDoneTimeout()) {
			_enter(State::NTP_CNTP_START);
		}
		return;
	}

	case State::NTP_CNTP_START:
		if (!_atPending) _startAT("AT+CNTP", "+CNTP:", 20000);
		else {
			if (_atMatchedExpect() || _atDoneTimeout()) _enter(State::NTP_SAPBR_CLOSE);
		}
		return;

	case State::NTP_SAPBR_CLOSE:
		if (!_atPending) _startAT("AT+SAPBR=0,1", "OK", 8000);
		else if (_atOk || _atDoneTimeout()) _enter(State::REQ_CCLK_AFTER_NTP);
		return;

	case State::REQ_CCLK_AFTER_NTP:
		if (!_atPending) {
			_lastCCLK = "";
			_startAT("AT+CCLK?", "+CCLK:", 2000);
		}
		else {
			if (_atMatchedExpect()) {
				if (_lastCCLK.indexOf("+CCLK:") >= 0) _newTime = true;
			}
			_enter(State::IDLE);
		}
		return;
	}
}

// ---- UART ----
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
	// URC hints
	if (line.indexOf("RDY") >= 0) _seenRDY = true;
	if (line.indexOf("Call Ready") >= 0) _seenCallReady = true;
	if (line.indexOf("SMS Ready") >= 0) _seenSMSReady = true;

	if (_dbg && _logAT) {
		_dbg->print('['); _dbg->print(millis()); _dbg->print("] ");
		_dbg->print(F("<< "));
		_dbg->println(line);
	}

	if (_atPending) {
		if (line == "OK") { _atOk = true; _lastAtOkMs = millis(); }
		else if (line == "ERROR") _atErr = true;

		if (line.startsWith("+CCLK:")) _lastCCLK = line;

		_atResp += line;
		_atResp += "\n";
	}
}

// ---- AT helpers ----
void SIM800TimeAsync::_startAT(const char* cmd, const char* expect, uint32_t timeoutMs) {
	_atResp = "";
	_atExpect = expect;
	_atPending = true;
	_atDeadlineMs = millis() + timeoutMs;
	_atOk = false;
	_atErr = false;

	if (_dbg && _logAT) {
		_dbg->print('['); _dbg->print(millis()); _dbg->print("] ");
		_dbg->print(F(">> "));
		_dbg->println(cmd);
	}

	_sim.print(cmd);
	_sim.print("\r");
}

bool SIM800TimeAsync::_atDoneTimeout() const {
	if (!_atPending) return false;
	return (int32_t)(millis() - _atDeadlineMs) >= 0;
}

bool SIM800TimeAsync::_atMatchedExpect() const {
	if (!_atPending || !_atExpect) return false;
	return _atResp.indexOf(_atExpect) >= 0;
}

// ---- PWRKEY ----
void SIM800TimeAsync::_pwrkeyIdle() {
	// Для NPN->GND важно,чтобы "кнопка" не была удержана.
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

void SIM800TimeAsync::_pwrkeyRelease() { _pwrkeyIdle(); }

// ---- time parse/valid ----
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
		int zz = v.substring(18).toInt(); // четверти часа
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