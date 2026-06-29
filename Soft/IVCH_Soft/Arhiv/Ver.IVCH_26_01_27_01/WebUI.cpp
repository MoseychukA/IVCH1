
#include "WebUI.h"

#include <IPAddress.h>
#include <Wire.h>
#include <SD.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// проектные заголовки
#include "LanIfStore.h"
#include "SyncSourcesStore.h"
#include "AT24C128Settings.h"
#include "TimeSyncPlanner.h"
#include "NetFeed.h"
#include "Internet2Client.h"
#include "NtpLanService_Generic.h"

// закрытие лог-файла (из вашего логгера)
extern void SdLogClose();

// LOG mode (from .ino):false=full log,true=time-only
extern bool gLogTimeOnly;

// RTC for purge age compare
extern RealtimeClock rtc;

// --- externs из проекта ---
extern LanIfStore lanStore;

extern SyncSourcesStore syncStore;
extern SyncSourcesStore::Data syncData;

extern AT24C128Settings ee;
extern AT24C128Settings::Config cfg;

extern TimeSyncPlanner planner;

extern void applyInternet1FromStore();
extern void applyInternet2FromStore();

extern NtpLanService_Generic ntpLan; // INTERNET1
extern Internet2Client internet2; // INTERNET2 (I2C)

// GSM globals
extern bool gNetRegistered;
extern int16_t gRssiDbm;
extern uint8_t gSignalBars;
extern int8_t gCsqRssi; // 0..31,99=unknown

// GPS globals
extern uint8_t gGpsSatsUsed;
extern uint8_t gGpsSatsView;
extern bool gGpsFix;

// --- списки ---
static const char* kNtpIpStr[5] = {
	"162.159.200.123",
	"162.159.200.1",
	"129.6.15.28",
	"132.163.96.1",
	"216.239.35.0"
};

static const char* kPeriodsName[6] = { "1 мин","10 мин","30 мин","1 час","6 часов","12 часов" };

// GSM providers (WEB):только 3 оператора
static const char* kGsmProviders[3] = { "МТС","МЕГАФОН","БИЛАЙН" };

// GSM presets (APN/USER/PASS):только 3 оператора
struct GsmPreset {
	const char* key;
	const char* title;
	const char* apn;
	const char* user;
	const char* pass;
};

static const GsmPreset kGsmPresets[] = {
	{ "megafon","МЕГАФОН","internet","","" },
	{ "mts","МТС","internet","mts","mts" },
	{ "beeline","БИЛАЙН","internet.beeline.ru","beeline","beeline" }
};

static int8_t gsmProviderIdxByKey(const char* key)
{
	if (!key) return -1;
	if (strcmp(key, "mts") == 0) return 0;
	if (strcmp(key, "megafon") == 0) return 1;
	if (strcmp(key, "beeline") == 0) return 2;
	return -1;
}

static const char* gsmPresetKeyByProviderIdx(uint8_t idx)
{
	switch (idx % 3) {
	case 0:return "mts";
	case 1:return "megafon";
	default:return "beeline";
	}
}

static bool applyGsmPresetByKey(const char* key)
{
	if (!key || !key[0]) return false;

	for (size_t i = 0; i < sizeof(kGsmPresets) / sizeof(kGsmPresets[0]); i++) {
		if (strcmp(kGsmPresets[i].key, key) == 0) {

			// 1) APN/USER/PASS
			strncpy(cfg.apn, kGsmPresets[i].apn, sizeof(cfg.apn) - 1);
			cfg.apn[sizeof(cfg.apn) - 1] = 0;

			strncpy(cfg.user, kGsmPresets[i].user, sizeof(cfg.user) - 1);
			cfg.user[sizeof(cfg.user) - 1] = 0;

			strncpy(cfg.pass, kGsmPresets[i].pass, sizeof(cfg.pass) - 1);
			cfg.pass[sizeof(cfg.pass) - 1] = 0;

			(void)ee.writeAPN(cfg.apn);
			(void)ee.writeUSER(cfg.user);
			(void)ee.writePASS(cfg.pass);

			// 2) also set GSM operator index in syncData
			int8_t op = gsmProviderIdxByKey(key);
			if (op >= 0) {
				syncData.gsmProviderIdx = (uint8_t)op;
				(void)syncStore.save(syncData);
			}

			return true;
		}
	}
	return false;
}

// ================== ctor ==================
WebUI::WebUI(uint16_t port) :_srv(port) {}

// ================== helpers ==================
bool WebUI::startsWith(const char* s, const char* pref)
{
	while (*pref) { if (*s++ != *pref++) return false; }
	return true;
}

bool WebUI::startsWithI(const char* s, const char* pref)
{
	auto up = [](char c)->char {
		if (c >= 'a' && c <= 'z') return (char)(c - 'a' + 'A');
		return c;
	};
	while (*pref) {
		if (up(*s++) != up(*pref++)) return false;
	}
	return true;
}

void WebUI::htmlEscPrint(EthernetClient& c, const char* s)
{
	for (; s && *s; s++) {
		switch (*s) {
		case '&':c.print(F("&amp;")); break;
		case '<':c.print(F("&lt;")); break;
		case '>':c.print(F("&gt;")); break;
		case '"':c.print(F("&quot;")); break;
		default:c.write(*s); break;
		}
	}
}

void WebUI::printIpOct(EthernetClient& c, const uint8_t ip[4])
{
	c.print(ip[0]); c.print('.');
	c.print(ip[1]); c.print('.');
	c.print(ip[2]); c.print('.');
	c.print(ip[3]);
}

void WebUI::trimSpaces(char* s)
{
	while (*s == ' ' || *s == '\t') memmove(s, s + 1, strlen(s));
	size_t n = strlen(s);
	while (n && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r' || s[n - 1] == '\n')) {
		s[n - 1] = 0;
		n--;
	}
}

// -------- log filename helpers (8.3 daily logs) --------
static bool endsWithStr(const char* s, const char* suf)
{
	if (!s || !suf) return false;
	const size_t ns = strlen(s);
	const size_t nf = strlen(suf);
	if (nf > ns) return false;
	return (strcmp(s + (ns - nf), suf) == 0);
}

static bool rtcDateValid(const RTCTime& t)
{
	return (t.year >= 2000 && t.year <= 2099 &&
		t.month >= 1 && t.month <= 12 &&
		t.dayOfMonth >= 1 && t.dayOfMonth <= 31);
}

// days-from-civil for age compare (date only)
static int32_t daysFromCivil(int y, int m, int d)
{
	y -= (m <= 2);
	const int era = (y >= 0 ? y : y - 399) / 400;
	const unsigned yoe = (unsigned)(y - era * 400);
	const unsigned doy = (153u * (unsigned)(m + (m > 2 ? -3 : 9)) + 2u) / 5u + (unsigned)d - 1u;
	const unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
	return (int32_t)(era * 146097 + (int)doe - 719468);
}

// Parse "IVyymmdd.LOG" -> y,m,d (2000+yy)
static bool parseDailyLogName(const char* nm, int& y, int& m, int& d)
{
	if (!nm) return false;
	if (strlen(nm) != 12) return false; // IVyymmdd.LOG
	if (!(nm[0] == 'I' && nm[1] == 'V')) return false;
	if (!(nm[8] == '.' && (nm[9] == 'L' || nm[9] == 'l') && (nm[10] == 'O' || nm[10] == 'o') && (nm[11] == 'G' || nm[11] == 'g'))) return false;

	auto dig = [](char c)->int { return (c >= '0' && c <= '9') ? (c - '0') : -1; };

	int yy1 = dig(nm[2]), yy2 = dig(nm[3]);
	int mm1 = dig(nm[4]), mm2 = dig(nm[5]);
	int dd1 = dig(nm[6]), dd2 = dig(nm[7]);
	if (yy1 < 0 || yy2 < 0 || mm1 < 0 || mm2 < 0 || dd1 < 0 || dd2 < 0) return false;

	int yy = yy1 * 10 + yy2;
	m = mm1 * 10 + mm2;
	d = dd1 * 10 + dd2;
	y = 2000 + yy;

	if (m < 1 || m > 12) return false;
	if (d < 1 || d > 31) return false;
	return true;
}

static void buildDailyLogName(char out[13], const RTCTime& t)
{
	uint8_t yy = (t.year >= 2000) ? (uint8_t)(t.year - 2000) : (uint8_t)(t.year % 100);
	snprintf(out, 13, "IV%02u%02u%02u.LOG", (unsigned)yy, (unsigned)t.month, (unsigned)t.dayOfMonth);
	out[12] = 0;
}

static bool isSafeLogFileName(const char* fn)
{
	if (!fn || !fn[0]) return false;
	const size_t n = strlen(fn);
	if (n > 48) return false;

	for (size_t i = 0; i < n; i++) {
		const char c = fn[i];
		if (c == '/' || c == '\\' || c == ':') return false;
		const bool ok =
			(c >= '0' && c <= '9') ||
			(c >= 'A' && c <= 'Z') ||
			(c >= 'a' && c <= 'z') ||
			(c == '_') || (c == '-') || (c == '.');
		if (!ok) return false;
	}
	if (!endsWithStr(fn, ".log") && !endsWithStr(fn, ".LOG")) return false;
	return true;
}

// ================== Base64 ==================
bool WebUI::base64Encode(char* out, size_t outN, const uint8_t* in, size_t inN)
{
	static const char* T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	size_t need = 4 * ((inN + 2) / 3) + 1;
	if (outN < need) return false;

	size_t wi = 0;
	for (size_t i = 0; i < inN; i += 3)
	{
		uint32_t v = 0;
		int rem = (int)(inN - i);
		if (rem >= 3) {
			v = ((uint32_t)in[i] << 16) | ((uint32_t)in[i + 1] << 8) | (uint32_t)in[i + 2];
			out[wi++] = T[(v >> 18) & 63];
			out[wi++] = T[(v >> 12) & 63];
			out[wi++] = T[(v >> 6) & 63];
			out[wi++] = T[v & 63];
		}
		else if (rem == 2) {
			v = ((uint32_t)in[i] << 16) | ((uint32_t)in[i + 1] << 8);
			out[wi++] = T[(v >> 18) & 63];
			out[wi++] = T[(v >> 12) & 63];
			out[wi++] = T[(v >> 6) & 63];
			out[wi++] = '=';
		}
		else {
			v = ((uint32_t)in[i] << 16);
			out[wi++] = T[(v >> 18) & 63];
			out[wi++] = T[(v >> 12) & 63];
			out[wi++] = '=';
			out[wi++] = '=';
		}
	}
	out[wi] = 0;
	return true;
}

// ================== EEPROM low level (0x50) ==================
bool WebUI::eepromWaitReady(uint32_t timeoutMs)
{
	uint32_t t0 = millis();
	while ((millis() - t0) < timeoutMs) {
		Wire.beginTransmission(EEPROM_ADDR);
		if (Wire.endTransmission() == 0) return true;
		delay(1);
	}
	return false;
}

bool WebUI::eepromRead(uint16_t memAddr, uint8_t* out, size_t len) const
{
	Wire.beginTransmission(EEPROM_ADDR);
	Wire.write((uint8_t)(memAddr >> 8));
	Wire.write((uint8_t)(memAddr & 0xFF));
	if (Wire.endTransmission(false) != 0) return false;

	size_t got = Wire.requestFrom((int)EEPROM_ADDR, (int)len);
	if (got != len) return false;

	for (size_t i = 0; i < len; i++) {
		int v = Wire.read();
		if (v < 0) return false;
		out[i] = (uint8_t)v;
	}
	return true;
}

bool WebUI::eepromWritePage(uint16_t memAddr, const uint8_t* data, size_t len)
{
	Wire.beginTransmission(EEPROM_ADDR);
	Wire.write((uint8_t)(memAddr >> 8));
	Wire.write((uint8_t)(memAddr & 0xFF));
	for (size_t i = 0; i < len; i++) Wire.write(data[i]);
	return (Wire.endTransmission() == 0);
}

bool WebUI::eepromWrite(uint16_t memAddr, const uint8_t* data, size_t len)
{
	size_t off = 0;
	while (off < len)
	{
		uint16_t a = (uint16_t)(memAddr + off);
		uint8_t pageOff = (uint8_t)(a % EEPROM_PAGE);
		size_t chunk = min((size_t)(EEPROM_PAGE - pageOff), len - off);

		if (!eepromWritePage(a, data + off, chunk)) return false;
		if (!eepromWaitReady(50)) return false;

		off += chunk;
	}
	return true;
}

// ================== Auth store (0x0190..) ==================
static constexpr uint16_t AUTH_BASE = 0x0190;
static constexpr uint16_t AUTH_MAGIC_ADDR = AUTH_BASE + 0; // u16
static constexpr uint16_t AUTH_VER_ADDR = AUTH_BASE + 2; // u8
static constexpr uint16_t AUTH_DATA_ADDR = AUTH_BASE + 4; // user[32],pass[32]
static constexpr uint16_t AUTH_MAGIC = 0x4157; // 'W''A'
static constexpr uint8_t AUTH_VER = 1;

bool WebUI::authLoad(AuthRec& out) const
{
	uint8_t b[2];
	if (!eepromRead(AUTH_MAGIC_ADDR, b, 2)) return false;
	uint16_t mg = (uint16_t)b[0] | ((uint16_t)b[1] << 8);
	if (mg != AUTH_MAGIC) return false;

	uint8_t ver = 0;
	if (!eepromRead(AUTH_VER_ADDR, &ver, 1)) return false;
	if (ver != AUTH_VER) return false;

	uint8_t raw[64];
	if (!eepromRead(AUTH_DATA_ADDR, raw, sizeof(raw))) return false;

	memcpy(out.user, raw + 0, 32);
	memcpy(out.pass, raw + 32, 32);
	out.user[31] = 0;
	out.pass[31] = 0;
	trimSpaces(out.user);
	trimSpaces(out.pass);
	return true;
}

bool WebUI::authSave(const AuthRec& in)
{
	AuthRec w = in;
	w.user[31] = 0;
	w.pass[31] = 0;

	uint8_t raw[64];
	memset(raw, 0, sizeof(raw));
	strncpy((char*)raw, w.user, 31);
	strncpy((char*)raw + 32, w.pass, 31);

	if (!eepromWrite(AUTH_DATA_ADDR, raw, sizeof(raw))) return false;

	uint8_t ver = AUTH_VER;
	if (!eepromWrite(AUTH_VER_ADDR, &ver, 1)) return false;

	uint8_t mg[2] = { (uint8_t)(AUTH_MAGIC & 0xFF),(uint8_t)(AUTH_MAGIC >> 8) };
	if (!eepromWrite(AUTH_MAGIC_ADDR, mg, 2)) return false;

	return true;
}

void WebUI::authLoadOrDefaults(AuthRec& out, bool& existed) const
{
	existed = authLoad(out);
	if (existed) return;

	memset(&out, 0, sizeof(out));
	strncpy(out.user, "admin", sizeof(out.user) - 1);
	strncpy(out.pass, "admin", sizeof(out.pass) - 1);
}

// ================== BasicAuth check ==================
bool WebUI::checkAuth(const char* authB64) const
{
	AuthRec a;
	bool existed = false;
	authLoadOrDefaults(a, existed);

	char up[70];
	snprintf(up, sizeof(up), "%s:%s", a.user, a.pass);

	char b64[120];
	if (!base64Encode(b64, sizeof(b64), (const uint8_t*)up, strlen(up))) return false;

	return (authB64 && strcmp(authB64, b64) == 0);
}

void WebUI::sendUnauthorized(EthernetClient& c)
{
	c.print(F("HTTP/1.1 401 Unauthorized\r\n"));
	c.print(F("Connection:close\r\n"));
	c.print(F("Cache-Control:no-store\r\n"));
	c.print(F("WWW-Authenticate:Basic realm=\"IVCH\"\r\n"));
	c.print(F("Content-Type:text/plain; charset=utf-8\r\n\r\n"));
	c.print(F("Требуется логин/пароль для доступа к WEB-интерфейсу"));
}

// ================== HTTP parsing ==================
bool WebUI::readLine(EthernetClient& c, char* out, size_t outN, uint32_t timeoutMs)
{
	uint32_t t0 = millis();
	size_t n = 0;

	while ((millis() - t0) < timeoutMs) {
		while (c.available()) {
			char ch = (char)c.read();
			if (ch == '\r') continue;
			if (ch == '\n') { out[n] = 0; return true; }
			if (n + 1 < outN) out[n++] = ch;
		}
	}
	out[0] = 0;
	return false;
}

bool WebUI::readHeaders(EthernetClient& c,
	char* authB64, size_t authB64N,
	size_t& contentLength,
	uint32_t timeoutMs)
{
	if (authB64 && authB64N) authB64[0] = 0;
	contentLength = 0;

	char line[192];
	while (true)
	{
		if (!readLine(c, line, sizeof(line), timeoutMs)) return false;
		if (line[0] == 0) return true;

		if (startsWithI(line, "Authorization:"))
		{
			const char* p = line + strlen("Authorization:");
			while (*p == ' ' || *p == '\t') p++;

			if (startsWithI(p, "Basic"))
			{
				p += 5;
				while (*p == ' ' || *p == '\t') p++;

				if (authB64 && authB64N)
				{
					strncpy(authB64, p, authB64N - 1);
					authB64[authB64N - 1] = 0;
					trimSpaces(authB64);
				}
			}
		}
		else if (startsWithI(line, "Content-Length:"))
		{
			const char* p = line + strlen("Content-Length:");
			while (*p == ' ' || *p == '\t') p++;
			long v = strtol(p, nullptr, 10);
			if (v > 0) contentLength = (size_t)v;
		}
	}
}

bool WebUI::readBody(EthernetClient& c, char* out, size_t outN, size_t contentLength, uint32_t timeoutMs)
{
	if (!out || outN == 0) return false;
	if (contentLength == 0) { out[0] = 0; return true; }

	if (contentLength + 1 > outN) return false;

	uint32_t t0 = millis();
	size_t n = 0;
	while (n < contentLength && (millis() - t0) < timeoutMs)
	{
		while (c.available() && n < contentLength)
		{
			out[n++] = (char)c.read();
		}
	}
	if (n != contentLength) return false;

	out[n] = 0;
	urlDecodeInPlace(out);
	return true;
}

// ================== common send helpers ==================
void WebUI::sendHeader(EthernetClient& c, int code, const __FlashStringHelper* ct)
{
	if (code == 200) c.print(F("HTTP/1.1 200 OK\r\n"));
	else if (code == 303) c.print(F("HTTP/1.1 303 See Other\r\n"));
	else c.print(F("HTTP/1.1 404 Not Found\r\n"));

	c.print(F("Connection:close\r\n"));
	c.print(F("Cache-Control:no-store\r\n"));
	c.print(F("Content-Type:"));
	c.print(ct);
	c.print(F("\r\n\r\n"));
}

void WebUI::sendRedirect(EthernetClient& c, const char* location)
{
	c.print(F("HTTP/1.1 303 See Other\r\n"));
	c.print(F("Connection:close\r\n"));
	c.print(F("Cache-Control:no-store\r\n"));
	c.print(F("Location:"));
	c.print(location);
	c.print(F("\r\n\r\n"));
}

void WebUI::urlDecodeInPlace(char* s)
{
	auto hex = [](char c)->int {
		if (c >= '0' && c <= '9') return c - '0';
		if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
		if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
		return -1;
	};

	char* w = s;
	for (char* r = s; *r; r++) {
		if (*r == '+') { *w++ = ' '; continue; }
		if (*r == '%' && r[1] && r[2]) {
			int hi = hex(r[1]), lo = hex(r[2]);
			if (hi >= 0 && lo >= 0) {
				*w++ = (char)((hi << 4) | lo);
				r += 2;
				continue;
			}
		}
		*w++ = *r;
	}
	*w = 0;
}

// ================== findParam safe for multiple calls ==================
const char* WebUI::findParam(char* query, const char* key)
{
	static char valBuf[8][256];
	static uint8_t idx = 0;

	if (!query || !key) return nullptr;

	char* out = valBuf[idx];
	idx = (uint8_t)((idx + 1) % 8);
	out[0] = 0;

	const size_t klen = strlen(key);
	const char* s = query;

	while (*s) {
		const char* endSeg = strchr(s, '&');
		if (!endSeg) endSeg = s + strlen(s);

		const char* eq = (const char*)memchr(s, '=', (size_t)(endSeg - s));
		if (eq) {
			const size_t keyLenHere = (size_t)(eq - s);
			if (keyLenHere == klen && strncmp(s, key, klen) == 0) {
				const char* v = eq + 1;
				size_t vlen = (size_t)(endSeg - v);
				if (vlen > 255) vlen = 255;
				memcpy(out, v, vlen);
				out[vlen] = 0;
				return out;
			}
		}

		if (*endSeg == 0) break;
		s = endSeg + 1;
	}
	return nullptr;
}

bool WebUI::hasParam(char* query, const char* key)
{
	return (findParam(query, key) != nullptr);
}

bool WebUI::parseBool01(const char* s, bool& out)
{
	if (!s) return false;
	if (strcmp(s, "0") == 0) { out = false; return true; }
	if (strcmp(s, "1") == 0) { out = true; return true; }
	return false;
}

bool WebUI::parseU8(const char* s, uint8_t& out, uint8_t minV, uint8_t maxV)
{
	if (!s) return false;
	long v = strtol(s, nullptr, 10);
	if (v < minV || v > maxV) return false;
	out = (uint8_t)v;
	return true;
}

static bool parseU32(const char* s, uint32_t& out, uint32_t minV, uint32_t maxV)
{
	if (!s) return false;
	unsigned long v = strtoul(s, nullptr, 10);
	if (v < minV || v > maxV) return false;
	out = (uint32_t)v;
	return true;
}

bool WebUI::parseI8(const char* s, int8_t& out, int8_t minV, int8_t maxV)
{
	if (!s) return false;
	long v = strtol(s, nullptr, 10);
	if (v < minV || v > maxV) return false;
	out = (int8_t)v;
	return true;
}

bool WebUI::parseIp4(const char* s, uint8_t out[4])
{
	if (!s) return false;
	int a, b, c, d;
	if (sscanf(s, "%d.%d.%d.%d", &a, &b, &c, &d) != 4) return false;
	if ((unsigned)a > 255 || (unsigned)b > 255 || (unsigned)c > 255 || (unsigned)d > 255) return false;
	out[0] = (uint8_t)a; out[1] = (uint8_t)b; out[2] = (uint8_t)c; out[3] = (uint8_t)d;
	return true;
}

// ================== lifecycle ==================
void WebUI::begin()
{
	AuthRec a;
	bool existed = false;
	authLoadOrDefaults(a, existed);
	if (!existed) (void)authSave(a);

	_srv.begin();
}

void WebUI::tick()
{
	EthernetClient c = _srv.available();
	if (!c) return;
	handleClient(c);
}

// ============================================================================
// MISSING helpers (were used but not defined in your file) <-- FIX
// ============================================================================

static void sendHtmlOkHeader(EthernetClient& c)
{
	c.print(F("HTTP/1.1 200 OK\r\n"));
	c.print(F("Connection:close\r\n"));
	c.print(F("Cache-Control:no-store\r\n"));
	c.print(F("Content-Type:text/html; charset=utf-8\r\n\r\n"));
}

static void sendText(EthernetClient& c, const __FlashStringHelper* msg)
{
	c.print(F("HTTP/1.1 200 OK\r\n"));
	c.print(F("Connection:close\r\n"));
	c.print(F("Cache-Control:no-store\r\n"));
	c.print(F("Content-Type:text/plain; charset=utf-8\r\n\r\n"));
	c.print(msg);
}

static void htmlHeader(EthernetClient& c, const __FlashStringHelper* title)
{
	c.print(F("<!doctype html><html><head><meta charset='utf-8'>"));
	c.print(F("<title>WEB интерфейс ИВЧ - "));
	c.print(title);
	c.print(F("</title>"));

	c.print(F("<style>"
		"body{font-family:Arial,Helvetica,sans-serif;font-size:12px;line-height:1.05;margin:4px;}"
		"h2{font-size:15px;margin:2px 0;}"
		"h3{font-size:13px;margin:2px 0 1px;}"
		"p{margin:1px 0;}"
		"hr{margin:4px 0;}"
		"code{background:#eee;padding:0 3px;}"
		"input,select,button{font-size:12px;padding:1px 3px;}"
		".pw{font-size:16px;padding:2px 4px;}"
		".nav a{margin-right:8px;}"
		".box{border:1px solid #ddd;padding:4px;margin:2px 0;}"
		"</style>"));

	c.print(F("<script>"
		"function togPass(id,cb){"
		"var e=document.getElementById(id);"
		"if(!e)return;"
		"e.type=cb.checked?'text':'password';"
		"}"
		"</script>"));

	c.print(F("</head><body>"));
	c.print(F("<h2>WEB интерфейс ИВЧ</h2>"));

	c.print(F("<div class='nav'>"
		"<a href='/'>СТАТУС</a>"
		"<a href='/cfg'>НАСТРОЙКИ</a>"
		"<a href='/gsm'>GSM</a>"
		"<a href='/log'>LOG</a>"
		"</div><hr>"));
}

static void htmlFooter(EthernetClient& c)
{
	c.print(F("</body></html>"));
}

// -------------------- LOG pages --------------------
static void sendLogFile(EthernetClient& c, const char* fileName)
{
	if (!fileName || !fileName[0] || !isSafeLogFileName(fileName)) {
		sendText(c, F("Bad file name\n"));
		return;
	}

	File f = SD.open(fileName, FILE_READ);
	if (!f) {
		sendText(c, F("LOG file not found\n"));
		return;
	}

	c.print(F("HTTP/1.1 200 OK\r\n"));
	c.print(F("Connection:close\r\n"));
	c.print(F("Cache-Control:no-store\r\n"));
	c.print(F("Content-Type:text/plain; charset=utf-8\r\n"));
	c.print(F("Content-Disposition:attachment; filename=\""));
	c.print(fileName);
	c.print(F("\"\r\n"));
	c.print(F("\r\n"));

	uint8_t buf[256];
	while (true) {
		int nrd = f.read(buf, sizeof(buf));
		if (nrd <= 0) break;
		c.write(buf, (size_t)nrd);
	}
	f.close();
}

static uint16_t clampDays(uint32_t v)
{
	if (v < 1) return 1;
	if (v > 3650) return 3650;
	return (uint16_t)v;
}

static uint32_t purgeOldLogs(uint16_t keepDays)
{
	RTCTime t = rtc.getTime();
	if (!rtcDateValid(t)) return 0;

	char todayName[13];
	buildDailyLogName(todayName, t);

	const int32_t nowDays = daysFromCivil((int)t.year, (int)t.month, (int)t.dayOfMonth);
	uint32_t deleted = 0;

	File root = SD.open("/");
	if (!root) return 0;

	while (true) {
		File f = root.openNextFile();
		if (!f) break;

		if (f.isDirectory()) { f.close(); continue; }

		const char* nm = f.name();
		f.close();
		if (!nm) continue;

		if (!endsWithStr(nm, ".LOG") && !endsWithStr(nm, ".log")) continue;

		if (strcmp(nm, todayName) == 0) continue;

		if (strcmp(nm, "IVNORRTC.LOG") == 0) {
			if (SD.remove(nm)) deleted++;
			continue;
		}

		int y, m, d;
		if (!parseDailyLogName(nm, y, m, d)) continue;

		const int32_t fileDays = daysFromCivil(y, m, d);
		const int32_t age = nowDays - fileDays;
		if (age > (int32_t)keepDays) {
			if (SD.remove(nm)) deleted++;
		}
	}

	root.close();
	return deleted;
}

static void pageLogPurgeConfirm(EthernetClient& c, uint16_t keepDays)
{
	sendHtmlOkHeader(c);
	htmlHeader(c, F("УПРАВЛЕНИЕ ЛОГАМИ"));

	RTCTime t = rtc.getTime();
	if (!rtcDateValid(t)) {
		c.print(F("<div class='box'><h3>RTC не установлен</h3>"
			"<p>Невозможно определить возраст логов без даты RTC.</p>"
			"<p><a href='/log'><button>НАЗАД</button></a></p></div>"));
		htmlFooter(c);
		return;
	}

	c.print(F("<div class='box'><h3>Удалить логи старше N дней?</h3>"));
	c.print(F("<p>Хранить последние дней:<code>"));
	c.print((unsigned)keepDays);
	c.print(F("</code></p>"));

	c.print(F("<p>"
		"<a href='/log/purge?confirm=1&days="));
	c.print((unsigned)keepDays);
	c.print(F("'><button>ДА,УДАЛИТЬ</button></a> "
		"<a href='/log'><button>НЕТ</button></a>"
		"</p></div>"));

	htmlFooter(c);
}

static void pageLogClearConfirm(EthernetClient& c, const char* fileName)
{
	sendHtmlOkHeader(c);
	htmlHeader(c, F("УПРАВЛЕНИЕ ЛОГАМИ"));

	const char* fn = (fileName && fileName[0]) ? fileName : "IVNORRTC.LOG";

	c.print(F("<div class='box'>"
		"<h3>Очистить лог-файл?</h3><p><code>"));
	WebUI::htmlEscPrint(c, fn);
	c.print(F("</code></p><p>"
		"<a href='/log/clear?confirm=1&file="));
	WebUI::htmlEscPrint(c, fn);
	c.print(F("'><button>ДА,ОЧИСТИТЬ</button></a> "
		"<a href='/log'><button>НЕТ</button></a>"
		"</p></div>"));

	htmlFooter(c);
}

static void pageLogMain(EthernetClient& c)
{
	sendHtmlOkHeader(c);
	htmlHeader(c, F("УПРАВЛЕНИЕ ЛОГАМИ"));

	// LOG MODE controls (moved here from /cfg)
	c.print(F("<div class='box'><h3>Режим логирования</h3>"));
	c.print(F("<form method='post' action='/log/set'>"));
	c.print(F("<p>Режим:<select name='log_time_only'>"));
	c.print(F("<option value='0'"));
	if (!gLogTimeOnly) c.print(F(" selected"));
	c.print(F(">FULL (всё)</option>"));
	c.print(F("<option value='1'"));
	if (gLogTimeOnly) c.print(F(" selected"));
	c.print(F(">TIME ONLY (только TIME REQ/RECV/OK/FAIL)</option>"));
	c.print(F("</select> <button type='submit'>ПРИМЕНИТЬ</button></p>"));
	c.print(F("</form></div>"));

	// Purge controls
	c.print(F("<div class='box'><h3>Удаление старых логов</h3>"));
	c.print(F("<form method='get' action='/log/purge'>"
		"<p>Удалить логи старше <input name='days' value='30' size='4'> дней "
		"<button type='submit'>ПРОВЕРИТЬ</button></p>"
		"</form></div>"));

	// List files
	c.print(F("<div class='box'><h3>Файлы логов на SD</h3>"));
	c.print(F("<p><small>Нажмите на имя для скачивания. Очистка — по ссылке.</small></p>"));

	File root = SD.open("/");
	if (!root) {
		c.print(F("<p><b>Ошибка:</b> SD.open(\"/\")</p></div>"));
		htmlFooter(c);
		return;
	}

	c.print(F("<table cellpadding='2' cellspacing='0' style='border-collapse:collapse;'>"));
	c.print(F("<tr><th align='left'>Файл</th><th align='right'>Размер</th><th align='left'>Действия</th></tr>"));

	while (true) {
		File f = root.openNextFile();
		if (!f) break;

		if (f.isDirectory()) { f.close(); continue; }

		const char* nm = f.name();
		if (!nm) { f.close(); continue; }

		if (!endsWithStr(nm, ".log") && !endsWithStr(nm, ".LOG")) { f.close(); continue; }
		if (!isSafeLogFileName(nm)) { f.close(); continue; }

		uint32_t sz = (uint32_t)f.size();
		f.close();

		c.print(F("<tr style='border-top:1px solid #eee;'><td>"));
		c.print(F("<a href='/log?file="));
		WebUI::htmlEscPrint(c, nm);
		c.print(F("'><code>"));
		WebUI::htmlEscPrint(c, nm);
		c.print(F("</code></a>"));
		c.print(F("</td><td align='right'><code>"));
		c.print((unsigned long)sz);
		c.print(F("</code></td><td>"));
		c.print(F("<a href='/log/clear?file="));
		WebUI::htmlEscPrint(c, nm);
		c.print(F("'>очистить</a>"));
		c.print(F("</td></tr>"));
	}

	root.close();

	c.print(F("</table></div>"));
	htmlFooter(c);
}

// ================== routing ==================
void WebUI::handleClient(EthernetClient& c)
{
	char req[192];
	if (!readLine(c, req, sizeof(req), 1500)) { c.stop(); return; }

	bool isGet = startsWith(req, "GET ");
	bool isPost = startsWith(req, "POST ");
	if (!isGet && !isPost) { sendHeader(c, 404, F("text/plain; charset=utf-8")); c.print(F("Неверный метод")); c.stop(); return; }

	const char* p = isGet ? (req + 4) : (req + 5);
	char path[160];
	size_t i = 0;
	while (*p && *p != ' ' && i + 1 < sizeof(path)) path[i++] = *p++;
	path[i] = 0;

	char authB64[140];
	size_t contentLen = 0;
	if (!readHeaders(c, authB64, sizeof(authB64), contentLen, 1500)) { c.stop(); return; }

	if (!checkAuth(authB64))
	{
		sendUnauthorized(c);
		c.stop();
		return;
	}

	char* q = strchr(path, '?');
	char* params = nullptr;

	static char bodyBuf[900];
	bodyBuf[0] = 0;

	if (q) { *q = 0; params = q + 1; urlDecodeInPlace(params); }

	if (isPost)
	{
		if (contentLen > 0)
		{
			if (!readBody(c, bodyBuf, sizeof(bodyBuf), contentLen, 2000))
			{
				sendHeader(c, 200, F("text/plain; charset=utf-8"));
				c.print(F("Ошибка чтения POST данных (слишком длинно или таймаут)"));
				c.stop();
				return;
			}
			params = bodyBuf;
		}
	}

	// Routes
	if (isGet && strcmp(path, "/") == 0) pageStatus(c);
	else if (isGet && strcmp(path, "/cfg") == 0) pageCfg(c);
	else if (isGet && strcmp(path, "/gsm") == 0) pageGsm(c);

	// LOG main / download
	else if (isGet && strcmp(path, "/log") == 0)
	{
		if (params && hasParam(params, "file")) {
			const char* fn = findParam(params, "file");
			sendLogFile(c, fn);
			c.stop();
			return;
		}

		pageLogMain(c);
		c.stop();
		return;
	}

	// LOG mode set (POST)
	else if (isPost && strcmp(path, "/log/set") == 0)
	{
		if (params) {
			bool b01;
			if (parseBool01(findParam(params, "log_time_only"), b01)) {
				gLogTimeOnly = b01;
			}
		}
		sendRedirect(c, "/log");
		c.stop();
		return;
	}

	// LOG clear
	else if (isGet && strcmp(path, "/log/clear") == 0)
	{
		const char* fn = (params && hasParam(params, "file")) ? findParam(params, "file") : nullptr;
		if (!fn || !fn[0]) fn = "IVNORRTC.LOG";

		if (!params || !hasParam(params, "confirm")) {
			pageLogClearConfirm(c, fn);
			c.stop();
			return;
		}

		if (!isSafeLogFileName(fn)) {
			sendText(c, F("Bad file name\n"));
			c.stop();
			return;
		}

		bool ok = false;
		SdLogClose();

		if (SD.exists(fn)) ok = SD.remove(fn);
		else ok = true;

		File nf = SD.open(fn, FILE_WRITE);
		if (nf) nf.close();

		if (ok) sendRedirect(c, "/log");
		else sendText(c, F("LOG clear FAILED (file may be open). Close logger or reboot and retry.\n"));

		c.stop();
		return;
	}

	// LOG purge
	else if (isGet && strcmp(path, "/log/purge") == 0)
	{
		uint16_t keepDays = 30;
		if (params && hasParam(params, "days")) {
			uint32_t d = 0;
			if (parseU32(findParam(params, "days"), d, 1UL, 3650UL)) keepDays = clampDays(d);
		}

		if (!params || !hasParam(params, "confirm")) {
			pageLogPurgeConfirm(c, keepDays);
			c.stop();
			return;
		}

		SdLogClose();
		uint32_t delCount = purgeOldLogs(keepDays);

		sendHtmlOkHeader(c);
		htmlHeader(c, F("УПРАВЛЕНИЕ ЛОГАМИ"));
		c.print(F("<div class='box'><h3>Готово</h3><p>Удалено файлов:<code>"));
		c.print((unsigned long)delCount);
		c.print(F("</code></p><p><a href='/log'><button>НАЗАД</button></a></p></div>"));
		htmlFooter(c);

		c.stop();
		return;
	}

	else if (isGet && strcmp(path, "/api/status") == 0) apiStatus(c);
	else if (isGet && strcmp(path, "/api/config") == 0) apiConfig(c);

	else if (isPost && strcmp(path, "/cfg/save") == 0) actionCfgSave(c, params);
	else if (isPost && strcmp(path, "/gsm/save") == 0) actionGsmSave(c, params);

	else pageNotFound(c);

	c.stop();
}

// ================== Pages ==================
void WebUI::pageStatus(EthernetClient& c)
{
	sendHeader(c, 200, F("text/html; charset=utf-8"));
	htmlHeader(c, F("СТАТУС"));

	c.print(F("<div class='box'><h3>LOG</h3>"));
	c.print(F("<p>Режим:<code>"));
	c.print(gLogTimeOnly ? "TIME ONLY" : "FULL");
	c.print(F("</code></p></div>"));

	c.print(F("<div class='box'><h3>ИНТЕРНЕТ 1</h3>"));
	c.print(F("IP:<code>")); c.print(ntpLan.localIP()); c.print(F("</code><br>"));
	c.print(F("Линк:<code>")); c.print(Ethernet.linkReport()); c.print(F("</code>"));
	c.print(F("</div>"));

	Internet2Client::Status st{};
	bool ok2 = internet2.readStatus(st);

	c.print(F("<div class='box'><h3>ИНТЕРНЕТ 2</h3>"));
	c.print(F("Статус чтения:<code>")); c.print(ok2 ? "OK" : "ОШИБКА"); c.print(F("</code><br>"));
	c.print(F("IP:<code>")); c.print(ok2 ? st.ip : IPAddress(0, 0, 0, 0)); c.print(F("</code><br>"));
	c.print(F("Последняя синхронизация:<code>")); c.print((ok2 && st.lastSyncOk) ? "УСПЕХ" : "НЕТ"); c.print(F("</code>"));
	c.print(F("</div>"));

	c.print(F("<div class='box'><h3>GSM</h3>"));
	c.print(F("Регистрация:<code>")); c.print(gNetRegistered ? "ДА" : "НЕТ"); c.print(F("</code><br>"));

	const bool rssiUnknown = (gCsqRssi == 99) || (gRssiDbm == 0);
	if (rssiUnknown) c.print(F("RSSI:<code>--</code><br>"));
	else {
		c.print(F("RSSI:<code>")); c.print((int)gRssiDbm); c.print(F(" dBm</code><br>"));
	}
	uint8_t bars = gSignalBars;
	if (bars > 5) bars = 5;
	c.print(F("Уровень:<code>")); c.print((int)bars); c.print(F("/5</code>"));
	c.print(F("</div>"));

	c.print(F("<div class='box'><h3>GPS</h3>"));
	c.print(F("Fix:<code>")); c.print(gGpsFix ? "ДА" : "НЕТ"); c.print(F("</code><br>"));
	c.print(F("Спутники:<code>"));
	c.print((int)gGpsSatsUsed); c.print('/'); c.print((int)gGpsSatsView);
	c.print(F("</code>"));
	c.print(F("</div>"));

	htmlFooter(c);
}

static void printIpInput(EthernetClient& c, const char* name, uint8_t ip[4])
{
	c.print(F("<input name='"));
	c.print(name);
	c.print(F("' value='"));
	c.print(ip[0]); c.print('.');
	c.print(ip[1]); c.print('.');
	c.print(ip[2]); c.print('.');
	c.print(ip[3]);
	c.print(F("'>"));
}

static void printSelectU8(EthernetClient& c, const char* name, uint8_t cur, uint8_t count, const char* const* labels)
{
	c.print(F("<select name='")); c.print(name); c.print(F("'>"));
	for (uint8_t i = 0; i < count; i++) {
		c.print(F("<option value='")); c.print(i); c.print(F("'"));
		if (i == cur) c.print(F(" selected"));
		c.print(F(">"));
		WebUI::htmlEscPrint(c, labels[i]);
		c.print(F("</option>"));
	}
	c.print(F("</select>"));
}

void WebUI::pageCfg(EthernetClient& c)
{
	LanIfStore::IfConfig if1{}, if2{};
	bool okIf1 = lanStore.load(LanIfStore::IF1, if1, true);
	if (!okIf1) LanIfStore::defaults(LanIfStore::IF1, if1);
	bool okIf2 = lanStore.load(LanIfStore::IF2, if2, true);
	if (!okIf2) LanIfStore::defaults(LanIfStore::IF2, if2);

	AuthRec a;
	bool existed = false;
	authLoadOrDefaults(a, existed);

	sendHeader(c, 200, F("text/html; charset=utf-8"));
	htmlHeader(c, F("НАСТРОЙКИ"));

	Internet2Client::Status st{};
	bool ok2 = internet2.readStatus(st);

	c.print(F("<div class='box'><h3>ТЕКУЩИЕ (реальные)</h3>"));
	c.print(F("INTERNET1 IP:<code>")); c.print(ntpLan.localIP()); c.print(F("</code> / LINK:<code>"));
	c.print(Ethernet.linkReport()); c.print(F("</code><br>"));
	c.print(F("INTERNET2 IP:<code>")); c.print(ok2 ? st.ip : IPAddress(0, 0, 0, 0)); c.print(F("</code>"));
	c.print(F("</div>"));

	c.print(F("<form method='post' action='/cfg/save'>"));

	// TZ
	c.print(F("<div class='box'><h3>Часовые пояса</h3>"));
	c.print(F("<p>Часовой пояс (-12..14):<input name='tzTarget' value='"));
	c.print((int)cfg.tzTargetHours);
	c.print(F("'></p>"));
	c.print(F("<p>Часовой пояс NTP (-12..14):<input name='tzNtp' value='"));
	c.print((int)cfg.tzNtpHours);
	c.print(F("'></p>"));
	c.print(F("</div>"));

	// SOURCES
	c.print(F("<div class='box'><h3>Источники синхронизации</h3>"));
	c.print(F("<p><small>Можно включить несколько. Приоритет:GPS → INTERNET1 → INTERNET2 → GSM</small></p>"));

	c.print(F("<p>"));
	auto cb = [&](const char* name, const char* label, bool checked) {
		c.print(F("<label><input type='checkbox' name='"));
		c.print(name);
		c.print(F("' value='1'"));
		if (checked) c.print(F(" checked"));
		c.print(F("> "));
		c.print(label);
		c.print(F("</label> "));
	};
	cb("src_gps", "GPS", syncData.gpsEnable != 0);
	cb("src_net1", "INTERNET1", syncData.netEnable != 0);
	cb("src_net2", "INTERNET2", syncData.net2Enable != 0);
	cb("src_gsm", "GSM", syncData.gsmEnable != 0);
	c.print(F("</p>"));

	c.print(F("<p>GPS период:"));
	printSelectU8(c, "gps_per", syncData.gpsPeriodIdx % 6, 6, kPeriodsName);
	c.print(F("</p>"));

	c.print(F("<p>INTERNET1 NTP сервер:"));
	printSelectU8(c, "net1_ntp", syncData.netProviderIdx % 5, 5, kNtpIpStr);
	c.print(F("</p>"));
	c.print(F("<p>INTERNET1 период:"));
	printSelectU8(c, "net1_per", syncData.netPeriodIdx % 6, 6, kPeriodsName);
	c.print(F("</p>"));

	c.print(F("<p>INTERNET2 период:"));
	printSelectU8(c, "net2_per", syncData.net2PeriodIdx % 6, 6, kPeriodsName);
	c.print(F("</p>"));

	c.print(F("<p>GSM оператор:"));
	printSelectU8(c, "gsm_op", syncData.gsmProviderIdx % 3, 3, kGsmProviders);
	c.print(F("</p>"));
	c.print(F("<p>GSM период:"));
	printSelectU8(c, "gsm_per", syncData.gsmPeriodIdx % 6, 6, kPeriodsName);
	c.print(F("</p>"));

	c.print(F("</div>"));

	// SIM800
	c.print(F("<div class='box'><h3>SIM800 (APN/NTP)</h3>"));
	c.print(F("<p>APN:<input name='apn' value='")); htmlEscPrint(c, cfg.apn); c.print(F("'></p>"));
	c.print(F("<p>USER:<input name='apn_user' value='")); htmlEscPrint(c, cfg.user); c.print(F("'></p>"));

	c.print(F("<p>PASS:<input id='apn_pass_cfg' type='password' name='apn_pass' value='"));
	htmlEscPrint(c, cfg.pass);
	c.print(F("'> <label><input type='checkbox' onclick=\"togPass('apn_pass_cfg',this)\"> показать</label></p>"));

	c.print(F("<p>NTP server:<input name='ntp_server' value='")); htmlEscPrint(c, cfg.server); c.print(F("'></p>"));

	c.print(F("<p>Fallback (0/1):<input name='enFall' value='"));
	c.print(cfg.enableFallback ? 1 : 0);
	c.print(F("'></p>"));

	c.print(F("<p>PeriodMs (1000..86400000):<input name='simPeriodMs' value='"));
	c.print((unsigned long)cfg.periodMs);
	c.print(F("'></p>"));
	c.print(F("</div>"));

	// INTERNET1 store
	c.print(F("<div class='box'><h3>INTERNET1 (EEPROM сеть)</h3>"));
	c.print(F("<p>DHCP:<select name='if1_dhcp'>"));
	c.print(F("<option value='1'")); if (if1.dhcp) c.print(F(" selected")); c.print(F(">ON</option>"));
	c.print(F("<option value='0'")); if (!if1.dhcp) c.print(F(" selected")); c.print(F(">OFF</option>"));
	c.print(F("</select></p>"));

	c.print(F("<p>NTP-сервер:"));
	printSelectU8(c, "if1_ntpIdx", if1.ntpIdx % 5, 5, kNtpIpStr);
	c.print(F("</p>"));

	c.print(F("<p>Период:"));
	printSelectU8(c, "if1_perIdx", if1.periodIdx % 6, 6, kPeriodsName);
	c.print(F("</p>"));

	c.print(F("<p>IP:")); printIpInput(c, "if1_ip", if1.ip); c.print(F("</p>"));
	c.print(F("<p>MASK:")); printIpInput(c, "if1_mask", if1.mask); c.print(F("</p>"));
	c.print(F("<p>GW:")); printIpInput(c, "if1_gw", if1.gw); c.print(F("</p>"));
	c.print(F("<p>DNS:")); printIpInput(c, "if1_dns", if1.dns); c.print(F("</p>"));
	c.print(F("</div>"));

	// INTERNET2 store
	c.print(F("<div class='box'><h3>INTERNET2 (EEPROM сеть)</h3>"));
	c.print(F("<p>DHCP:<select name='if2_dhcp'>"));
	c.print(F("<option value='1'")); if (if2.dhcp) c.print(F(" selected")); c.print(F(">ON</option>"));
	c.print(F("<option value='0'")); if (!if2.dhcp) c.print(F(" selected")); c.print(F(">OFF</option>"));
	c.print(F("</select></p>"));

	c.print(F("<p>NTP-сервер:"));
	printSelectU8(c, "if2_ntpIdx", if2.ntpIdx % 5, 5, kNtpIpStr);
	c.print(F("</p>"));

	c.print(F("<p>Период:"));
	printSelectU8(c, "if2_perIdx", if2.periodIdx % 6, 6, kPeriodsName);
	c.print(F("</p>"));

	c.print(F("<p>IP:")); printIpInput(c, "if2_ip", if2.ip); c.print(F("</p>"));
	c.print(F("<p>MASK:")); printIpInput(c, "if2_mask", if2.mask); c.print(F("</p>"));
	c.print(F("<p>GW:")); printIpInput(c, "if2_gw", if2.gw); c.print(F("</p>"));
	c.print(F("<p>DNS:")); printIpInput(c, "if2_dns", if2.dns); c.print(F("</p>"));
	c.print(F("</div>"));

	// WEB auth
	c.print(F("<div class='box'><h3>Доступ к WEB-интерфейсу (логин/пароль)</h3>"));
	c.print(F("<p>Текущий логин:<code>")); c.print(a.user); c.print(F("</code></p>"));
	c.print(F("<p>Новый логин:<input class='pw' name='web_user' value=''></p>"));
	c.print(F("<p>Новый пароль:<input class='pw' type='password' name='web_pass' value=''></p>"));
	c.print(F("<p><small>Введите оба поля,чтобы изменить логин/пароль.</small></p>"));
	c.print(F("</div>"));

	c.print(F("<p><button type='submit'>СОХРАНИТЬ + ПРИМЕНИТЬ</button></p>"));
	c.print(F("</form>"));

	htmlFooter(c);
}

void WebUI::pageGsm(EthernetClient& c)
{
	sendHeader(c, 200, F("text/html; charset=utf-8"));
	htmlHeader(c, F("НАСТРОЙКИ GSM"));

	c.print(F("<div class='box'><h3>Статус GSM</h3>"));
	c.print(F("Регистрация:<code>")); c.print(gNetRegistered ? "ДА" : "НЕТ"); c.print(F("</code><br>"));

	const bool rssiUnknown = (gCsqRssi == 99) || (gRssiDbm == 0);
	if (rssiUnknown) c.print(F("RSSI:<code>--</code><br>"));
	else {
		c.print(F("RSSI:<code>")); c.print((int)gRssiDbm); c.print(F(" dBm</code><br>"));
	}

	uint8_t bars = gSignalBars;
	if (bars > 5) bars = 5;
	c.print(F("Уровень:<code>")); c.print((int)bars); c.print(F("/5</code>"));
	c.print(F("</div>"));

	c.print(F("<div class='box'><h3>Профиль оператора (APN)</h3>"));
	c.print(F("<p><small>Примерные значения. Уточняйте у оператора/по тарифу.</small></p>"));

	c.print(F("<table cellpadding='2' cellspacing='0' style='border-collapse:collapse;'>"
		"<tr><th align='left'>Оператор</th><th align='left'>APN</th><th align='left'>USER</th><th align='left'>PASS</th></tr>"));
	for (size_t i = 0; i < sizeof(kGsmPresets) / sizeof(kGsmPresets[0]); i++) {
		c.print(F("<tr><td style='border-top:1px solid #eee;'>"));
		c.print(kGsmPresets[i].title);
		c.print(F("</td><td style='border-top:1px solid #eee;'><code>"));
		c.print(kGsmPresets[i].apn);
		c.print(F("</code></td><td style='border-top:1px solid #eee;'><code>"));
		c.print(kGsmPresets[i].user[0] ? kGsmPresets[i].user : "-");
		c.print(F("</code></td><td style='border-top:1px solid #eee;'><code>"));
		c.print(kGsmPresets[i].pass[0] ? kGsmPresets[i].pass : "-");
		c.print(F("</code></td></tr>"));
	}
	c.print(F("</table>"));

	const char* curKey = gsmPresetKeyByProviderIdx(syncData.gsmProviderIdx);

	c.print(F("<form method='post' action='/gsm/save'>"));
	c.print(F("<input type='hidden' name='doPreset' value='1'>"));
	c.print(F("<p>Выбрать профиль:<select name='preset'>"));
	for (size_t i = 0; i < sizeof(kGsmPresets) / sizeof(kGsmPresets[0]); i++) {
		c.print(F("<option value='")); c.print(kGsmPresets[i].key); c.print(F("'"));
		if (curKey && strcmp(curKey, kGsmPresets[i].key) == 0) c.print(F(" selected"));
		c.print(F(">"));
		c.print(kGsmPresets[i].title);
		c.print(F("</option>"));
	}
	c.print(F("</select> "));
	c.print(F("<button type='submit'>ПРИМЕНИТЬ ПРОФИЛЬ</button></p>"));
	c.print(F("</form>"));
	c.print(F("</div>"));

	c.print(F("<div class='box'><h3>Настройки GSM (ручной ввод)</h3>"));
	c.print(F("<form method='post' action='/gsm/save'>"));

	c.print(F("<p>Включить GSM:<select name='gsm_en'>"));
	c.print(F("<option value='1'")); if (syncData.gsmEnable) c.print(F(" selected")); c.print(F(">ВКЛ</option>"));
	c.print(F("<option value='0'")); if (!syncData.gsmEnable) c.print(F(" selected")); c.print(F(">ВЫКЛ</option>"));
	c.print(F("</select></p>"));

	c.print(F("<p>Оператор:<select name='gsm_op'>"));
	for (uint8_t i = 0; i < 3; i++) {
		c.print(F("<option value='")); c.print(i); c.print(F("'"));
		if ((syncData.gsmProviderIdx % 3) == i) c.print(F(" selected"));
		c.print(F(">"));
		WebUI::htmlEscPrint(c, kGsmProviders[i]);
		c.print(F("</option>"));
	}
	c.print(F("</select></p>"));

	c.print(F("<p>Период синхронизации:<select name='gsm_per'>"));
	for (uint8_t i = 0; i < 6; i++) {
		c.print(F("<option value='")); c.print(i); c.print(F("'"));
		if ((syncData.gsmPeriodIdx % 6) == i) c.print(F(" selected"));
		c.print(F(">"));
		WebUI::htmlEscPrint(c, kPeriodsName[i]);
		c.print(F("</option>"));
	}
	c.print(F("</select></p>"));

	c.print(F("<hr>"));
	c.print(F("<p>APN:<input name='apn' value='")); WebUI::htmlEscPrint(c, cfg.apn); c.print(F("'></p>"));
	c.print(F("<p>USER:<input name='apn_user' value='")); WebUI::htmlEscPrint(c, cfg.user); c.print(F("'></p>"));

	c.print(F("<p>PASS:<input id='apn_pass_gsm' type='password' name='apn_pass' value='"));
	WebUI::htmlEscPrint(c, cfg.pass);
	c.print(F("'> <label><input type='checkbox' onclick=\"togPass('apn_pass_gsm',this)\"> показать</label></p>"));

	c.print(F("<p>NTP server:<input name='ntp_server' value='")); WebUI::htmlEscPrint(c, cfg.server); c.print(F("'></p>"));

	c.print(F("<p><button type='submit'>СОХРАНИТЬ</button></p>"));
	c.print(F("</form></div>"));

	htmlFooter(c);
}

void WebUI::pageNotFound(EthernetClient& c)
{
	sendHeader(c, 404, F("text/plain; charset=utf-8"));
	c.print(F("404 Не найдено"));
}

// ================== JSON ==================
static void jsonStr(EthernetClient& c, const char* s)
{
	c.print('"');
	for (; s && *s; s++) {
		if (*s == '"' || *s == '\\') { c.print('\\'); c.print(*s); }
		else if (*s == '\n') c.print(F("\\n"));
		else if (*s == '\r') c.print(F("\\r"));
		else c.print(*s);
	}
	c.print('"');
}

void WebUI::apiStatus(EthernetClient& c)
{
	sendHeader(c, 200, F("application/json; charset=utf-8"));

	Internet2Client::Status st{};
	bool ok2 = internet2.readStatus(st);

	String ip1s = ntpLan.localIP().toString();
	String ip2s = (ok2 ? st.ip : IPAddress(0, 0, 0, 0)).toString();

	c.print(F("{"));
	c.print(F("\"ip1\":")); jsonStr(c, ip1s.c_str()); c.print(F(","));
	c.print(F("\"link1\":")); jsonStr(c, Ethernet.linkReport()); c.print(F(","));
	c.print(F("\"internet2_ok\":")); c.print(ok2 ? F("true") : F("false")); c.print(F(","));
	c.print(F("\"ip2\":")); jsonStr(c, ip2s.c_str()); c.print(F(","));
	c.print(F("\"tz_target\":")); c.print((int)cfg.tzTargetHours); c.print(F(","));
	c.print(F("\"gps_en\":")); c.print(syncData.gpsEnable ? F("true") : F("false")); c.print(F(","));
	c.print(F("\"net1_en\":")); c.print(syncData.netEnable ? F("true") : F("false")); c.print(F(","));
	c.print(F("\"net2_en\":")); c.print(syncData.net2Enable ? F("true") : F("false")); c.print(F(","));
	c.print(F("\"gsm_en\":")); c.print(syncData.gsmEnable ? F("true") : F("false"));
	c.print(F("}"));
}

void WebUI::apiConfig(EthernetClient& c)
{
	sendHeader(c, 200, F("application/json; charset=utf-8"));

	c.print(F("{"));
	c.print(F("\"tz_target\":")); c.print((int)cfg.tzTargetHours); c.print(F(","));
	c.print(F("\"tz_ntp\":")); c.print((int)cfg.tzNtpHours); c.print(F(","));
	c.print(F("\"sources\":{"));
	c.print(F("\"gps\":")); c.print(syncData.gpsEnable ? F("true") : F("false")); c.print(F(","));
	c.print(F("\"internet1\":")); c.print(syncData.netEnable ? F("true") : F("false")); c.print(F(","));
	c.print(F("\"internet2\":")); c.print(syncData.net2Enable ? F("true") : F("false")); c.print(F(","));
	c.print(F("\"gsm\":")); c.print(syncData.gsmEnable ? F("true") : F("false"));
	c.print(F("}"));
	c.print(F("}"));
}

// ================== Actions ==================
void WebUI::actionCfgSave(EthernetClient& c, char* params)
{
	if (!params) { sendRedirect(c, "/cfg"); return; }

	// TZ
	int8_t tzT, tzN;
	if (parseI8(findParam(params, "tzTarget"), tzT, -12, 14)) {
		cfg.tzTargetHours = tzT;
		(void)ee.writeTzTargetHours(cfg.tzTargetHours);
	}
	if (parseI8(findParam(params, "tzNtp"), tzN, -12, 14)) {
		cfg.tzNtpHours = tzN;
		(void)ee.writeTzNtpHours(cfg.tzNtpHours);
	}

	// Sync sources enable
	syncData.gpsEnable = hasParam(params, "src_gps") ? 1 : 0;
	syncData.netEnable = hasParam(params, "src_net1") ? 1 : 0;
	syncData.net2Enable = hasParam(params, "src_net2") ? 1 : 0;
	syncData.gsmEnable = hasParam(params, "src_gsm") ? 1 : 0;

	// Sync sources params
	uint8_t u8;
	if (parseU8(findParam(params, "gps_per"), u8, 0, 5)) syncData.gpsPeriodIdx = u8;

	if (parseU8(findParam(params, "net1_ntp"), u8, 0, 4)) syncData.netProviderIdx = u8;
	if (parseU8(findParam(params, "net1_per"), u8, 0, 5)) syncData.netPeriodIdx = u8;

	if (parseU8(findParam(params, "net2_per"), u8, 0, 5)) syncData.net2PeriodIdx = u8;

	if (parseU8(findParam(params, "gsm_op"), u8, 0, 2)) syncData.gsmProviderIdx = u8;
	if (parseU8(findParam(params, "gsm_per"), u8, 0, 5)) syncData.gsmPeriodIdx = u8;

	(void)syncStore.save(syncData);

	// SIM settings
	const char* apn = findParam(params, "apn");
	const char* apn_user = findParam(params, "apn_user");
	const char* apn_pass = findParam(params, "apn_pass");
	const char* ntp_server = findParam(params, "ntp_server");

	if (apn) { strncpy(cfg.apn, apn, sizeof(cfg.apn) - 1); cfg.apn[sizeof(cfg.apn) - 1] = 0; (void)ee.writeAPN(cfg.apn); }
	if (apn_user) { strncpy(cfg.user, apn_user, sizeof(cfg.user) - 1); cfg.user[sizeof(cfg.user) - 1] = 0; (void)ee.writeUSER(cfg.user); }
	if (apn_pass) { strncpy(cfg.pass, apn_pass, sizeof(cfg.pass) - 1); cfg.pass[sizeof(cfg.pass) - 1] = 0; (void)ee.writePASS(cfg.pass); }
	if (ntp_server) { strncpy(cfg.server, ntp_server, sizeof(cfg.server) - 1); cfg.server[sizeof(cfg.server) - 1] = 0; (void)ee.writeSERVER(cfg.server); }

	bool b01;
	if (parseBool01(findParam(params, "enFall"), b01)) {
		cfg.enableFallback = b01;
		(void)ee.writeEnableFallback(cfg.enableFallback);
	}

	uint32_t pms;
	if (parseU32(findParam(params, "simPeriodMs"), pms, 1000UL, 86400000UL)) {
		cfg.periodMs = pms;
		(void)ee.writePeriodMs(cfg.periodMs);
	}

	// INTERNET1/2 store
	LanIfStore::IfConfig if1{}, if2{};
	lanStore.load(LanIfStore::IF1, if1, false);
	lanStore.load(LanIfStore::IF2, if2, false);

	bool b;
	uint8_t ip4[4];

	if (parseBool01(findParam(params, "if1_dhcp"), b)) if1.dhcp = b ? 1 : 0;
	if (parseU8(findParam(params, "if1_ntpIdx"), u8, 0, 4)) if1.ntpIdx = u8;
	if (parseU8(findParam(params, "if1_perIdx"), u8, 0, 5)) if1.periodIdx = u8;
	if (parseIp4(findParam(params, "if1_ip"), ip4)) memcpy(if1.ip, ip4, 4);
	if (parseIp4(findParam(params, "if1_mask"), ip4)) memcpy(if1.mask, ip4, 4);
	if (parseIp4(findParam(params, "if1_gw"), ip4)) memcpy(if1.gw, ip4, 4);
	if (parseIp4(findParam(params, "if1_dns"), ip4)) memcpy(if1.dns, ip4, 4);

	if (parseBool01(findParam(params, "if2_dhcp"), b)) if2.dhcp = b ? 1 : 0;
	if (parseU8(findParam(params, "if2_ntpIdx"), u8, 0, 4)) if2.ntpIdx = u8;
	if (parseU8(findParam(params, "if2_perIdx"), u8, 0, 5)) if2.periodIdx = u8;
	if (parseIp4(findParam(params, "if2_ip"), ip4)) memcpy(if2.ip, ip4, 4);
	if (parseIp4(findParam(params, "if2_mask"), ip4)) memcpy(if2.mask, ip4, 4);
	if (parseIp4(findParam(params, "if2_gw"), ip4)) memcpy(if2.gw, ip4, 4);
	if (parseIp4(findParam(params, "if2_dns"), ip4)) memcpy(if2.dns, ip4, 4);

	lanStore.save(LanIfStore::IF1, if1, true);
	lanStore.save(LanIfStore::IF2, if2, true);

	applyInternet1FromStore();
	applyInternet2FromStore();

	// WEB auth change
	const char* nu = findParam(params, "web_user");
	const char* np = findParam(params, "web_pass");
	if (nu && np && nu[0] && np[0])
	{
		AuthRec aa;
		memset(&aa, 0, sizeof(aa));
		strncpy(aa.user, nu, sizeof(aa.user) - 1);
		strncpy(aa.pass, np, sizeof(aa.pass) - 1);
		trimSpaces(aa.user);
		trimSpaces(aa.pass);
		if (aa.user[0] && aa.pass[0]) (void)authSave(aa);
	}

	planner.onSettingsChanged();
	sendRedirect(c, "/cfg");
}

void WebUI::actionGsmSave(EthernetClient& c, char* params)
{
	if (!params) { sendRedirect(c, "/gsm"); return; }

	if (hasParam(params, "doPreset")) {
		const char* p = findParam(params, "preset");
		bool ok = applyGsmPresetByKey(p);
		Serial.print(F("Web:GSM preset=")); Serial.print(p ? p : "(null)");
		Serial.print(F(" -> ")); Serial.println(ok ? F("OK") : F("FAIL"));

		planner.onSettingsChanged();
		sendRedirect(c, "/gsm");
		return;
	}

	bool en;
	uint8_t u8;

	if (parseBool01(findParam(params, "gsm_en"), en)) syncData.gsmEnable = en ? 1 : 0;
	if (parseU8(findParam(params, "gsm_op"), u8, 0, 2)) syncData.gsmProviderIdx = u8;
	if (parseU8(findParam(params, "gsm_per"), u8, 0, 5)) syncData.gsmPeriodIdx = u8;

	(void)syncStore.save(syncData);

	const char* apn = findParam(params, "apn");
	const char* apn_user = findParam(params, "apn_user");
	const char* apn_pass = findParam(params, "apn_pass");
	const char* ntp_server = findParam(params, "ntp_server");

	if (apn) { strncpy(cfg.apn, apn, sizeof(cfg.apn) - 1); cfg.apn[sizeof(cfg.apn) - 1] = 0; (void)ee.writeAPN(cfg.apn); }
	if (apn_user) { strncpy(cfg.user, apn_user, sizeof(cfg.user) - 1); cfg.user[sizeof(cfg.user) - 1] = 0; (void)ee.writeUSER(cfg.user); }
	if (apn_pass) { strncpy(cfg.pass, apn_pass, sizeof(cfg.pass) - 1); cfg.pass[sizeof(cfg.pass) - 1] = 0; (void)ee.writePASS(cfg.pass); }
	if (ntp_server) { strncpy(cfg.server, ntp_server, sizeof(cfg.server) - 1); cfg.server[sizeof(cfg.server) - 1] = 0; (void)ee.writeSERVER(cfg.server); }

	planner.onSettingsChanged();
	sendRedirect(c, "/gsm");
}

