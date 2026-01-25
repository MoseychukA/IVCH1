#include "WebUI.h"

#include <IPAddress.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// ваши проектные заголовки
#include "LanIfStore.h"
#include "SyncSourcesStore.h"
#include "TimeSyncPlanner.h"
#include "NetFeed.h"
#include "Internet2Client.h"
#include "NtpLanService_Generic.h"

// --- externs из проекта ---
extern LanIfStore lanStore;
extern SyncSourcesStore syncStore;
extern SyncSourcesStore::Data syncData;
extern TimeSyncPlanner planner;

extern void applyInternet1FromStore();
extern void applyInternet2FromStore();

extern NtpLanService_Generic ntpLan; // INTERNET1
extern Internet2Client internet2; // INTERNET2 (I2C)

// GSM globals (по вашему menu.cpp)
extern bool gNetRegistered;
extern int16_t gRssiDbm;
extern uint8_t gSignalBars;

// NTP upstream list (как в menu.cpp)
static const char* kNtpIpStr[5] = {
	"162.159.200.123",
	"162.159.200.1",
	"129.6.15.28",
	"132.163.96.1",
	"216.239.35.0"
};

static const char* kPeriodsName[6] = { "1 мин","10 мин","30 мин","1 час","6 часов","12 часов" };

// ---------------- WebUI ctor ----------------

WebUI::WebUI(uint16_t port)
	:_srv(port)
{}

// ---------------- WebUI (public static) ----------------

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

// ---------------- WebUI ----------------

void WebUI::begin()
{
	_srv.begin();
}

void WebUI::tick()
{
	EthernetClient c = _srv.available();
	if (!c) return;
	handleClient(c);
}

// ---------------- HTTP core ----------------

bool WebUI::startsWith(const char* s, const char* pref)
{
	while (*pref) {
		if (*s++ != *pref++) return false;
	}
	return true;
}

bool WebUI::readRequestLine(EthernetClient& c, char* out, size_t outN, uint32_t timeoutMs)
{
	uint32_t t0 = millis();
	size_t n = 0;

	while ((millis() - t0) < timeoutMs) {
		while (c.available()) {
			char ch = (char)c.read();
			if (ch == '\r') continue;
			if (ch == '\n') {
				out[n] = 0;
				return (n > 0);
			}
			if (n + 1 < outN) out[n++] = ch;
		}
	}
	out[0] = 0;
	return false;
}

void WebUI::drainHeaders(EthernetClient& c, uint32_t timeoutMs)
{
	uint32_t t0 = millis();
	int crlfCount = 0;
	while ((millis() - t0) < timeoutMs) {
		while (c.available()) {
			char ch = (char)c.read();
			if (ch == '\n') {
				crlfCount++;
				if (crlfCount >= 2) return;
			}
			else if (ch != '\r') {
				crlfCount = 0;
			}
		}
	}
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

bool WebUI::parseIp4(const char* s, uint8_t out[4])
{
	if (!s) return false;
	int a, b, c, d;
	if (sscanf(s, "%d.%d.%d.%d", &a, &b, &c, &d) != 4) return false;
	if ((unsigned)a > 255 || (unsigned)b > 255 || (unsigned)c > 255 || (unsigned)d > 255) return false;
	out[0] = (uint8_t)a; out[1] = (uint8_t)b; out[2] = (uint8_t)c; out[3] = (uint8_t)d;
	return true;
}

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
	c.print(F("Location:"));
	c.print(location);
	c.print(F("\r\n\r\n"));
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

void WebUI::handleClient(EthernetClient& c)
{
	char line[192];
	if (!readRequestLine(c, line, sizeof(line), 1500)) { c.stop(); return; }

	drainHeaders(c, 1500);

	bool isGet = startsWith(line, "GET ");
	bool isPost = startsWith(line, "POST ");
	if (!isGet && !isPost) { sendHeader(c, 404, F("text/plain")); c.print(F("Bad method")); c.stop(); return; }

	const char* p = isGet ? (line + 4) : (line + 5);
	char path[128];
	size_t i = 0;
	while (*p && *p != ' ' && i + 1 < sizeof(path)) path[i++] = *p++;
	path[i] = 0;

	char* q = strchr(path, '?');
	char* query = nullptr;
	if (q) { *q = 0; query = q + 1; urlDecodeInPlace(query); }

	if (isGet && strcmp(path, "/") == 0) { pageHome(c); }
	else if (isGet && strcmp(path, "/internet1") == 0) { pageInternet1(c); }
	else if (isGet && strcmp(path, "/internet2") == 0) { pageInternet2(c); }
	else if (isGet && strcmp(path, "/gsm") == 0) { pageGsm(c); }
	else if (isPost && strcmp(path, "/internet1/save") == 0) { actionInternet1Save(c, query); }
	else if (isPost && strcmp(path, "/internet2/save") == 0) { actionInternet2Save(c, query); }
	else if (isPost && strcmp(path, "/gsm/save") == 0) { actionGsmSave(c, query); }
	else { pageNotFound(c); }

	c.stop();
}

// ---------------- Pages ----------------

void WebUI::pageHome(EthernetClient& c)
{
	sendHeader(c, 200, F("text/html; charset=utf-8"));

	c.print(F("<!doctype html><html><head><meta charset='utf-8'>"
		"<title>IVCH Settings</title>"
		"<style>body{font-family:sans-serif} code{background:#eee;padding:2px 4px}</style>"
		"</head><body>"));

	c.print(F("<h2>Устройство:настройки и статус</h2>"));

	IPAddress ip1 = ntpLan.localIP();
	c.print(F("<h3>INTERNET1 (W5500)</h3><ul>"));
	c.print(F("<li>IP:<code>")); c.print(ip1); c.print(F("</code></li>"));
	c.print(F("<li>Link:<code>")); c.print(Ethernet.linkReport()); c.print(F("</code></li>"));
	c.print(F("</ul>"));

	c.print(F("<h3>INTERNET2 (I2C)</h3><ul>"));
	Internet2Client::Status st{};
	bool ok = internet2.readStatus(st);
	c.print(F("<li>Status read:<code>")); c.print(ok ? "OK" : "FAIL"); c.print(F("</code></li>"));
	c.print(F("<li>IP:<code>")); c.print(ok ? st.ip : IPAddress(0, 0, 0, 0)); c.print(F("</code></li>"));
	c.print(F("<li>Last sync ok:<code>")); c.print(ok && st.lastSyncOk ? "YES" : "NO"); c.print(F("</code></li>"));
	c.print(F("</ul>"));

	c.print(F("<h3>GSM</h3><ul>"));
	c.print(F("<li>Registered:<code>")); c.print(gNetRegistered ? "YES" : "NO"); c.print(F("</code></li>"));
	c.print(F("<li>RSSI dBm:<code>")); c.print((int)gRssiDbm); c.print(F("</code></li>"));
	c.print(F("<li>Bars:<code>")); c.print((int)gSignalBars); c.print(F("</code></li>"));
	c.print(F("</ul>"));

	c.print(F("<h3>Разделы</h3><ul>"
		"<li><a href='/internet1'>Настройки INTERNET1</a></li>"
		"<li><a href='/internet2'>Настройки INTERNET2</a></li>"
		"<li><a href='/gsm'>Настройки GSM</a></li>"
		"</ul>"));

	c.print(F("</body></html>"));
}

void WebUI::pageInternet1(EthernetClient& c)
{
	LanIfStore::IfConfig cfg{};
	lanStore.load(LanIfStore::IF1, cfg, false);

	sendHeader(c, 200, F("text/html; charset=utf-8"));

	c.print(F("<!doctype html><html><head><meta charset='utf-8'>"
		"<title>INTERNET1</title></head><body>"));
	c.print(F("<h2>INTERNET1</h2>"));

	c.print(F("<p>Текущий IP (реальный):<code>"));
	c.print(ntpLan.localIP());
	c.print(F("</code></p>"));

	c.print(F("<form method='post' action='/internet1/save?'>"));

	c.print(F("<p>DHCP:"));
	c.print(F("<select name='dhcp'>"));
	c.print(F("<option value='1'")); if (cfg.dhcp) c.print(F(" selected")); c.print(F(">ON</option>"));
	c.print(F("<option value='0'")); if (!cfg.dhcp) c.print(F(" selected")); c.print(F(">OFF</option>"));
	c.print(F("</select></p>"));

	c.print(F("<p>NTP upstream:"));
	const char* ntpLabels[5] = { kNtpIpStr[0],kNtpIpStr[1],kNtpIpStr[2],kNtpIpStr[3],kNtpIpStr[4] };
	printSelectU8(c, "ntpIdx", cfg.ntpIdx % 5, 5, ntpLabels);
	c.print(F("</p>"));

	c.print(F("<p>Период:"));
	printSelectU8(c, "perIdx", cfg.periodIdx % 6, 6, kPeriodsName);
	c.print(F("</p>"));

	c.print(F("<p>IP:")); printIpInput(c, "ip", cfg.ip); c.print(F("</p>"));
	c.print(F("<p>MASK:")); printIpInput(c, "mask", cfg.mask); c.print(F("</p>"));
	c.print(F("<p>GW:")); printIpInput(c, "gw", cfg.gw); c.print(F("</p>"));
	c.print(F("<p>DNS:")); printIpInput(c, "dns", cfg.dns); c.print(F("</p>"));

	c.print(F("<p><button type='submit'>SAVE + APPLY</button> "));
	c.print(F("<a href='/'>Назад</a></p>"));

	c.print(F("</form></body></html>"));
}

void WebUI::pageInternet2(EthernetClient& c)
{
	LanIfStore::IfConfig cfg{};
	lanStore.load(LanIfStore::IF2, cfg, false);

	sendHeader(c, 200, F("text/html; charset=utf-8"));

	c.print(F("<!doctype html><html><head><meta charset='utf-8'>"
		"<title>INTERNET2</title></head><body>"));
	c.print(F("<h2>INTERNET2</h2>"));

	Internet2Client::Status st{};
	bool ok = internet2.readStatus(st);

	c.print(F("<p>Status read:<code>")); c.print(ok ? "OK" : "FAIL"); c.print(F("</code></p>"));
	c.print(F("<p>Текущий IP (реальный,I2C):<code>"));
	c.print(ok ? st.ip : IPAddress(0, 0, 0, 0));
	c.print(F("</code></p>"));

	c.print(F("<form method='post' action='/internet2/save?'>"));

	c.print(F("<p>DHCP:"));
	c.print(F("<select name='dhcp'>"));
	c.print(F("<option value='1'")); if (cfg.dhcp) c.print(F(" selected")); c.print(F(">ON</option>"));
	c.print(F("<option value='0'")); if (!cfg.dhcp) c.print(F(" selected")); c.print(F(">OFF</option>"));
	c.print(F("</select></p>"));

	const char* ntpLabels[5] = { kNtpIpStr[0],kNtpIpStr[1],kNtpIpStr[2],kNtpIpStr[3],kNtpIpStr[4] };
	c.print(F("<p>NTP upstream:"));
	printSelectU8(c, "ntpIdx", cfg.ntpIdx % 5, 5, ntpLabels);
	c.print(F("</p>"));

	c.print(F("<p>Период:"));
	printSelectU8(c, "perIdx", cfg.periodIdx % 6, 6, kPeriodsName);
	c.print(F("</p>"));

	c.print(F("<p>IP:")); printIpInput(c, "ip", cfg.ip); c.print(F("</p>"));
	c.print(F("<p>MASK:")); printIpInput(c, "mask", cfg.mask); c.print(F("</p>"));
	c.print(F("<p>GW:")); printIpInput(c, "gw", cfg.gw); c.print(F("</p>"));
	c.print(F("<p>DNS:")); printIpInput(c, "dns", cfg.dns); c.print(F("</p>"));

	c.print(F("<p><button type='submit'>SAVE + APPLY (I2C)</button> "));
	c.print(F("<a href='/'>Назад</a></p>"));

	c.print(F("</form></body></html>"));
}

void WebUI::pageGsm(EthernetClient& c)
{
	SyncSourcesStore::Data tmp = syncData;

	sendHeader(c, 200, F("text/html; charset=utf-8"));

	c.print(F("<!doctype html><html><head><meta charset='utf-8'>"
		"<title>GSM</title></head><body>"));
	c.print(F("<h2>GSM</h2>"));

	c.print(F("<p>Registered:<code>")); c.print(gNetRegistered ? "YES" : "NO"); c.print(F("</code></p>"));
	c.print(F("<p>RSSI dBm:<code>")); c.print((int)gRssiDbm); c.print(F("</code></p>"));

	c.print(F("<form method='post' action='/gsm/save?'>"));

	c.print(F("<p>Enable:"));
	c.print(F("<select name='en'>"));
	c.print(F("<option value='1'")); if (tmp.gsmEnable) c.print(F(" selected")); c.print(F(">ON</option>"));
	c.print(F("<option value='0'")); if (!tmp.gsmEnable) c.print(F(" selected")); c.print(F(">OFF</option>"));
	c.print(F("</select></p>"));

	c.print(F("<p>Operator idx (0..4):<input name='op' value='"));
	c.print(tmp.gsmProviderIdx);
	c.print(F("'></p>"));

	c.print(F("<p>Period idx (0..5):<input name='per' value='"));
	c.print(tmp.gsmPeriodIdx);
	c.print(F("'></p>"));

	c.print(F("<p><button type='submit'>SAVE</button> "));
	c.print(F("<a href='/'>Назад</a></p>"));

	c.print(F("</form></body></html>"));
}

void WebUI::pageNotFound(EthernetClient& c)
{
	sendHeader(c, 404, F("text/plain; charset=utf-8"));
	c.print(F("404 Not Found"));
}

// ---------------- Actions ----------------

void WebUI::actionInternet1Save(EthernetClient& c, char* query)
{
	if (!query) { sendHeader(c, 404, F("text/plain")); c.print(F("No query")); return; }

	LanIfStore::IfConfig cfg{};
	lanStore.load(LanIfStore::IF1, cfg, false);

	bool dhcp;
	uint8_t ntpIdx, perIdx;
	uint8_t ip[4], mask[4], gw[4], dns[4];

	if (parseBool01(findParam(query, "dhcp"), dhcp)) cfg.dhcp = dhcp ? 1 : 0;
	if (parseU8(findParam(query, "ntpIdx"), ntpIdx, 0, 4)) cfg.ntpIdx = ntpIdx;
	if (parseU8(findParam(query, "perIdx"), perIdx, 0, 5)) cfg.periodIdx = perIdx;

	const char* sip = findParam(query, "ip");
	const char* smask = findParam(query, "mask");
	const char* sgw = findParam(query, "gw");
	const char* sdns = findParam(query, "dns");

	if (sip && parseIp4(sip, ip)) memcpy(cfg.ip, ip, 4);
	if (smask && parseIp4(smask, mask)) memcpy(cfg.mask, mask, 4);
	if (sgw && parseIp4(sgw, gw)) memcpy(cfg.gw, gw, 4);
	if (sdns && parseIp4(sdns, dns)) memcpy(cfg.dns, dns, 4);

	lanStore.save(LanIfStore::IF1, cfg, true);
	applyInternet1FromStore();

	sendRedirect(c, "/internet1");
}

void WebUI::actionInternet2Save(EthernetClient& c, char* query)
{
	if (!query) { sendHeader(c, 404, F("text/plain")); c.print(F("No query")); return; }

	LanIfStore::IfConfig cfg{};
	lanStore.load(LanIfStore::IF2, cfg, false);

	bool dhcp;
	uint8_t ntpIdx, perIdx;
	uint8_t ip[4], mask[4], gw[4], dns[4];

	if (parseBool01(findParam(query, "dhcp"), dhcp)) cfg.dhcp = dhcp ? 1 : 0;
	if (parseU8(findParam(query, "ntpIdx"), ntpIdx, 0, 4)) cfg.ntpIdx = ntpIdx;
	if (parseU8(findParam(query, "perIdx"), perIdx, 0, 5)) cfg.periodIdx = perIdx;

	const char* sip = findParam(query, "ip");
	const char* smask = findParam(query, "mask");
	const char* sgw = findParam(query, "gw");
	const char* sdns = findParam(query, "dns");

	if (sip && parseIp4(sip, ip)) memcpy(cfg.ip, ip, 4);
	if (smask && parseIp4(smask, mask)) memcpy(cfg.mask, mask, 4);
	if (sgw && parseIp4(sgw, gw)) memcpy(cfg.gw, gw, 4);
	if (sdns && parseIp4(sdns, dns)) memcpy(cfg.dns, dns, 4);

	lanStore.save(LanIfStore::IF2, cfg, true);

	// ВАЖНО:applyInternet2FromStore() должен при DHCP=1 отправлять 0.0.0.0 в ip/mask/gw/dns
	applyInternet2FromStore();

	sendRedirect(c, "/internet2");
}

void WebUI::actionGsmSave(EthernetClient& c, char* query)
{
	if (!query) { sendHeader(c, 404, F("text/plain")); c.print(F("No query")); return; }

	SyncSourcesStore::Data tmp = syncData;

	bool en;
	uint8_t op, per;

	if (parseBool01(findParam(query, "en"), en)) tmp.gsmEnable = en ? 1 : 0;
	if (parseU8(findParam(query, "op"), op, 0, 4)) tmp.gsmProviderIdx = op;
	if (parseU8(findParam(query, "per"), per, 0, 5)) tmp.gsmPeriodIdx = per;

	syncData = tmp;
	syncStore.save(syncData);

	planner.onSettingsChanged();

	sendRedirect(c, "/gsm");
}