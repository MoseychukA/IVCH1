#include "WebUI.h"

#include <IPAddress.h>
#include <Wire.h>

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

// GPS globals (если в проекте есть; если нет — закомментируйте extern и блоки в JSON)
extern uint8_t gGpsSatsUsed;
extern uint8_t gGpsSatsView;
extern bool gGpsFix;

// --- списки как в menu.cpp ---
static const char* kNtpIpStr[5] = {
	"162.159.200.123",
	"162.159.200.1",
	"129.6.15.28",
	"132.163.96.1",
	"216.239.35.0"
};

static const char* kPeriodsName[6] = { "1 мин","10 мин","30 мин","1 час","6 часов","12 часов" };

// ================== ctor ==================
WebUI::WebUI(uint16_t port) :_srv(port) {}

// ================== small helpers ==================
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
	for (; *s; s++) {
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
	// trim left
	while (*s == ' ' || *s == '\t') memmove(s, s + 1, strlen(s));
	// trim right
	size_t n = strlen(s);
	while (n && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r' || s[n - 1] == '\n')) {
		s[n - 1] = 0;
		n--;
	}
}

// ================== Base64 ==================
bool WebUI::base64Encode(char* out, size_t outN, const uint8_t* in, size_t inN)
{
	static const char* T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	// worst-case len = 4*ceil(n/3) + 1
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
		else { // rem == 1
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
static constexpr uint16_t AUTH_MAGIC = 0x4157; // 'W''A' (little-endian)
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

	uint8_t mg[2] = { (uint8_t)(AUTH_MAGIC & 0xFF),(uint8_t)(AUTH_MAGIC >> 8) };
	if (!eepromWrite(AUTH_MAGIC_ADDR, mg, 2)) return false;

	uint8_t ver = AUTH_VER;
	if (!eepromWrite(AUTH_VER_ADDR, &ver, 1)) return false;

	uint8_t raw[64];
	memset(raw, 0, sizeof(raw));
	strncpy((char*)raw, w.user, 31);
	strncpy((char*)raw + 32, w.pass, 31);

	return eepromWrite(AUTH_DATA_ADDR, raw, sizeof(raw));
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

	// expected token = base64("user:pass")
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
	c.print(F("Unauthorized"));
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
			if (ch == '\n') {
				out[n] = 0;
				return true;
			}
			if (n + 1 < outN) out[n++] = ch;
		}
	}
	out[0] = 0;
	return false;
}

bool WebUI::readHeaders(EthernetClient& c, char* authB64, size_t authB64N, uint32_t timeoutMs)
{
	// authB64:только base64 часть без "Basic "
	if (authB64 && authB64N) authB64[0] = 0;

	char line[192];
	while (true)
	{
		if (!readLine(c, line, sizeof(line), timeoutMs)) return false;
		if (line[0] == 0) return true; // пустая строка = конец заголовков

		if (startsWithI(line, "Authorization:"))
		{
			// ожидаем:Authorization:Basic xxxx
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
	}
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

const char* WebUI::findParam(char* query, const char* key)
{
	if (!query || !key) return nullptr;

	size_t klen = strlen(key);
	char* p = query;

	while (p && *p) {
		char* amp = strchr(p, '&');
		if (amp) *amp = 0;

		char* eq = strchr(p, '=');
		if (eq) {
			*eq = 0;
			if (strlen(p) == klen && strcmp(p, key) == 0) {
				char* val = eq + 1;
				if (amp) *amp = '&';
				*eq = '=';
				return val;
			}
			*eq = '=';
		}

		if (amp) {
			*amp = '&';
			p = amp + 1;
		}
		else break;
	}
	return nullptr;
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
	// если auth записи нет — создадим дефолт admin/admin (один раз)
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

// ================== routing ==================
void WebUI::handleClient(EthernetClient& c)
{
	// Request-Line
	char req[192];
	if (!readLine(c, req, sizeof(req), 1500)) { c.stop(); return; }

	bool isGet = startsWith(req, "GET ");
	bool isPost = startsWith(req, "POST ");
	if (!isGet && !isPost) { sendHeader(c, 404, F("text/plain; charset=utf-8")); c.print(F("Bad method")); c.stop(); return; }

	// Path
	const char* p = isGet ? (req + 4) : (req + 5);
	char path[160];
	size_t i = 0;
	while (*p && *p != ' ' && i + 1 < sizeof(path)) path[i++] = *p++;
	path[i] = 0;

	// Headers (need auth)
	char authB64[140];
	if (!readHeaders(c, authB64, sizeof(authB64), 1500)) { c.stop(); return; }

	if (!checkAuth(authB64))
	{
		sendUnauthorized(c);
		c.stop();
		return;
	}

	// Query
	char* q = strchr(path, '?');
	char* query = nullptr;
	if (q) { *q = 0; query = q + 1; urlDecodeInPlace(query); }

	// Routes (3 pages + JSON + actions)
	if (isGet && strcmp(path, "/") == 0) pageStatus(c);
	else if (isGet && strcmp(path, "/cfg") == 0) pageCfg(c);
	else if (isGet && strcmp(path, "/gsm") == 0) pageGsm(c);

	else if (isGet && strcmp(path, "/api/status") == 0) apiStatus(c);
	else if (isGet && strcmp(path, "/api/config") == 0) apiConfig(c);

	else if (isPost && strcmp(path, "/cfg/save") == 0) actionCfgSave(c, query);
	else if (isPost && strcmp(path, "/gsm/save") == 0) actionGsmSave(c, query);

	// legacy redirects (если у вас были старые ссылки)
	else if (isGet && strcmp(path, "/internet1") == 0) sendRedirect(c, "/cfg");
	else if (isGet && strcmp(path, "/internet2") == 0) sendRedirect(c, "/cfg");

	else pageNotFound(c);

	c.stop();
}

// ================== Pages ==================
static void htmlHeader(EthernetClient& c, const __FlashStringHelper* title)
{
	c.print(F("<!doctype html><html><head><meta charset='utf-8'>"));
	c.print(F("<title>"));
	c.print(title);
	c.print(F("</title></head><body>"));
	c.print(F("<div><a href='/'>STATUS</a> | <a href='/cfg'>CONFIG</a> | <a href='/gsm'>GSM</a></div><hr>"));
}

static void htmlFooter(EthernetClient& c)
{
	c.print(F("</body></html>"));
}

void WebUI::pageStatus(EthernetClient& c)
{
	sendHeader(c, 200, F("text/html; charset=utf-8"));
	htmlHeader(c, F("STATUS"));

	// INTERNET1
	c.print(F("<h3>INTERNET1 (W5500)</h3><ul>"));
	c.print(F("<li>IP:<code>")); c.print(ntpLan.localIP()); c.print(F("</code></li>"));
	c.print(F("<li>Link:<code>")); c.print(Ethernet.linkReport()); c.print(F("</code></li>"));
	c.print(F("</ul>"));

	// INTERNET2
	c.print(F("<h3>INTERNET2 (I2C)</h3><ul>"));
	Internet2Client::Status st{};
	bool ok2 = internet2.readStatus(st);
	c.print(F("<li>Status read:<code>")); c.print(ok2 ? "OK" : "FAIL"); c.print(F("</code></li>"));
	c.print(F("<li>IP:<code>")); c.print(ok2 ? st.ip : IPAddress(0, 0, 0, 0)); c.print(F("</code></li>"));
	c.print(F("<li>Last sync ok:<code>")); c.print(ok2 && st.lastSyncOk ? "YES" : "NO"); c.print(F("</code></li>"));
	c.print(F("</ul>"));

	// TZ + source
	c.print(F("<h3>TIME</h3><ul>"));
	c.print(F("<li>TZ target:<code>")); c.print((int)cfg.tzTargetHours); c.print(F("</code></li>"));
	c.print(F("<li>TZ NTP:<code>")); c.print((int)cfg.tzNtpHours); c.print(F("</code></li>"));

	const char* src = "NONE";
	if (syncData.gpsEnable) src = "GPS";
	else if (syncData.netEnable) src = "NET";
	else if (syncData.net2Enable) src = "NET2";
	else if (syncData.gsmEnable) src = "GSM";
	c.print(F("<li>Source:<code>")); c.print(src); c.print(F("</code></li>"));
	c.print(F("</ul>"));

	// GSM
	c.print(F("<h3>GSM</h3><ul>"));
	c.print(F("<li>Registered:<code>")); c.print(gNetRegistered ? "YES" : "NO"); c.print(F("</code></li>"));
	c.print(F("<li>RSSI dBm:<code>")); c.print((int)gRssiDbm); c.print(F("</code></li>"));
	c.print(F("<li>Bars:<code>")); c.print((int)gSignalBars); c.print(F("</code></li>"));
	c.print(F("</ul>"));

	// GPS
	c.print(F("<h3>GPS</h3><ul>"));
	c.print(F("<li>Fix:<code>")); c.print(gGpsFix ? "YES" : "NO"); c.print(F("</code></li>"));
	c.print(F("<li>Sat used/view:<code>")); c.print((int)gGpsSatsUsed); c.print('/'); c.print((int)gGpsSatsView); c.print(F("</code></li>"));
	c.print(F("</ul>"));

	htmlFooter(c);
}

static void printIpInput(EthernetClient& c, const char* name, const uint8_t ip[4])
{
	c.print(F("<input name='"));
	c.print(name);
	c.print(F("' value='"));
	c.print(ip[0]); c.print('.'); c.print(ip[1]); c.print('.'); c.print(ip[2]); c.print('.'); c.print(ip[3]);
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
	lanStore.load(LanIfStore::IF1, if1, false);
	lanStore.load(LanIfStore::IF2, if2, false);

	AuthRec a;
	bool existed = false;
	authLoadOrDefaults(a, existed);

	sendHeader(c, 200, F("text/html; charset=utf-8"));
	htmlHeader(c, F("CONFIG"));

	c.print(F("<h3>TZ</h3>"));
	c.print(F("<form method='post' action='/cfg/save?'>"));

	c.print(F("<p>TZ target (-12..14):<input name='tzTarget' value='"));
	c.print((int)cfg.tzTargetHours);
	c.print(F("'></p>"));

	c.print(F("<p>TZ NTP (-12..14):<input name='tzNtp' value='"));
	c.print((int)cfg.tzNtpHours);
	c.print(F("'></p>"));

	c.print(F("<h3>Sync source (select one)</h3>"));
	c.print(F("<p>"));
	auto radio = [&](const char* val, const char* label, bool checked) {
		c.print(F("<label><input type='radio' name='src' value='"));
		c.print(val);
		c.print(F("'"));
		if (checked) c.print(F(" checked"));
		c.print(F("> "));
		c.print(label);
		c.print(F("</label> "));
	};
	const char* cur = "none";
	if (syncData.gpsEnable) cur = "gps";
	else if (syncData.netEnable) cur = "net";
	else if (syncData.net2Enable) cur = "net2";
	else if (syncData.gsmEnable) cur = "gsm";

	radio("gps", "GPS", strcmp(cur, "gps") == 0);
	radio("net", "NET (INTERNET1)", strcmp(cur, "net") == 0);
	radio("net2", "NET2 (INTERNET2)", strcmp(cur, "net2") == 0);
	radio("gsm", "GSM", strcmp(cur, "gsm") == 0);
	c.print(F("</p>"));

	c.print(F("<h3>INTERNET1 (store)</h3>"));
	c.print(F("<p>DHCP:<select name='if1_dhcp'>"));
	c.print(F("<option value='1'")); if (if1.dhcp) c.print(F(" selected")); c.print(F(">ON</option>"));
	c.print(F("<option value='0'")); if (!if1.dhcp) c.print(F(" selected")); c.print(F(">OFF</option>"));
	c.print(F("</select></p>"));

	c.print(F("<p>NTP upstream:"));
	printSelectU8(c, "if1_ntpIdx", if1.ntpIdx % 5, 5, kNtpIpStr);
	c.print(F("</p>"));

	c.print(F("<p>Period:"));
	printSelectU8(c, "if1_perIdx", if1.periodIdx % 6, 6, kPeriodsName);
	c.print(F("</p>"));

	c.print(F("<p>IP:")); printIpInput(c, "if1_ip", if1.ip); c.print(F("</p>"));
	c.print(F("<p>MASK:")); printIpInput(c, "if1_mask", if1.mask); c.print(F("</p>"));
	c.print(F("<p>GW:")); printIpInput(c, "if1_gw", if1.gw); c.print(F("</p>"));
	c.print(F("<p>DNS:")); printIpInput(c, "if1_dns", if1.dns); c.print(F("</p>"));

	c.print(F("<h3>INTERNET2 (store)</h3>"));
	c.print(F("<p>DHCP:<select name='if2_dhcp'>"));
	c.print(F("<option value='1'")); if (if2.dhcp) c.print(F(" selected")); c.print(F(">ON</option>"));
	c.print(F("<option value='0'")); if (!if2.dhcp) c.print(F(" selected")); c.print(F(">OFF</option>"));
	c.print(F("</select></p>"));

	c.print(F("<p>NTP upstream:"));
	printSelectU8(c, "if2_ntpIdx", if2.ntpIdx % 5, 5, kNtpIpStr);
	c.print(F("</p>"));

	c.print(F("<p>Period:"));
	printSelectU8(c, "if2_perIdx", if2.periodIdx % 6, 6, kPeriodsName);
	c.print(F("</p>"));

	c.print(F("<p>IP:")); printIpInput(c, "if2_ip", if2.ip); c.print(F("</p>"));
	c.print(F("<p>MASK:")); printIpInput(c, "if2_mask", if2.mask); c.print(F("</p>"));
	c.print(F("<p>GW:")); printIpInput(c, "if2_gw", if2.gw); c.print(F("</p>"));
	c.print(F("<p>DNS:")); printIpInput(c, "if2_dns", if2.dns); c.print(F("</p>"));

	c.print(F("<h3>WEB BasicAuth (EEPROM)</h3>"));
	c.print(F("<p>Current user:<code>"));
	c.print(a.user);
	c.print(F("</code></p>"));
	c.print(F("<p>New user:<input name='web_user' value=''></p>"));
	c.print(F("<p>New pass:<input name='web_pass' value=''></p>"));
	c.print(F("<p><small>Если поля пустые — пароль не меняется.</small></p>"));

	c.print(F("<p><button type='submit'>SAVE + APPLY</button></p>"));
	c.print(F("</form>"));

	htmlFooter(c);
}

void WebUI::pageGsm(EthernetClient& c)
{
	sendHeader(c, 200, F("text/html; charset=utf-8"));
	htmlHeader(c, F("GSM"));

	c.print(F("<form method='post' action='/gsm/save?'>"));

	c.print(F("<p>Enable GSM:<select name='gsm_en'>"));
	c.print(F("<option value='1'")); if (syncData.gsmEnable) c.print(F(" selected")); c.print(F(">ON</option>"));
	c.print(F("<option value='0'")); if (!syncData.gsmEnable) c.print(F(" selected")); c.print(F(">OFF</option>"));
	c.print(F("</select></p>"));

	c.print(F("<p>Provider idx (0..4):<input name='gsm_op' value='"));
	c.print((int)syncData.gsmProviderIdx);
	c.print(F("'></p>"));

	c.print(F("<p>Period idx (0..5):<input name='gsm_per' value='"));
	c.print((int)syncData.gsmPeriodIdx);
	c.print(F("'></p>"));

	c.print(F("<h3>Provider params (EEPROM cfg)</h3>"));
	c.print(F("<p>APN:<input name='apn' value='")); htmlEscPrint(c, cfg.apn); c.print(F("'></p>"));
	c.print(F("<p>USER:<input name='user' value='")); htmlEscPrint(c, cfg.user); c.print(F("'></p>"));
	c.print(F("<p>PASS:<input name='pass' value='")); htmlEscPrint(c, cfg.pass); c.print(F("'></p>"));
	c.print(F("<p>SERVER:<input name='server' value='")); htmlEscPrint(c, cfg.server); c.print(F("'></p>"));

	c.print(F("<p><button type='submit'>SAVE</button></p>"));
	c.print(F("</form>"));

	c.print(F("<hr><p>GSM status:Registered=<code>"));
	c.print(gNetRegistered ? "YES" : "NO");
	c.print(F("</code> RSSI=<code>"));
	c.print((int)gRssiDbm);
	c.print(F("</code> Bars=<code>"));
	c.print((int)gSignalBars);
	c.print(F("</code></p>"));

	htmlFooter(c);
}

void WebUI::pageNotFound(EthernetClient& c)
{
	sendHeader(c, 404, F("text/plain; charset=utf-8"));
	c.print(F("404 Not Found"));
}

// ================== JSON ==================
static void jsonStr(EthernetClient& c, const char* s)
{
	c.print('"');
	for (; *s; s++) {
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

	const char* src = "NONE";
	if (syncData.gpsEnable) src = "GPS";
	else if (syncData.netEnable) src = "NET";
	else if (syncData.net2Enable) src = "NET2";
	else if (syncData.gsmEnable) src = "GSM";

	c.print(F("{"));

	c.print(F("\"ip1\":")); jsonStr(c, ntpLan.localIP().toString().c_str()); c.print(F(","));
	c.print(F("\"link1\":")); jsonStr(c, Ethernet.linkReport()); c.print(F(","));

	c.print(F("\"internet2_ok\":")); c.print(ok2 ? F("true") : F("false")); c.print(F(","));
	c.print(F("\"ip2\":")); jsonStr(c, (ok2 ? st.ip : IPAddress(0, 0, 0, 0)).toString().c_str()); c.print(F(","));
	c.print(F("\"last_sync_ok\":")); c.print((ok2 && st.lastSyncOk) ? F("true") : F("false")); c.print(F(","));

	c.print(F("\"tz_target\":")); c.print((int)cfg.tzTargetHours); c.print(F(","));
	c.print(F("\"tz_ntp\":")); c.print((int)cfg.tzNtpHours); c.print(F(","));
	c.print(F("\"source\":")); jsonStr(c, src); c.print(F(","));

	c.print(F("\"gsm\":{"));
	c.print(F("\"registered\":")); c.print(gNetRegistered ? F("true") : F("false")); c.print(F(","));
	c.print(F("\"rssi_dbm\":")); c.print((int)gRssiDbm); c.print(F(","));
	c.print(F("\"bars\":")); c.print((int)gSignalBars);
	c.print(F("},"));

	c.print(F("\"gps\":{"));
	c.print(F("\"fix\":")); c.print(gGpsFix ? F("true") : F("false")); c.print(F(","));
	c.print(F("\"sats_used\":")); c.print((int)gGpsSatsUsed); c.print(F(","));
	c.print(F("\"sats_view\":")); c.print((int)gGpsSatsView);
	c.print(F("}"));

	c.print(F("}"));
}

void WebUI::apiConfig(EthernetClient& c)
{
	LanIfStore::IfConfig if1{}, if2{};
	lanStore.load(LanIfStore::IF1, if1, false);
	lanStore.load(LanIfStore::IF2, if2, false);

	sendHeader(c, 200, F("application/json; charset=utf-8"));

	c.print(F("{"));
	c.print(F("\"tz_target\":")); c.print((int)cfg.tzTargetHours); c.print(F(","));
	c.print(F("\"tz_ntp\":")); c.print((int)cfg.tzNtpHours); c.print(F(","));

	c.print(F("\"sources\":{"));
	c.print(F("\"gpsEnable\":")); c.print(syncData.gpsEnable ? F("true") : F("false")); c.print(F(","));
	c.print(F("\"netEnable\":")); c.print(syncData.netEnable ? F("true") : F("false")); c.print(F(","));
	c.print(F("\"net2Enable\":")); c.print(syncData.net2Enable ? F("true") : F("false")); c.print(F(","));
	c.print(F("\"gsmEnable\":")); c.print(syncData.gsmEnable ? F("true") : F("false")); c.print(F("}"));

	c.print(F(",\"internet1\":{"));
	c.print(F("\"dhcp\":")); c.print(if1.dhcp ? F("true") : F("false")); c.print(F(","));
	c.print(F("\"ip\":\"")); printIpOct(c, if1.ip); c.print(F("\","));
	c.print(F("\"mask\":\"")); printIpOct(c, if1.mask); c.print(F("\","));
	c.print(F("\"gw\":\"")); printIpOct(c, if1.gw); c.print(F("\","));
	c.print(F("\"dns\":\"")); printIpOct(c, if1.dns); c.print(F("\","));
	c.print(F("\"ntpIdx\":")); c.print((int)if1.ntpIdx); c.print(F(","));
	c.print(F("\"periodIdx\":")); c.print((int)if1.periodIdx);
	c.print(F("}"));

	c.print(F(",\"internet2\":{"));
	c.print(F("\"dhcp\":")); c.print(if2.dhcp ? F("true") : F("false")); c.print(F(","));
	c.print(F("\"ip\":\"")); printIpOct(c, if2.ip); c.print(F("\","));
	c.print(F("\"mask\":\"")); printIpOct(c, if2.mask); c.print(F("\","));
	c.print(F("\"gw\":\"")); printIpOct(c, if2.gw); c.print(F("\","));
	c.print(F("\"dns\":\"")); printIpOct(c, if2.dns); c.print(F("\","));
	c.print(F("\"ntpIdx\":")); c.print((int)if2.ntpIdx); c.print(F(","));
	c.print(F("\"periodIdx\":")); c.print((int)if2.periodIdx);
	c.print(F("}"));

	// GSM provider params (пароль лучше не отдавать; отдадим "***" если непустой)
	c.print(F(",\"gsm\":{"));
	c.print(F("\"apn\":")); jsonStr(c, cfg.apn); c.print(F(","));
	c.print(F("\"user\":")); jsonStr(c, cfg.user); c.print(F(","));
	if (cfg.pass[0]) jsonStr(c, "***"); else jsonStr(c, "");
	c.print(F(",\"server\":")); jsonStr(c, cfg.server);
	c.print(F("}"));

	c.print(F("}"));
}

// ================== Actions ==================
void WebUI::actionCfgSave(EthernetClient& c, char* query)
{
	if (!query) { sendRedirect(c, "/cfg"); return; }

	// --- TZ ---
	int8_t tzT, tzN;
	if (parseI8(findParam(query, "tzTarget"), tzT, -12, 14)) {
		cfg.tzTargetHours = tzT;
		ee.writeTzTargetHours(cfg.tzTargetHours);
	}
	if (parseI8(findParam(query, "tzNtp"), tzN, -12, 14)) {
		cfg.tzNtpHours = tzN;
		ee.writeTzNtpHours(cfg.tzNtpHours);
	}

	// --- Source select one ---
	const char* src = findParam(query, "src");
	if (src)
	{
		syncData.gpsEnable = 0;
		syncData.netEnable = 0;
		syncData.net2Enable = 0;
		syncData.gsmEnable = 0;

		if (strcmp(src, "gps") == 0) syncData.gpsEnable = 1;
		else if (strcmp(src, "net") == 0) syncData.netEnable = 1;
		else if (strcmp(src, "net2") == 0) syncData.net2Enable = 1;
		else if (strcmp(src, "gsm") == 0) syncData.gsmEnable = 1;

		syncStore.save(syncData);
	}

	// --- INTERNET1/2 config store ---
	LanIfStore::IfConfig if1{}, if2{};
	lanStore.load(LanIfStore::IF1, if1, false);
	lanStore.load(LanIfStore::IF2, if2, false);

	bool b;
	uint8_t u8;
	uint8_t ip[4];

	if (parseBool01(findParam(query, "if1_dhcp"), b)) if1.dhcp = b ? 1 : 0;
	if (parseU8(findParam(query, "if1_ntpIdx"), u8, 0, 4)) if1.ntpIdx = u8;
	if (parseU8(findParam(query, "if1_perIdx"), u8, 0, 5)) if1.periodIdx = u8;
	if (parseIp4(findParam(query, "if1_ip"), ip)) memcpy(if1.ip, ip, 4);
	if (parseIp4(findParam(query, "if1_mask"), ip)) memcpy(if1.mask, ip, 4);
	if (parseIp4(findParam(query, "if1_gw"), ip)) memcpy(if1.gw, ip, 4);
	if (parseIp4(findParam(query, "if1_dns"), ip)) memcpy(if1.dns, ip, 4);

	if (parseBool01(findParam(query, "if2_dhcp"), b)) if2.dhcp = b ? 1 : 0;
	if (parseU8(findParam(query, "if2_ntpIdx"), u8, 0, 4)) if2.ntpIdx = u8;
	if (parseU8(findParam(query, "if2_perIdx"), u8, 0, 5)) if2.periodIdx = u8;
	if (parseIp4(findParam(query, "if2_ip"), ip)) memcpy(if2.ip, ip, 4);
	if (parseIp4(findParam(query, "if2_mask"), ip)) memcpy(if2.mask, ip, 4);
	if (parseIp4(findParam(query, "if2_gw"), ip)) memcpy(if2.gw, ip, 4);
	if (parseIp4(findParam(query, "if2_dns"), ip)) memcpy(if2.dns, ip, 4);

	lanStore.save(LanIfStore::IF1, if1, true);
	lanStore.save(LanIfStore::IF2, if2, true);

	applyInternet1FromStore();
	applyInternet2FromStore();

	// --- WEB auth change (only if provided) ---
	const char* nu = findParam(query, "web_user");
	const char* np = findParam(query, "web_pass");
	if (nu && np && nu[0] && np[0])
	{
		AuthRec a;
		memset(&a, 0, sizeof(a));
		strncpy(a.user, nu, sizeof(a.user) - 1);
		strncpy(a.pass, np, sizeof(a.pass) - 1);
		trimSpaces(a.user);
		trimSpaces(a.pass);
		if (a.user[0] && a.pass[0]) (void)authSave(a);
	}

	planner.onSettingsChanged();
	sendRedirect(c, "/cfg");
}

void WebUI::actionGsmSave(EthernetClient& c, char* query)
{
	if (!query) { sendRedirect(c, "/gsm"); return; }

	bool en;
	uint8_t u8;

	if (parseBool01(findParam(query, "gsm_en"), en)) syncData.gsmEnable = en ? 1 : 0;
	if (parseU8(findParam(query, "gsm_op"), u8, 0, 4)) syncData.gsmProviderIdx = u8;
	if (parseU8(findParam(query, "gsm_per"), u8, 0, 5)) syncData.gsmPeriodIdx = u8;

	syncStore.save(syncData);

	// provider params (AT24C128Settings cfg)
	const char* apn = findParam(query, "apn");
	const char* user = findParam(query, "user");
	const char* pass = findParam(query, "pass");
	const char* server = findParam(query, "server");

	if (apn) { strncpy(cfg.apn, apn, sizeof(cfg.apn) - 1); cfg.apn[sizeof(cfg.apn) - 1] = 0; ee.writeAPN(cfg.apn); }
	if (user) { strncpy(cfg.user, user, sizeof(cfg.user) - 1); cfg.user[sizeof(cfg.user) - 1] = 0; ee.writeUSER(cfg.user); }
	if (pass) { strncpy(cfg.pass, pass, sizeof(cfg.pass) - 1); cfg.pass[sizeof(cfg.pass) - 1] = 0; ee.writePASS(cfg.pass); }
	if (server) { strncpy(cfg.server, server, sizeof(cfg.server) - 1); cfg.server[sizeof(cfg.server) - 1] = 0; ee.writeSERVER(cfg.server); }

	planner.onSettingsChanged();
	sendRedirect(c, "/gsm");
}