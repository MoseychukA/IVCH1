#include "W5500NtpServerClient.h"

#include <SPI.h>
#include <IPAddress.h>
#include <Ethernet_Generic.h>
#include <EthernetUdp.h>


static bool s_prevLinkUp = false;
static bool s_printedAfterConnect = false;

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

// -------------------- link/time helpers --------------------
bool W5500NtpServerClient::linkUp() const {
	const char* s = Ethernet.linkReport();
	return (s && strcmp(s, "LINK") == 0);
}

uint32_t W5500NtpServerClient::msToNtpFrac(uint16_t ms) {
	// frac = ms/1000 * 2^32
	return (uint32_t)(((uint64_t)ms << 32) / 1000ULL);
}

void W5500NtpServerClient::nowUtcSecFrac(uint32_t& unixSec, uint32_t& ntpFrac) const {
	if (_baseUnix == 0) { unixSec = 0; ntpFrac = 0; return; }

	uint32_t nowMs = millis();
	uint32_t dMs = nowMs - _baseMillis; // wrap-safe
	uint64_t totalMs = (uint64_t)_baseMs + (uint64_t)dMs;

	unixSec = _baseUnix + (uint32_t)(totalMs / 1000ULL);
	uint16_t remMs = (uint16_t)(totalMs % 1000ULL);
	ntpFrac = msToNtpFrac(remMs);
}

uint32_t W5500NtpServerClient::nowUtcSeconds() const {
	uint32_t s, f;
	nowUtcSecFrac(s, f);
	return s;
}

// -------------------- begin/config --------------------
void W5500NtpServerClient::begin() {
	pinMode(USE_THIS_SS_PIN, OUTPUT);
	digitalWrite(USE_THIS_SS_PIN, HIGH);

	Ethernet.init(USE_THIS_SS_PIN);
	SPI_New.begin();

	// Ethernet_Generic uses global pCUR_SPI
	extern SPIClass* pCUR_SPI;
	pCUR_SPI = &SPI_New;

	ethBegin();

	udpSrv.begin(NTP_PORT);
	udpCli.begin(0);

	_nextSyncMs = millis() + 1000;

	// reset FSM
	_syncState = SYNC_IDLE;
	_syncDeadlineMs = 0;
	_syncRetriesLeft = 0;
	_st.syncInProgress = false;
}

void W5500NtpServerClient::ethBegin() {
	uint8_t mac[6] = { 0xDE,0xAD,0xBE,0xEF,0x02,0x02 };

	// Fast startup:if LINK down -> skip DHCP to avoid long delays
	if (_cfg.dhcp && linkUp()) {
		int ok = Ethernet.begin(mac, &SPI_New, 3000, 1000);
		if (!ok) _cfg.dhcp = 0;
	}
	else {
		_cfg.dhcp = 0;
	}

	if (!_cfg.dhcp) {
		Ethernet.begin(mac,
			IPAddress(_cfg.ip[0], _cfg.ip[1], _cfg.ip[2], _cfg.ip[3]),
			IPAddress(_cfg.dns[0], _cfg.dns[1], _cfg.dns[2], _cfg.dns[3]),
			IPAddress(_cfg.gw[0], _cfg.gw[1], _cfg.gw[2], _cfg.gw[3]),
			IPAddress(_cfg.mask[0], _cfg.mask[1], _cfg.mask[2], _cfg.mask[3]));
	}
}

void W5500NtpServerClient::applyNetCfg(const NetCfg& cfg) {
	_cfg = cfg;

	// ensure Ethernet_Generic uses our SPI
	extern SPIClass* pCUR_SPI;
	pCUR_SPI = &SPI_New;

	ethBegin();

	udpSrv.stop();
	udpCli.stop();
	udpSrv.begin(NTP_PORT);
	udpCli.begin(0);

	// stop any in-progress upstream sync
	_syncState = SYNC_IDLE;
	_st.syncInProgress = false;
}

void W5500NtpServerClient::setMasterTimeUtc(uint32_t unixUtc, uint16_t ms) {
	if (ms > 999) ms = 999;
	_baseUnix = unixUtc;
	_baseMillis = millis();
	_baseMs = ms;
	_st.haveMasterTime = (unixUtc != 0);
}

void W5500NtpServerClient::requestSyncNow() {
	_syncReq = true;
}

// -------------------- main tick --------------------
void W5500NtpServerClient::tick() {
	_st.linkUp = linkUp();
	_st.dhcp = (_cfg.dhcp != 0);

	IPAddress ip = Ethernet.localIP();
	_st.ip[0] = ip[0];
	_st.ip[1] = ip[1];
	_st.ip[2] = ip[2];
	_st.ip[3] = ip[3];
	// One-shot print when link becomes UP and IP is assigned
	bool ipValid = (ip[0] != 0 && ip[0] != 255); // грубая проверка (0.0.0.0 / 255.x.x.x)
	if (!_st.linkUp) {
		s_printedAfterConnect = false; // сброс,чтобы при следующем подключении напечатать снова
	}

	if (_st.linkUp && ipValid && !s_printedAfterConnect) 
	{
		s_printedAfterConnect = true;

		IPAddress mask = Ethernet.subnetMask();
		IPAddress gw = Ethernet.gatewayIP();
		IPAddress dns = Ethernet.dnsServerIP();

		Serial.println(F("=== INTERNET2 CONNECTED (W5500) ==="));
		Serial.print(F("DHCP=")); Serial.println(_cfg.dhcp ? 1 : 0);

		Serial.print(F("IP="));
		Serial.print(ip[0]); Serial.print('.'); Serial.print(ip[1]); Serial.print('.'); Serial.print(ip[2]); Serial.print('.'); Serial.println(ip[3]);

		Serial.print(F("MASK="));
		Serial.print(mask[0]); Serial.print('.'); Serial.print(mask[1]); Serial.print('.'); Serial.print(mask[2]); Serial.print('.'); Serial.println(mask[3]);

		Serial.print(F("GW="));
		Serial.print(gw[0]); Serial.print('.'); Serial.print(gw[1]); Serial.print('.'); Serial.print(gw[2]); Serial.print('.'); Serial.println(gw[3]);

		Serial.print(F("DNS="));
		Serial.print(dns[0]); Serial.print('.'); Serial.print(dns[1]); Serial.print('.'); Serial.print(dns[2]); Serial.print('.'); Serial.println(dns[3]);

		Serial.print(F("UPSTREAM_NTP="));
		Serial.print(_cfg.upstream[0]); Serial.print('.'); Serial.print(_cfg.upstream[1]); Serial.print('.');
		Serial.print(_cfg.upstream[2]); Serial.print('.'); Serial.println(_cfg.upstream[3]);

		Serial.print(F("SYNC_PERIOD_MS=")); Serial.println((unsigned long)_cfg.periodMs);
		Serial.println(F("================================"));
	}
	// Local NTP server tick (non-blocking)
	tickNtpServer();

	// If link down:stop upstream attempt and exit (no maintain)
	if (!_st.linkUp) {
		if (_syncState != SYNC_IDLE) finishUpstreamSync(false, 0);
		return;
	}

	// DHCP maintain only when link up
	if (_cfg.dhcp) Ethernet.maintain();

	// Upstream non-blocking client
	tickNtpClient();
}

// -------------------- NTP server (local clients) --------------------
void W5500NtpServerClient::tickNtpServer() {
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

	// LI=0,VN=4,Mode=4(Server)
	resp[0] = (0 << 6) | (4 << 3) | 4;
	resp[1] = 1; // stratum
	resp[2] = 6; // poll
	resp[3] = (uint8_t)-20; // precision (rough)

	// Reference ID
	resp[12] = 'L'; resp[13] = 'O'; resp[14] = 'C'; resp[15] = '2';

	// Originate Timestamp = client's Transmit Timestamp (request[40..47])
	memcpy(&resp[24], &req[40], 8);

	// Receive Timestamp (current)
	resp[32] = (txSec1900 >> 24) & 0xFF;
	resp[33] = (txSec1900 >> 16) & 0xFF;
	resp[34] = (txSec1900 >> 8) & 0xFF;
	resp[35] = (txSec1900) & 0xFF;
	resp[36] = (frac >> 24) & 0xFF;
	resp[37] = (frac >> 16) & 0xFF;
	resp[38] = (frac >> 8) & 0xFF;
	resp[39] = (frac) & 0xFF;

	// Transmit Timestamp (current)
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
void W5500NtpServerClient::tickNtpClient() {
	// If currently waiting for reply,poll it
	if (_syncState != SYNC_IDLE) {
		pollUpstreamSync();
		return;
	}

	// start new attempt on schedule or immediate request
	uint32_t now = millis();
	if (_syncReq || (int32_t)(now - _nextSyncMs) >= 0) {
		_syncReq = false;
		_nextSyncMs = now + _cfg.periodMs;

		if (!_st.haveMasterTime) return; // no base -> cannot compute meaningful offset

		startUpstreamSync();
	}
}

void W5500NtpServerClient::startUpstreamSync() {
	_st.syncInProgress = true;
	_st.lastSyncOk = false;
	_st.lastSyncMs = millis();

	_syncState = SYNC_WAIT;
	_syncDeadlineMs = millis() + 1200;
	_syncRetriesLeft = 1; // one retry

	IPAddress up(_cfg.upstream[0], _cfg.upstream[1], _cfg.upstream[2], _cfg.upstream[3]);

	uint8_t pkt[48] = { 0 };
	// LI=0,VN=4,Mode=3(Client)
	pkt[0] = (0 << 6) | (4 << 3) | 3;
	pkt[1] = 0; pkt[2] = 6; pkt[3] = 0xEC;

	udpCli.beginPacket(up, NTP_PORT);
	udpCli.write(pkt, 48);
	udpCli.endPacket();
}

void W5500NtpServerClient::pollUpstreamSync() {
	// if link down mid-wait -> fail quickly
	if (!linkUp()) {
		finishUpstreamSync(false, 0);
		return;
	}

	// response?
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
		// invalid packet -> ignore,keep waiting until timeout
	}

	// timeout?
	if ((int32_t)(millis() - _syncDeadlineMs) >= 0) {
		if (_syncRetriesLeft > 0) {
			_syncRetriesLeft--;

			// resend
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

void W5500NtpServerClient::finishUpstreamSync(bool ok, uint32_t ntpUtc) {
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

// -------------------- Compatibility:syncUpstreamOnce() --------------------
// В вашем .h этот метод объявлен,но в неблокирующем варианте он не используется.
// Оставляем как "неблокирующий stub",чтобы не было ошибок линковки.
uint32_t W5500NtpServerClient::syncUpstreamOnce() {
	// НЕ БЛОКИРУЕТ.
	// Для ручного режима можно инициировать FSM:
	if (_syncState == SYNC_IDLE && _st.haveMasterTime) startUpstreamSync();
	return 0;
}

// -------------------- UART clients --------------------
void W5500NtpServerClient::printNowTo(Stream& s) const {
	uint32_t sec, frac;
	nowUtcSecFrac(sec, frac);

	uint16_t ms = 0;
	if (_baseUnix != 0) ms = (uint16_t)(((uint64_t)frac * 1000ULL) >> 32);

	auto st = status();
	s.print("UNIX:"); s.println((unsigned long)sec);
	s.print("MS:"); s.println((unsigned)ms);
	s.print("IP:"); s.print(st.ip[0]); s.print('.'); s.print(st.ip[1]); s.print('.'); s.print(st.ip[2]); s.print('.'); s.println(st.ip[3]);
	s.print("SYNC_OK:"); s.println(st.lastSyncOk ? 1 : 0);
	s.print("OFFS:"); s.println(st.lastOffsetSec);
}