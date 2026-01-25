#include "W5500NtpServerClient.h"

#include <SPI.h>
#include <IPAddress.h>
#include <Ethernet_Generic.h>
#include <EthernetUdp.h>
#include <string.h>

// ---- W5500 on SPI1 ----
#define USE_THIS_SS_PIN PA4
#define CUR_PIN_MISO PA6
#define CUR_PIN_MOSI PA7
#define CUR_PIN_SCK PA5

SPIClass SPI_New(CUR_PIN_MOSI, CUR_PIN_MISO, CUR_PIN_SCK);

static EthernetUDP udpSrv; // local NTP server socket (port 123)
static EthernetUDP udpCli; // upstream NTP client socket

static const uint16_t NTP_PORT = 123;
static const uint32_t NTP_UNIX_EPOCH_DELTA = 2208988800UL; // 1900->1970 seconds

static uint32_t unixToNtp1900(uint32_t unixUtc) { return unixUtc + NTP_UNIX_EPOCH_DELTA; }
static uint32_t ntp1900ToUnix(uint32_t ntpSec1900) { return ntpSec1900 - NTP_UNIX_EPOCH_DELTA; }

// ---- state for edge detection / one-shot prints ----
static bool s_prevLinkUp = false;
static bool s_printedAfterConnect = false;

// -------------------- link/time helpers --------------------
bool W5500NtpServerClient::linkUp() const
{
	const char* s = Ethernet.linkReport();
	if (!s) return false;

	//  –»“»„Ќќ:"NO LINK" содержит подстроку "LINK"
	if (strstr(s, "NO LINK") != nullptr) return false;

	if (strcmp(s, "LINK") == 0) return true;
	if (strstr(s, "LinkON") != nullptr) return true;
	if (strstr(s, "LINKON") != nullptr) return true;
	if (strcmp(s, "ON") == 0) return true;
	if (strcmp(s, "UP") == 0) return true;

	return false;
}

bool W5500NtpServerClient::ipValid(const uint8_t ip[4])
{
	// минимальна€ проверка "не нулевой и не 255.*"
	return (ip[0] != 0 && ip[0] != 255);
}

uint32_t W5500NtpServerClient::msToNtpFrac(uint16_t ms)
{
	return (uint32_t)(((uint64_t)ms << 32) / 1000ULL);
}

void W5500NtpServerClient::nowUtcSecFrac(uint32_t& unixSec, uint32_t& ntpFrac) const
{
	if (_baseUnix == 0) { unixSec = 0; ntpFrac = 0; return; }

	uint32_t nowMs = millis();
	uint32_t dMs = nowMs - _baseMillis; // wrap-safe
	uint64_t totalMs = (uint64_t)_baseMs + (uint64_t)dMs;

	unixSec = _baseUnix + (uint32_t)(totalMs / 1000ULL);
	uint16_t remMs = (uint16_t)(totalMs % 1000ULL);
	ntpFrac = msToNtpFrac(remMs);
}

uint32_t W5500NtpServerClient::nowUtcSeconds() const
{
	uint32_t s, f;
	nowUtcSecFrac(s, f);
	return s;
}

// -------------------- begin/config --------------------
void W5500NtpServerClient::begin()
{
	pinMode(USE_THIS_SS_PIN, OUTPUT);
	digitalWrite(USE_THIS_SS_PIN, HIGH);

	SPI_New.begin();

	extern SPIClass* pCUR_SPI; // Ethernet_Generic global
	pCUR_SPI = &SPI_New;

	Ethernet.init(USE_THIS_SS_PIN);

	ethBegin();

	udpSrv.begin(NTP_PORT);
	udpCli.begin(0);

	_nextSyncMs = millis() + 1000;

	_syncState = SYNC_IDLE;
	_syncDeadlineMs = 0;
	_syncRetriesLeft = 0;
	_st.syncInProgress = false;

	s_prevLinkUp = linkUp();
	s_printedAfterConnect = false;

	_linkUpSinceMs = s_prevLinkUp ? millis() : 0;
	_lastGoodIpMs = 0;
	_lastReinitMs = 0;
	_dhcpFailStreak = 0;
	_wantNetReinit = false;
	_st.netReinitCount = 0;
	_st.lastErr = 0;
}

void W5500NtpServerClient::ethBegin()
{
	uint8_t mac[6] = { 0xDE,0xAD,0xBE,0xEF,0x02,0x02 };

	extern SPIClass* pCUR_SPI;
	pCUR_SPI = &SPI_New;

	const bool lu = linkUp();
	const bool wantDhcp = (_cfg.dhcp != 0);

	if (wantDhcp && lu)
	{
		int ok = Ethernet.begin(mac, 3000, 1000);

		if (!ok)
		{
			Serial.println(F("[ETH] DHCP failed -> fallback to static"));
			Ethernet.begin(mac,
				IPAddress(_cfg.ip[0], _cfg.ip[1], _cfg.ip[2], _cfg.ip[3]),
				IPAddress(_cfg.dns[0], _cfg.dns[1], _cfg.dns[2], _cfg.dns[3]),
				IPAddress(_cfg.gw[0], _cfg.gw[1], _cfg.gw[2], _cfg.gw[3]),
				IPAddress(_cfg.mask[0], _cfg.mask[1], _cfg.mask[2], _cfg.mask[3]));
		}
	}
	else
	{
		Ethernet.begin(mac,
			IPAddress(_cfg.ip[0], _cfg.ip[1], _cfg.ip[2], _cfg.ip[3]),
			IPAddress(_cfg.dns[0], _cfg.dns[1], _cfg.dns[2], _cfg.dns[3]),
			IPAddress(_cfg.gw[0], _cfg.gw[1], _cfg.gw[2], _cfg.gw[3]),
			IPAddress(_cfg.mask[0], _cfg.mask[1], _cfg.mask[2], _cfg.mask[3]));
	}

	Serial.print(F("linkReport=")); Serial.println(Ethernet.linkReport());
	if (linkUp())
	{
		Serial.print(F("localIP=")); Serial.println(Ethernet.localIP());
		Serial.print(F("gw=")); Serial.println(Ethernet.gatewayIP());
		Serial.print(F("mask=")); Serial.println(Ethernet.subnetMask());
		Serial.print(F("dns=")); Serial.println(Ethernet.dnsServerIP());
	}
	else
	{
		Serial.println(F("localIP=0.0.0.0 (link down)"));
	}
}

void W5500NtpServerClient::applyNetCfg(const NetCfg& cfg)
{
	_cfg = cfg;

	extern SPIClass* pCUR_SPI;
	pCUR_SPI = &SPI_New;

	ethBegin();

	udpSrv.stop();
	udpCli.stop();
	udpSrv.begin(NTP_PORT);
	udpCli.begin(0);

	_syncState = SYNC_IDLE;
	_st.syncInProgress = false;

	s_printedAfterConnect = false;

	// reset supervision timers
	_wantNetReinit = false;
	_dhcpFailStreak = 0;
	_linkUpSinceMs = linkUp() ? millis() : 0;
	_lastGoodIpMs = 0;
	_lastReinitMs = 0;
	_st.lastErr = 0;
}

void W5500NtpServerClient::setMasterTimeUtc(uint32_t unixUtc, uint16_t ms)
{
	if (ms > 999) ms = 999;
	_baseUnix = unixUtc;
	_baseMillis = millis();
	_baseMs = ms;
	_st.haveMasterTime = (unixUtc != 0);
}

void W5500NtpServerClient::requestSyncNow()
{
	_syncReq = true;
}

void W5500NtpServerClient::requestNetReinit()
{
	_wantNetReinit = true;
}

// -------------------- self-heal core --------------------
void W5500NtpServerClient::doNetReinit(uint8_t errCode)
{
	const uint32_t now = millis();

	// антиспам:не чаще 1 раза в 10 секунд
	if (now - _lastReinitMs < 10000UL) return;
	_lastReinitMs = now;

	_st.lastErr = errCode;
	if (_st.netReinitCount < 255) _st.netReinitCount++;

	Serial.print(F("[NET2] REINIT net,err="));
	Serial.println((unsigned)errCode);

	ethBegin();

	udpSrv.stop();
	udpCli.stop();
	udpSrv.begin(NTP_PORT);
	udpCli.begin(0);

	// stop any in-progress upstream sync
	if (_syncState != SYNC_IDLE) finishUpstreamSync(false, 0);

	_dhcpFailStreak = 0;
	s_printedAfterConnect = false;

	// after reinit allow time for DHCP
	_linkUpSinceMs = linkUp() ? now : 0;
}

// -------------------- main tick --------------------
void W5500NtpServerClient::tick()
{
	const uint32_t now = millis();

	const bool lu = linkUp();
	_st.linkUp = lu;
	_st.dhcp = (_cfg.dhcp != 0);

	// manual reinit request (from master over I2C)
	if (_wantNetReinit) {
		_wantNetReinit = false;
		doNetReinit(3 /*manual*/);
	}

	// LINK edge detect
	if (!s_prevLinkUp && lu)
	{
		Serial.println(F("[ETH] Link UP -> re-init"));
		_linkUpSinceMs = now;
		doNetReinit(4 /*link-flap*/);
	}
	if (s_prevLinkUp && !lu)
	{
		// link went down:clear IP,stop sync quickly
		_st.ip[0] = 0; _st.ip[1] = 0; _st.ip[2] = 0; _st.ip[3] = 0;
		s_printedAfterConnect = false;

		if (_syncState != SYNC_IDLE) finishUpstreamSync(false, 0);

		// do not keep DHCP failure streak while link down
		_dhcpFailStreak = 0;
		_linkUpSinceMs = 0;

		s_prevLinkUp = lu;
		return;
	}
	s_prevLinkUp = lu;

	// Link up:read IP
	IPAddress ip = Ethernet.localIP();
	_st.ip[0] = ip[0];
	_st.ip[1] = ip[1];
	_st.ip[2] = ip[2];
	_st.ip[3] = ip[3];

	const bool okIp = ipValid(_st.ip);
	if (okIp) _lastGoodIpMs = now;

	// DHCP maintain (only when link up and dhcp enabled)
	if (_cfg.dhcp) {
		int m = Ethernet.maintain();
		// typical codes:0 no action,1 renew fail,2 renew ok,3 rebind fail,4 rebind ok
		if (m == 1 || m == 3) {
			if (_dhcpFailStreak < 255) _dhcpFailStreak++;
			if (_dhcpFailStreak >= 3) {
				doNetReinit(2 /*dhcp-fail*/);
			}
		}
		else if (m == 2 || m == 4) {
			_dhcpFailStreak = 0;
		}
	}

	// Self-heal:link is up but no valid IP too long (DHCP stuck or stack glitch)
	// wait a little after link up
	if (!okIp) {
		if (_linkUpSinceMs == 0) _linkUpSinceMs = now;

		// если 8 секунд после link up нет IP Ч реинит
		if (now - _linkUpSinceMs > 8000UL) {
			doNetReinit(1 /*no-ip-too-long*/);
		}
	}

	// One-shot print when connected and IP assigned
	if (lu && okIp && !s_printedAfterConnect)
	{
		s_printedAfterConnect = true;

		IPAddress mask = Ethernet.subnetMask();
		IPAddress gw = Ethernet.gatewayIP();
		IPAddress dns = Ethernet.dnsServerIP();

		Serial.println(F("=== INTERNET2 CONNECTED (W5500) ==="));
		Serial.print(F("DHCP=")); Serial.println(_cfg.dhcp ? 1 : 0);

		Serial.print(F("IP="));
		Serial.print(ip[0]); Serial.print('.'); Serial.print(ip[1]); Serial.print('.');
		Serial.print(ip[2]); Serial.print('.'); Serial.println(ip[3]);

		Serial.print(F("MASK="));
		Serial.print(mask[0]); Serial.print('.'); Serial.print(mask[1]); Serial.print('.');
		Serial.print(mask[2]); Serial.print('.'); Serial.println(mask[3]);

		Serial.print(F("GW="));
		Serial.print(gw[0]); Serial.print('.'); Serial.print(gw[1]); Serial.print('.');
		Serial.print(gw[2]); Serial.print('.'); Serial.println(gw[3]);

		Serial.print(F("DNS="));
		Serial.print(dns[0]); Serial.print('.'); Serial.print(dns[1]); Serial.print('.');
		Serial.print(dns[2]); Serial.print('.'); Serial.println(dns[3]);

		Serial.print(F("UPSTREAM_NTP="));
		Serial.print(_cfg.upstream[0]); Serial.print('.'); Serial.print(_cfg.upstream[1]); Serial.print('.');
		Serial.print(_cfg.upstream[2]); Serial.print('.'); Serial.println(_cfg.upstream[3]);

		Serial.print(F("SYNC_PERIOD_MS=")); Serial.println((unsigned long)_cfg.periodMs);
		Serial.println(F("================================"));
	}

	// Local NTP server tick
	tickNtpServer();

	// Upstream non-blocking client
	tickNtpClient();
}

// -------------------- NTP server (local clients) --------------------
void W5500NtpServerClient::tickNtpServer()
{
	int packetSize = udpSrv.parsePacket();
	if (packetSize <= 0) return;

	uint8_t req[48];
	int n = udpSrv.read(req, 48);
	if (n < 48) return;

	if (!_st.haveMasterTime) return;

	uint32_t unixSec = 0, frac = 0;
	nowUtcSecFrac(unixSec, frac);
	if (unixSec == 0) return;

	uint32_t txSec1900 = unixToNtp1900(unixSec);

	uint8_t resp[48] = { 0 };

	resp[0] = (0 << 6) | (4 << 3) | 4; // server
	resp[1] = 1;
	resp[2] = 6;
	resp[3] = (uint8_t)-20;

	resp[12] = 'L'; resp[13] = 'O'; resp[14] = 'C'; resp[15] = '2';

	memcpy(&resp[24], &req[40], 8);

	resp[32] = (txSec1900 >> 24) & 0xFF;
	resp[33] = (txSec1900 >> 16) & 0xFF;
	resp[34] = (txSec1900 >> 8) & 0xFF;
	resp[35] = (txSec1900) & 0xFF;
	resp[36] = (frac >> 24) & 0xFF;
	resp[37] = (frac >> 16) & 0xFF;
	resp[38] = (frac >> 8) & 0xFF;
	resp[39] = (frac) & 0xFF;

	resp[40] = (txSec1900 >> 24) & 0xFF;
	resp[41] = (txSec1900 >> 16) & 0xFF;
	resp[42] = (txSec1900 >> 8) & 0xFF;
	resp[43] = (txSec1900) & 0xFF;
	resp[44] = (frac >> 24) & 0xFF;
	resp[45] = (frac >> 16) & 0xFF;
	resp[46] = (frac >> 8) & 0xFF;
	resp[47] = (frac) & 0xFF;

	udpSrv.beginPacket(udpSrv.remoteIP(), udpSrv.remotePort());
	udpSrv.write(resp, 48);
	udpSrv.endPacket();
}

// -------------------- Upstream NTP client (non-blocking FSM) --------------------
void W5500NtpServerClient::tickNtpClient()
{
	if (_syncState != SYNC_IDLE) {
		pollUpstreamSync();
		return;
	}

	uint32_t now = millis();
	if (_syncReq || (int32_t)(now - _nextSyncMs) >= 0) {
		_syncReq = false;
		_nextSyncMs = now + _cfg.periodMs;

		if (!_st.haveMasterTime) return;

		startUpstreamSync();
	}
}

void W5500NtpServerClient::startUpstreamSync()
{
	_st.syncInProgress = true;
	_st.lastSyncOk = false;
	_st.lastSyncMs = millis();

	_syncState = SYNC_WAIT;
	_syncDeadlineMs = millis() + 1200;
	_syncRetriesLeft = 1;

	IPAddress up(_cfg.upstream[0], _cfg.upstream[1], _cfg.upstream[2], _cfg.upstream[3]);

	uint8_t pkt[48] = { 0 };
	pkt[0] = (0 << 6) | (4 << 3) | 3;
	pkt[1] = 0; pkt[2] = 6; pkt[3] = 0xEC;

	udpCli.beginPacket(up, NTP_PORT);
	udpCli.write(pkt, 48);
	udpCli.endPacket();
}

void W5500NtpServerClient::pollUpstreamSync()
{
	if (!linkUp()) {
		finishUpstreamSync(false, 0);
		return;
	}

	int sz = udpCli.parsePacket();
	if (sz >= 48) {
		uint8_t resp[48];
		int n = udpCli.read(resp, 48);
		if (n >= 48) {
			uint32_t sec1900 =
				((uint32_t)resp[40] << 24) |
				((uint32_t)resp[41] << 16) |
				((uint32_t)resp[42] << 8) |
				(uint32_t)resp[43];

			if (sec1900 >= NTP_UNIX_EPOCH_DELTA) {
				uint32_t ntpUtc = ntp1900ToUnix(sec1900);
				finishUpstreamSync(true, ntpUtc);
				return;
			}
		}
	}

	if ((int32_t)(millis() - _syncDeadlineMs) >= 0) {
		if (_syncRetriesLeft > 0) {
			_syncRetriesLeft--;

			IPAddress up(_cfg.upstream[0], _cfg.upstream[1], _cfg.upstream[2], _cfg.upstream[3]);

			uint8_t pkt[48] = { 0 };
			pkt[0] = (0 << 6) | (4 << 3) | 3;
			pkt[1] = 0; pkt[2] = 6; pkt[3] = 0xEC;

			udpCli.beginPacket(up, NTP_PORT);
			udpCli.write(pkt, 48);
			udpCli.endPacket();

			_syncDeadlineMs = millis() + 1200;
			return;
		}

		finishUpstreamSync(false, 0);
		return;
	}
}

void W5500NtpServerClient::finishUpstreamSync(bool ok, uint32_t ntpUtc)
{
	_syncState = SYNC_IDLE;
	_st.syncInProgress = false;
	_st.lastSyncMs = millis();

	if (!ok || ntpUtc == 0) {
		_st.lastSyncOk = false;
		return;
	}

	uint32_t curUtc = nowUtcSeconds();
	_st.lastNtpUtc = ntpUtc;
	_st.lastOffsetSec = (int32_t)ntpUtc - (int32_t)curUtc;
	_st.lastSyncOk = true;
}

uint32_t W5500NtpServerClient::syncUpstreamOnce()
{
	if (_syncState == SYNC_IDLE && _st.haveMasterTime) startUpstreamSync();
	return 0;
}

// -------------------- UART clients --------------------
void W5500NtpServerClient::printNowTo(Stream& s) const
{
	uint32_t sec, frac;
	nowUtcSecFrac(sec, frac);

	uint16_t ms = 0;
	if (_baseUnix != 0) ms = (uint16_t)(((uint64_t)frac * 1000ULL) >> 32);

	auto st = status();
	s.print("UNIX:"); s.println((unsigned long)sec);
	s.print("MS:"); s.println((unsigned)ms);
	s.print("IP:"); s.print(st.ip[0]); s.print('.'); s.print(st.ip[1]); s.print('.');
	s.print(st.ip[2]); s.print('.'); s.println(st.ip[3]);
	s.print("SYNC_OK:"); s.println(st.lastSyncOk ? 1 : 0);
	s.print("OFFS:"); s.println(st.lastOffsetSec);
}