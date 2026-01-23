#include "NtpLanService.h"

// ---- constructor ----
NtpLanService::NtpLanService(RealtimeClock& rtc, AT24C128Settings::Config& cfg)
	:_rtc(rtc), _cfg(cfg) {}

bool NtpLanService::begin(const NetConfig& net, uint8_t csPin)
{
	_net = net;
	_csPin = csPin;

	// Важно:CS всех остальных SPI устройств должен быть HIGH
	pinMode(_csPin, OUTPUT);
	digitalWrite(_csPin, HIGH);

	// Ethernet init (для многих реализаций)
	// Для Ethernet/Ethernet2 на некоторых платформах можно использовать Ethernet.init(csPin),
	// но на Arduino_STM32 может не быть. Поэтому используем стандартный pin.
	// Если у вас есть Ethernet.init(csPin) — можно добавить.
#if defined(ETHERNET_HAS_INIT)
	Ethernet.init(_csPin);
#endif

	Ethernet.begin(_net.mac, _net.ip, _net.dns, _net.gw, _net.mask);

	// сервер UDP:порт 123
	if (_udpSrv.begin(NTP_PORT) == 0) return false;

	// клиент UDP:любой порт
	_udpCli.begin(0);

	_nextSyncMs = millis(); // первая попытка скоро
	_syncRequested = true;

	return true;
}

void NtpLanService::forceSyncNow()
{
	_syncRequested = true;
	_nextSyncMs = millis();
}

void NtpLanService::tick()
{
	handleNtpServer();
	handleSyncClient();
}

// ---- NTP server ----
void NtpLanService::handleNtpServer()
{
	int packetSize = _udpSrv.parsePacket();
	if (packetSize <= 0) return;

	uint8_t req[48];
	int n = _udpSrv.read(req, sizeof(req));
	if (n < 48) return;

	// Клиентский режим обычно 3 (client)
	uint8_t li_vn_mode = req[0];
	uint8_t mode = li_vn_mode & 0x07;
	(void)mode;

	// Получим текущее UTC из RTC (который хранит локальное target TZ)
	RTCTime tLocal = _rtc.getTime();
	uint32_t unixUtc = unixFromRtcLocal(tLocal, _cfg.tzTargetHours);

	uint32_t txSec1900 = unixToNtp1900(unixUtc);
	uint32_t txFrac = millisToFrac(millis() % 1000);

	// Время приёма (rx) можно поставить как tx (достаточно для LAN)
	uint32_t rxSec1900 = txSec1900;
	uint32_t rxFrac = txFrac;

	uint8_t resp[48];
	buildNtpResponse(resp, rxSec1900, rxFrac, txSec1900, txFrac, req);

	_udpSrv.beginPacket(_udpSrv.remoteIP(), _udpSrv.remotePort());
	_udpSrv.write(resp, 48);
	_udpSrv.endPacket();
}

// ---- NTP client sync ----
void NtpLanService::handleSyncClient()
{
	uint32_t now = millis();

	if (_syncRequested || (int32_t)(now - _nextSyncMs) >= 0) {
		_syncRequested = false;
		_nextSyncMs = now + _net.syncPeriodMs;

		uint8_t pkt[48] = { 0 };
		pkt[0] = 0b11100011;
		pkt[1] = 0; pkt[2] = 6; pkt[3] = 0xEC;

		_udpCli.beginPacket(_net.upstreamIp, _net.upstreamPort);
		_udpCli.write(pkt, 48);
		_udpCli.endPacket();

		// <<< ВСТАВИТЬ СЮДА >>>
		_lastAttemptMs = now;
		_lastSyncOk = false;
		_lastSyncMs = now;

		// return НЕ нужен — мы продолжаем и можем тут же принять ответ,если пришёл
	}

	// читаем ответ (не блокируем)
	int sz = _udpCli.parsePacket();

	// <<< ВСТАВИТЬ СЮДА (диагностика) >>>
	if (sz > 0) {
		Serial.print("[NTP-CLI] got packet size=");
		Serial.print(sz);
		Serial.print(" from ");
		IPAddress rip = _udpCli.remoteIP();
		Serial.print(rip[0]); Serial.print('.');
		Serial.print(rip[1]); Serial.print('.');
		Serial.print(rip[2]); Serial.print('.');
		Serial.print(rip[3]);
		Serial.print(":");
		Serial.println(_udpCli.remotePort());
	}

	if (sz < 48) return;

	uint8_t resp[48];
	int n = _udpCli.read(resp, 48);
	if (n < 48) return;

	// Transmit Timestamp сервера:байты 40..47
	uint32_t sec1900 =
		((uint32_t)resp[40] << 24) | ((uint32_t)resp[41] << 16) | ((uint32_t)resp[42] << 8) | (uint32_t)resp[43];

	if (sec1900 < 2208988800UL) return; // защита
	uint32_t unixUtc = ntp1900ToUnix(sec1900);

	// выставим RTC в локальном target TZ
	RTCTime tLocal;
	rtcLocalFromUnix(unixUtc, _cfg.tzTargetHours, tLocal);

	// dayOfWeek в вашем RTCSupport:1=Mon..7=Sun
	_rtc.setTime(tLocal.second, tLocal.minute, tLocal.hour,
		tLocal.dayOfWeek, tLocal.dayOfMonth, tLocal.month, tLocal.year);

	_lastSyncOk = true;
	_lastSuccessMs = millis();
}

// ---- Build NTP response ----
void NtpLanService::buildNtpResponse(uint8_t* out48,
	uint32_t rxSeconds1900, uint32_t rxFrac,
	uint32_t txSeconds1900, uint32_t txFrac,
	const uint8_t* clientRequest48)
{
	memset(out48, 0, 48);

	// LI=0,VN=4,Mode=4 (server)
	out48[0] = (0 << 6) | (4 << 3) | (4);
	out48[1] = 1; // stratum 1 (мы "локальный" сервер)
	out48[2] = 6; // poll
	out48[3] = (uint8_t)-20; // precision ~ -20

	// Reference ID "LOCL"
	out48[12] = 'L'; out48[13] = 'O'; out48[14] = 'C'; out48[15] = 'L';

	// Originate Timestamp = Transmit Timestamp клиента (байты 40..47 в запросе)
	memcpy(&out48[24], &clientRequest48[40], 8);

	// Receive Timestamp
	out48[32] = (rxSeconds1900 >> 24) & 0xFF;
	out48[33] = (rxSeconds1900 >> 16) & 0xFF;
	out48[34] = (rxSeconds1900 >> 8) & 0xFF;
	out48[35] = (rxSeconds1900) & 0xFF;
	out48[36] = (rxFrac >> 24) & 0xFF;
	out48[37] = (rxFrac >> 16) & 0xFF;
	out48[38] = (rxFrac >> 8) & 0xFF;
	out48[39] = (rxFrac) & 0xFF;

	// Transmit Timestamp
	out48[40] = (txSeconds1900 >> 24) & 0xFF;
	out48[41] = (txSeconds1900 >> 16) & 0xFF;
	out48[42] = (txSeconds1900 >> 8) & 0xFF;
	out48[43] = (txSeconds1900) & 0xFF;
	out48[44] = (txFrac >> 24) & 0xFF;
	out48[45] = (txFrac >> 16) & 0xFF;
	out48[46] = (txFrac >> 8) & 0xFF;
	out48[47] = (txFrac) & 0xFF;
}

// ---- RTC local <-> unix UTC helpers ----
// ВНИМАНИЕ:DS3231 у вас хранит "локальное" время целевого TZ (cfg.tzTargetHours).
// Переводим его в UTC,а NTP отдаём в UTC.

uint32_t NtpLanService::unixFromRtcLocal(const RTCTime& t, int8_t tzTargetHours)
{
	// конвертация календаря в unix (UTC) через простую epoch-функцию.
	// Считаем,что RTCTime — локальное время TZ_target.
	auto daysFromCivil = [](int y, int m, int d)->int32_t {
		y -= (m <= 2);
		const int era = (y >= 0 ? y : y - 399) / 400;
		const unsigned yoe = (unsigned)(y - era * 400);
		const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + (unsigned)d - 1;
		const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
		return (int32_t)(era * 146097 + (int)doe - 719468);
	};

	int32_t days = daysFromCivil((int)t.year, (int)t.month, (int)t.dayOfMonth);
	int64_t local = (int64_t)days * 86400LL + (int64_t)t.hour * 3600LL + (int64_t)t.minute * 60LL + (int64_t)t.second;

	int64_t utc = local - (int64_t)tzTargetHours * 3600LL;
	if (utc < 0) utc = 0;
	return (uint32_t)utc;
}

void NtpLanService::rtcLocalFromUnix(uint32_t unixUtc, int8_t tzTargetHours, RTCTime& outLocal)
{
	// unixUtc -> local target TZ -> civil
	int64_t t = (int64_t)unixUtc + (int64_t)tzTargetHours * 3600LL;

	int64_t days = t / 86400LL;
	int64_t rem = t % 86400LL;

	auto civilFromDays = [](int32_t z, int& y, int& m, int& d) {
		z += 719468;
		const int era = (z >= 0 ? z : z - 146096) / 146097;
		const unsigned doe = (unsigned)(z - era * 146097);
		const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
		y = (int)yoe + era * 400;
		const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
		const unsigned mp = (5 * doy + 2) / 153;
		d = (int)(doy - (153 * mp + 2) / 5 + 1);
		m = (int)(mp + (mp < 10 ? 3 : -9));
		y += (m <= 2);
	};

	int y, mo, d;
	civilFromDays((int32_t)days, y, mo, d);

	int hh = (int)(rem / 3600LL); rem %= 3600LL;
	int mm = (int)(rem / 60LL);
	int ss = (int)(rem % 60LL);

	outLocal.year = (uint16_t)y;
	outLocal.month = (uint8_t)mo;
	outLocal.dayOfMonth = (uint8_t)d;
	outLocal.hour = (uint8_t)hh;
	outLocal.minute = (uint8_t)mm;
	outLocal.second = (uint8_t)ss;

	// dayOfWeek для DS3231:1=Mon..7=Sun.
	// Считаем по Sakamoto (0=Sun..6=Sat) и переводим.
	static int tt[] = { 0,3,2,5,0,3,5,1,4,6,2,4 };
	int yy = y;
	if (mo < 3) yy -= 1;
	uint8_t dow0 = (uint8_t)((yy + yy / 4 - yy / 100 + yy / 400 + tt[mo - 1] + d) % 7);
	outLocal.dayOfWeek = (dow0 == 0) ? 7 : dow0;
}
