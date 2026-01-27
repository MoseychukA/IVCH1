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
	while (*s == ' ' || *s == '\t') memmove(s, s + 1, strlen(s));
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
// ХРАНИМ как раньше:user/pass для BasicAuth.
// UI просто называем "логин/пароль".
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

	// commit-порядок:data -> ver -> magic
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

	// Headers
	char authB64[140];
	size_t contentLen = 0;
	if (!readHeaders(c, authB64, sizeof(authB64), contentLen, 1500)) { c.stop(); return; }

	// Auth
	if (!checkAuth(authB64))
	{
		sendUnauthorized(c);
		c.stop();
		return;
	}

	// Query from URL (GET) or Body (POST)
	char* q = strchr(path, '?');
	char* params = nullptr;

	static char bodyBuf[700];
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

	else if (isGet && strcmp(path, "/api/status") == 0) apiStatus(c);
	else if (isGet && strcmp(path, "/api/config") == 0) apiConfig(c);

	else if (isPost && strcmp(path, "/cfg/save") == 0) actionCfgSave(c, params);
	else if (isPost && strcmp(path, "/gsm/save") == 0) actionGsmSave(c, params);

	else pageNotFound(c);

	c.stop();
}

// ================== HTML header/footer ==================
static void htmlHeader(EthernetClient& c, const __FlashStringHelper* title)
{
	c.print(F("<!doctype html><html><head><meta charset='utf-8'>"));
	c.print(F("<title>")); c.print(title); c.print(F("</title>"));

	c.print(F("<style>"
		"body{font-family:Arial,Helvetica,sans-serif;font-size:12px;line-height:1.25;margin:10px;}"
		"h2{font-size:16px;margin:8px 0;}"
		"h3{font-size:14px;margin:10px 0 6px;}"
		"code{background:#eee;padding:1px 3px;}"
		"input,select,button{font-size:12px;padding:2px 4px;}"
		".pw{font-size:18px;padding:4px 6px;}"
		".nav a{margin-right:10px;}"
		".box{border:1px solid #ddd;padding:8px;margin:8px 0;}"
		"</style>"));

	c.print(F("</head><body>"));
	c.print(F("<div class='nav'>"
		"<a href='/'>СТАТУС</a>"
		"<a href='/cfg'>НАСТРОЙКИ</a>"
		"<a href='/gsm'>GSM</a>"
		"</div><hr>"));
}

static void htmlFooter(EthernetClient& c)
{
	c.print(F("</body></html>"));
}

// ================== Pages ==================
void WebUI::pageStatus(EthernetClient& c)
{
	sendHeader(c, 200, F("text/html; charset=utf-8"));
	htmlHeader(c, F("СТАТУС"));

	// ИНТЕРНЕТ1
	c.print(F("<div class='box'><h3>ИНТЕРНЕТ 1</h3>"));
	c.print(F("IP:<code>")); c.print(ntpLan.localIP()); c.print(F("</code><br>"));
	c.print(F("Линк:<code>")); c.print(Ethernet.linkReport()); c.print(F("</code>"));
	c.print(F("</div>"));

	// ИНТЕРНЕТ2
	Internet2Client::Status st{};
	bool ok2 = internet2.readStatus(st);

	c.print(F("<div class='box'><h3>ИНТЕРНЕТ 2</h3>"));
	c.print(F("Статус чтения:<code>")); c.print(ok2 ? "OK" : "ОШИБКА"); c.print(F("</code><br>"));
	c.print(F("IP:<code>")); c.print(ok2 ? st.ip : IPAddress(0, 0, 0, 0)); c.print(F("</code><br>"));
	c.print(F("Последняя синхронизация:<code>")); c.print((ok2 && st.lastSyncOk) ? "УСПЕХ" : "НЕТ"); c.print(F("</code>"));
	c.print(F("</div>"));

	// Источники (теперь может быть несколько)
	c.print(F("<div class='box'><h3>СИНХРОНИЗАЦИЯ</h3>"));
	c.print(F("<p>Приоритет:<code>GPS → INTERNET1 → INTERNET2 → GSM</code></p>"));
	c.print(F("<p>Включено:<code>"));
	bool any = false;
	if (syncData.gpsEnable) { c.print("GPS "); any = true; }
	if (syncData.netEnable) { c.print("INTERNET1 "); any = true; }
	if (syncData.net2Enable) { c.print("INTERNET2 "); any = true; }
	if (syncData.gsmEnable) { c.print("GSM "); any = true; }
	if (!any) c.print("НИЧЕГО");
	c.print(F("</code></p>"));
	c.print(F("<p>Часовой пояс:<code>UTC"));
	c.print((int)cfg.tzTargetHours >= 0 ? "+" : "");
	c.print((int)cfg.tzTargetHours);
	c.print(F("</code></p>"));
	c.print(F("</div>"));

	// GSM
	c.print(F("<div class='box'><h3>GSM</h3>"));
	c.print(F("Регистрация в сети:<code>")); c.print(gNetRegistered ? "ДА" : "НЕТ"); c.print(F("</code><br>"));
	c.print(F("RSSI:<code>")); c.print((int)gRssiDbm); c.print(F(" dBm</code><br>"));
	c.print(F("Уровень:<code>")); c.print((int)gSignalBars); c.print(F("/5</code>"));
	c.print(F("</div>"));

	// GPS
	c.print(F("<div class='box'><h3>GPS</h3>"));
	c.print(F("Fix:<code>")); c.print(gGpsFix ? "ДА" : "НЕТ"); c.print(F("</code><br>"));
	c.print(F("Спутники (исп/вид):<code>"));
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
	// STRICT load + defaults если не инициализировано
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

	c.print(F("<div class='box'><h3>ТЕКУЩИЕ ПАРАМЕТРЫ (реальные)</h3>"));
	c.print(F("ИНТЕРНЕТ 1 IP:<code>")); c.print(ntpLan.localIP()); c.print(F("</code> / Линк:<code>"));
	c.print(Ethernet.linkReport()); c.print(F("</code><br>"));
	c.print(F("ИНТЕРНЕТ 2 IP:<code>")); c.print(ok2 ? st.ip : IPAddress(0, 0, 0, 0)); c.print(F("</code>"));
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

	// SOURCES:теперь можно несколько
	c.print(F("<div class='box'><h3>Источники синхронизации (можно несколько)</h3>"));
	c.print(F("<p><small>Приоритет:GPS → INTERNET1 → INTERNET2 → GSM</small></p>"));
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
	cb("src_net1", "ИНТЕРНЕТ 1", syncData.netEnable != 0);
	cb("src_net2", "ИНТЕРНЕТ 2", syncData.net2Enable != 0);
	cb("src_gsm", "GSM", syncData.gsmEnable != 0);
	c.print(F("</p></div>"));

	// INTERNET1 store
	c.print(F("<div class='box'><h3>ИНТЕРНЕТ 1 (настройки из EEPROM)</h3>"));
	c.print(F("<p>DHCP:<select name='if1_dhcp'>"));
	c.print(F("<option value='1'")); if (if1.dhcp) c.print(F(" selected")); c.print(F(">ВКЛ</option>"));
	c.print(F("<option value='0'")); if (!if1.dhcp) c.print(F(" selected")); c.print(F(">ВЫКЛ</option>"));
	c.print(F("</select></p>"));

	c.print(F("<p>NTP-сервер:"));
	printSelectU8(c, "if1_ntpIdx", if1.ntpIdx % 5, 5, kNtpIpStr);
	c.print(F("</p>"));

	c.print(F("<p>Период синхронизации:"));
	printSelectU8(c, "if1_perIdx", if1.periodIdx % 6, 6, kPeriodsName);
	c.print(F("</p>"));

	c.print(F("<p>IP:")); printIpInput(c, "if1_ip", if1.ip); c.print(F("</p>"));
	c.print(F("<p>MASK:")); printIpInput(c, "if1_mask", if1.mask); c.print(F("</p>"));
	c.print(F("<p>GW:")); printIpInput(c, "if1_gw", if1.gw); c.print(F("</p>"));
	c.print(F("<p>DNS:")); printIpInput(c, "if1_dns", if1.dns); c.print(F("</p>"));
	c.print(F("</div>"));

	// INTERNET2 store
	c.print(F("<div class='box'><h3>ИНТЕРНЕТ 2 (настройки из EEPROM)</h3>"));
	c.print(F("<p>DHCP:<select name='if2_dhcp'>"));
	c.print(F("<option value='1'")); if (if2.dhcp) c.print(F(" selected")); c.print(F(">ВКЛ</option>"));
	c.print(F("<option value='0'")); if (!if2.dhcp) c.print(F(" selected")); c.print(F(">ВЫКЛ</option>"));
	c.print(F("</select></p>"));

	c.print(F("<p>NTP-сервер:"));
	printSelectU8(c, "if2_ntpIdx", if2.ntpIdx % 5, 5, kNtpIpStr);
	c.print(F("</p>"));

	c.print(F("<p>Период синхронизации:"));
	printSelectU8(c, "if2_perIdx", if2.periodIdx % 6, 6, kPeriodsName);
	c.print(F("</p>"));

	c.print(F("<p>IP:")); printIpInput(c, "if2_ip", if2.ip); c.print(F("</p>"));
	c.print(F("<p>MASK:")); printIpInput(c, "if2_mask", if2.mask); c.print(F("</p>"));
	c.print(F("<p>GW:")); printIpInput(c, "if2_gw", if2.gw); c.print(F("</p>"));
	c.print(F("<p>DNS:")); printIpInput(c, "if2_dns", if2.dns); c.print(F("</p>"));
	c.print(F("</div>"));

	// WEB auth (только переименование в UI)
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

	static const char* kGsmProviders[5] = { "МТС","МЕГАФОН","БИЛАЙН","YOTA","ТЕЛЕ 2" };
	static const char* kPeriodsRu[6] = { "1 мин","10 мин","30 мин","1 час","6 часов","12 часов" };

	c.print(F("<div class='box'><h3>Статус GSM</h3>"));
	c.print(F("Регистрация в сети:<code>")); c.print(gNetRegistered ? "ДА" : "НЕТ"); c.print(F("</code><br>"));
	c.print(F("RSSI:<code>")); c.print((int)gRssiDbm); c.print(F(" dBm</code><br>"));
	c.print(F("Уровень:<code>")); c.print((int)gSignalBars); c.print(F("/5</code>"));
	c.print(F("</div>"));

	c.print(F("<div class='box'><h3>Параметры GSM</h3>"));
	c.print(F("<form method='post' action='/gsm/save'>"));

	c.print(F("<p>Включить GSM:<select name='gsm_en'>"));
	c.print(F("<option value='1'")); if (syncData.gsmEnable) c.print(F(" selected")); c.print(F(">ВКЛ</option>"));
	c.print(F("<option value='0'")); if (!syncData.gsmEnable) c.print(F(" selected")); c.print(F(">ВЫКЛ</option>"));
	c.print(F("</select></p>"));

	c.print(F("<p>Оператор:<select name='gsm_op'>"));
	for (uint8_t i = 0; i < 5; i++) {
		c.print(F("<option value='")); c.print(i); c.print(F("'"));
		if ((syncData.gsmProviderIdx % 5) == i) c.print(F(" selected"));
		c.print(F(">"));
		htmlEscPrint(c, kGsmProviders[i]);
		c.print(F("</option>"));
	}
	c.print(F("</select></p>"));

	c.print(F("<p>Период синхронизации:<select name='gsm_per'>"));
	for (uint8_t i = 0; i < 6; i++) {
		c.print(F("<option value='")); c.print(i); c.print(F("'"));
		if ((syncData.gsmPeriodIdx % 6) == i) c.print(F(" selected"));
		c.print(F(">"));
		htmlEscPrint(c, kPeriodsRu[i]);
		c.print(F("</option>"));
	}
	c.print(F("</select></p>"));

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
	LanIfStore::IfConfig if1{}, if2{};
	bool okIf1 = lanStore.load(LanIfStore::IF1, if1, true);
	if (!okIf1) LanIfStore::defaults(LanIfStore::IF1, if1);
	bool okIf2 = lanStore.load(LanIfStore::IF2, if2, true);
	if (!okIf2) LanIfStore::defaults(LanIfStore::IF2, if2);

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
		ee.writeTzTargetHours(cfg.tzTargetHours);
	}
	if (parseI8(findParam(params, "tzNtp"), tzN, -12, 14)) {
		cfg.tzNtpHours = tzN;
		ee.writeTzNtpHours(cfg.tzNtpHours);
	}

	// Sources:теперь можно несколько (checkbox => param present)
	syncData.gpsEnable = hasParam(params, "src_gps") ? 1 : 0;
	syncData.netEnable = hasParam(params, "src_net1") ? 1 : 0;
	syncData.net2Enable = hasParam(params, "src_net2") ? 1 : 0;
	syncData.gsmEnable = hasParam(params, "src_gsm") ? 1 : 0;

	bool okS = syncStore.save(syncData);
	Serial.print(F("Web:syncStore.save=")); Serial.println(okS ? F("OK") : F("FAIL"));

	SyncSourcesStore::Data rd;
	bool okL = syncStore.load(rd);
	Serial.print(F("Web:syncStore.load=")); Serial.println(okL ? F("OK") : F("FAIL"));
	Serial.print(F("Web:rd gps=")); Serial.print(rd.gpsEnable);
	Serial.print(F(" net1=")); Serial.print(rd.netEnable);
	Serial.print(F(" net2=")); Serial.print(rd.net2Enable);
	Serial.print(F(" gsm=")); Serial.println(rd.gsmEnable);

	// INTERNET1/2 store
	LanIfStore::IfConfig if1{}, if2{};
	lanStore.load(LanIfStore::IF1, if1, false);
	lanStore.load(LanIfStore::IF2, if2, false);

	bool b;
	uint8_t u8;
	uint8_t ip[4];

	if (parseBool01(findParam(params, "if1_dhcp"), b)) if1.dhcp = b ? 1 : 0;
	if (parseU8(findParam(params, "if1_ntpIdx"), u8, 0, 4)) if1.ntpIdx = u8;
	if (parseU8(findParam(params, "if1_perIdx"), u8, 0, 5)) if1.periodIdx = u8;
	if (parseIp4(findParam(params, "if1_ip"), ip)) memcpy(if1.ip, ip, 4);
	if (parseIp4(findParam(params, "if1_mask"), ip)) memcpy(if1.mask, ip, 4);
	if (parseIp4(findParam(params, "if1_gw"), ip)) memcpy(if1.gw, ip, 4);
	if (parseIp4(findParam(params, "if1_dns"), ip)) memcpy(if1.dns, ip, 4);

	if (parseBool01(findParam(params, "if2_dhcp"), b)) if2.dhcp = b ? 1 : 0;
	if (parseU8(findParam(params, "if2_ntpIdx"), u8, 0, 4)) if2.ntpIdx = u8;
	if (parseU8(findParam(params, "if2_perIdx"), u8, 0, 5)) if2.periodIdx = u8;
	if (parseIp4(findParam(params, "if2_ip"), ip)) memcpy(if2.ip, ip, 4);
	if (parseIp4(findParam(params, "if2_mask"), ip)) memcpy(if2.mask, ip, 4);
	if (parseIp4(findParam(params, "if2_gw"), ip)) memcpy(if2.gw, ip, 4);
	if (parseIp4(findParam(params, "if2_dns"), ip)) memcpy(if2.dns, ip, 4);

	lanStore.save(LanIfStore::IF1, if1, true);
	lanStore.save(LanIfStore::IF2, if2, true);

	applyInternet1FromStore();
	applyInternet2FromStore();

	// WEB login/pass change (only if provided)
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

	bool en;
	uint8_t u8;

	if (parseBool01(findParam(params, "gsm_en"), en)) syncData.gsmEnable = en ? 1 : 0;
	if (parseU8(findParam(params, "gsm_op"), u8, 0, 4)) syncData.gsmProviderIdx = u8;
	if (parseU8(findParam(params, "gsm_per"), u8, 0, 5)) syncData.gsmPeriodIdx = u8;

	bool okS = syncStore.save(syncData);
	Serial.print(F("Web:gsm syncStore.save=")); Serial.println(okS ? F("OK") : F("FAIL"));

	planner.onSettingsChanged();
	sendRedirect(c, "/gsm");
}