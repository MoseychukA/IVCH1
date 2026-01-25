
/*
 Программа модуля часов и измерения питающей сети.
 Дисплей ST7789 SPI 170x320
 Микроконтроллер STM32F103VGT6
 Среда программирования Arduino IDE
 Плата:Generic STM32F1 series (STMicroelectronics_GenF1)

 ВАЖНО (исправления):
 - убраны дубли объявлений planner и дубли функций daysFromCivil/civilFromDays/toEpochSeconds/fromEpochSeconds
 - исправлен порядок глобальных объектов:sim/gps создаются ДО planner
*/

#include <Arduino.h>

#include "PCF8575_simple.h"
#include <Wire.h>
#include <stdint.h>
#include <math.h>

#include <TFT_eSPI.h>
#include <SPI.h>
#include <SD.h>

#include "Menu.h"
#include "Buttons.h"

#include "SIM800TimeAsync.h"
#include "AT24C128Settings.h"
#include "SyncSourcesStore.h"
#include "TimeSyncPlanner.h"

#include "TimeFeed.h"
#include "NetFeed.h"
#include "RTCSupport.h"
#include "GPSNmeaParser.h"

#include "EthConfig.h"
#include "NtpLanService_Generic.h"
#include "LanIfStore.h"
#include "Internet2Client.h"
#include "WebUI.h"

WebUI web(80);

// -------------------- Pins / UI --------------------
#define SW1_PIN PC3
#define SW2_PIN PC2
#define SW3_PIN PC1
#define SW4_PIN PC0

// 7-seg via PCF8575
#define PCF_HOUR 0x21
#define PCF_MIN 0x22
#define PCF_SEC 0x23

// ВНИМАНИЕ:совпадает с SW4_PIN,оставляю как у вас
#define BUTTON_PIN PC0

#define INPUT_PIN PA1

// -------------------- TFT/Menu/Buttons --------------------
TFT_eSPI tft = TFT_eSPI();
Menu menu_start(&tft);

Button btnSW1(SW1_PIN);
Button btnSW2(SW2_PIN);
Button btnSW3(SW3_PIN);
Button btnSW4(SW4_PIN);

// -------------------- PCF8575 --------------------
PCF8575_simple pcfHour(PCF_HOUR);
PCF8575_simple pcfMin(PCF_MIN);
PCF8575_simple pcfSec(PCF_SEC);

const uint8_t segTable[10] = {
 0b00111111,0b00000110,0b01011011,0b01001111,0b01100110,
 0b01101101,0b01111101,0b00000111,0b01111111,0b01101111
};

// -------------------- Frequency measure --------------------
volatile uint32_t diff_sum = 0;
volatile uint16_t diff_count = 0;
volatile bool skip = false;
float frequency = 0.0f;

#ifndef dtostrf
char* dtostrf(double val, signed char width, unsigned char prec, char* sout) {
	sprintf(sout, "%*.*f", width, prec, val);
	return sout;
}
#endif

void freqInterrupt() {
	static uint32_t prev = 0;
	if (skip) { skip = false; return; }
	skip = true;

	uint32_t now = micros();
	if (prev > 0) {
		uint32_t diff = now - prev;
		diff_sum += diff;
		diff_count++;
	}
	prev = now;
}

static void setPCF(PCF8575_simple& pcf, uint8_t first, uint8_t second) {
	uint16_t word = 0;
	word |= (first & 0xFF);
	word |= ((uint16_t)second) << 8;
	pcf.write16(word);
}

static void displayFrequency(float freq)
{
	if (isnan(freq) || freq < 0 || freq > 99.9999) {
		setPCF(pcfHour, 0xFF, 0xFF);
		setPCF(pcfMin, 0xFF, 0xFF);
		setPCF(pcfSec, 0xFF, 0xFF);
		return;
	}

	if (freq > 99.9999) freq = 99.9999;
	if (freq < 0) freq = 0.0;

	uint8_t d1 = (uint8_t)(freq / 10);
	uint8_t d2 = (uint8_t)(fmod(freq, 10));
	uint16_t fract = (uint16_t)(fmod(freq, 1) * 10000);

	uint8_t f1 = (fract / 1000) % 10;
	uint8_t f2 = (fract / 100) % 10;
	uint8_t f3 = (fract / 10) % 10;
	uint8_t f4 = (fract / 1) % 10;

	uint8_t dig[6];
	dig[0] = ~segTable[d1];
	dig[1] = ~segTable[d2] & ~(0b10000000); // точка
	dig[2] = ~segTable[f1];
	dig[3] = ~segTable[f2];
	dig[4] = ~segTable[f3];
	dig[5] = ~segTable[f4];

	setPCF(pcfHour, dig[0], dig[1]);
	setPCF(pcfMin, dig[2], dig[3]);
	setPCF(pcfSec, dig[4], dig[5]);
}

static void displayTime(uint8_t hours, uint8_t minutes, uint8_t seconds) {
	setPCF(pcfHour, ~segTable[hours / 10], ~segTable[hours % 10]);
	setPCF(pcfMin, ~segTable[minutes / 10], ~segTable[minutes % 10]);
	setPCF(pcfSec, ~segTable[seconds / 10], ~segTable[seconds % 10]);
}

// -------------------- Calendar/Epoch helpers (ОДНА копия) --------------------
static int32_t daysFromCivil(int y, int m, int d) {
	y -= (m <= 2);
	const int era = (y >= 0 ? y : y - 399) / 400;
	const unsigned yoe = (unsigned)(y - era * 400);
	const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + (unsigned)d - 1;
	const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
	return (int32_t)(era * 146097 + (int)doe - 719468);
}

static void civilFromDays(int32_t z, int& y, int& m, int& d) {
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
}

static int64_t toEpochSeconds(int y, int mo, int d, int h, int mi, int s) {
	int32_t days = daysFromCivil(y, mo, d);
	return (int64_t)days * 86400LL + (int64_t)h * 3600LL + (int64_t)mi * 60LL + (int64_t)s;
}

static void fromEpochSeconds(int64_t t, int& y, int& mo, int& d, int& h, int& mi, int& s) {
	int64_t days = t / 86400LL;
	int64_t rem = t % 86400LL;
	if (rem < 0) { rem += 86400LL; days -= 1; }
	civilFromDays((int32_t)days, y, mo, d);
	h = (int)(rem / 3600LL); rem %= 3600LL;
	mi = (int)(rem / 60LL);
	s = (int)(rem % 60LL);
}

// local(net) -> UTC по tzq_net
static void localToUTC(int& y, int& mo, int& d, int& h, int& mi, int& s, int tzQuarterHours) {
	const int offsetMin = tzQuarterHours * 15;
	int64_t epochLocal = toEpochSeconds(y, mo, d, h, mi, s);
	int64_t epochUTC = epochLocal - (int64_t)offsetMin * 60LL;
	fromEpochSeconds(epochUTC, y, mo, d, h, mi, s);
}

// UTC -> local(target) по tzq_target
static void utcToLocal(int& y, int& mo, int& d, int& h, int& mi, int& s, int tzQuarterHours) {
	const int offsetMin = tzQuarterHours * 15;
	int64_t epochUTC = toEpochSeconds(y, mo, d, h, mi, s);
	int64_t epochLocal = epochUTC + (int64_t)offsetMin * 60LL;
	fromEpochSeconds(epochLocal, y, mo, d, h, mi, s);
}

// DOW helpers
static uint8_t dow0_sun(int y, int m, int d) {
	static int t[] = { 0,3,2,5,0,3,5,1,4,6,2,4 };
	if (m < 3) y -= 1;
	return (uint8_t)((y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7);
}
static uint8_t ds3231DowFromDow0(uint8_t dow0) { return (dow0 == 0) ? 7 : dow0; }

// DS3231 local -> unix UTC by tzTargetHours
static uint32_t unixUtcFromRtcLocal(const RTCTime& t, int8_t tzTargetHours) {
	int64_t local = toEpochSeconds(t.year, t.month, t.dayOfMonth, t.hour, t.minute, t.second);
	int64_t utc = local - (int64_t)tzTargetHours * 3600LL;
	if (utc < 0) utc = 0;
	return (uint32_t)utc;
}

// unix UTC -> DS3231 local by tzTargetHours
static void rtcLocalFromUnixUtc(uint32_t unixUtc, int8_t tzTargetHours, RTCTime& out) {
	int64_t local = (int64_t)unixUtc + (int64_t)tzTargetHours * 3600LL;
	int y, mo, d, h, mi, s;
	fromEpochSeconds(local, y, mo, d, h, mi, s);

	out.year = (uint16_t)y; out.month = (uint8_t)mo; out.dayOfMonth = (uint8_t)d;
	out.hour = (uint8_t)h; out.minute = (uint8_t)mi; out.second = (uint8_t)s;

	// dayOfWeek как в вашем RTCSupport:1..7 (Mon..Sun)
	static int tt[] = { 0,3,2,5,0,3,5,1,4,6,2,4 };
	int yy = y; if (mo < 3) yy -= 1;
	uint8_t dow0 = (uint8_t)((yy + yy / 4 - yy / 100 + yy / 400 + tt[mo - 1] + d) % 7); // 0=Sun
	out.dayOfWeek = (dow0 == 0) ? 7 : dow0;
}

// -------------------- GPS --------------------
/*
 PA3 = USART2_RX (GPS TX -> PA3)
 PA2 = USART2_TX (опционально)
*/
HardwareSerial gps_Serial(PA3, PA2);
GPSNmeaParser gps(gps_Serial);

// -------------------- SIM800 UART4 --------------------
HardwareSerial MODEM(PC11, PC10); // RX,TX (UART4:PC11/PC10)
static const uint8_t PWRKEY_PIN = PE0; // через NPN на землю
static const bool PWRKEY_ACTIVE_HIGH = true; // "нажать"=HIGH
SIM800TimeAsync sim(MODEM, PWRKEY_PIN, PWRKEY_ACTIVE_HIGH);

// -------------------- I2C EEPROM stores / Net services (глобально,для extern в Menu.cpp) --------------------
AT24C128Settings ee(Wire, 0x50);
AT24C128Settings::Config cfg;

SyncSourcesStore syncStore(Wire, 0x50);
SyncSourcesStore::Data syncData;

RealtimeClock rtc;

NtpLanService_Generic ntpLan(rtc, cfg);

LanIfStore lanStore(0x50, Wire);

Internet2Client internet2(Wire, 0x42);

// Планировщик (ВАЖНО:после sim/gps/rtc/cfg/syncData/internet2)
TimeSyncPlanner planner(sim, gps, rtc, cfg, syncData, &internet2);

// -------------------- NTP lists/periods (shared with Menu order) --------------------
static const IPAddress kNtpUpstreamIp[5] = {
 IPAddress(162,159,200,123),
 IPAddress(162,159,200,1),
 IPAddress(129,6,15,28),
 IPAddress(132,163,96,1),
 IPAddress(216,239,35,0)
};

static uint32_t periodMsFromIdx(uint8_t idx)
{
	static const uint32_t ms[6] = {
	60000UL,// 1 мин
	600000UL,// 10 мин
	1800000UL,// 30 мин
	3600000UL,// 1 час
	21600000UL,// 6 часов
	43200000UL // 12 часов
	};
	return ms[idx % 6];
}

// -------------------- Config -> SIM --------------------
static void applyCfgToSimNoConflict(const AT24C128Settings::Config& c)
{
	SIM800TimeAsync::NtpConfig ntp;
	ntp.apn = c.apn;
	ntp.user = c.user;
	ntp.pass = c.pass;
	ntp.server = c.server;
	ntp.tzHours = c.tzNtpHours; // для AT+CNTP (обычно 0)
	ntp.enableFallback = c.enableFallback;

	sim.setNtpConfig(ntp);

	// Чтобы не конфликтовать с planner:внутренний период "далеко"
	sim.setPeriodMs(86400000UL); // 1 сутки
}

// -------------------- INTERNET apply (extern used by Menu.cpp) --------------------
void applyInternet1FromStore()
{
	LanIfStore::IfConfig c;
	lanStore.load(LanIfStore::IF1, c, false);

	ntpLan.applyLanConfig(
		c.dhcp != 0,
		IPAddress(c.ip[0], c.ip[1], c.ip[2], c.ip[3]),
		IPAddress(c.dns[0], c.dns[1], c.dns[2], c.dns[3]),
		IPAddress(c.gw[0], c.gw[1], c.gw[2], c.gw[3]),
		IPAddress(c.mask[0], c.mask[1], c.mask[2], c.mask[3])
	);

	ntpLan.applyUpstream(
		kNtpUpstreamIp[c.ntpIdx % 5],
		periodMsFromIdx(c.periodIdx)
	);

	planner.onSettingsChanged();
}

void applyInternet2FromStore()
{
	LanIfStore::IfConfig c;
	lanStore.load(LanIfStore::IF2, c, false);

	const bool dhcp = (c.dhcp != 0);

	IPAddress up = kNtpUpstreamIp[c.ntpIdx % 5];
	uint32_t per = periodMsFromIdx(c.periodIdx);

	// ВАЖНО:
	// если DHCP=ON,реальные IP/MASK/GW/DNS модуль получит сам от DHCP.
	// Поэтому сюда отправляем нули (или можно отправить сохранённые как fallback,
	// но только если прошивка INTERNET2 гарантированно их игнорирует при DHCP=ON).
	IPAddress ip = dhcp ? IPAddress(0, 0, 0, 0) : IPAddress(c.ip[0], c.ip[1], c.ip[2], c.ip[3]);
	IPAddress mask = dhcp ? IPAddress(0, 0, 0, 0) : IPAddress(c.mask[0], c.mask[1], c.mask[2], c.mask[3]);
	IPAddress gw = dhcp ? IPAddress(0, 0, 0, 0) : IPAddress(c.gw[0], c.gw[1], c.gw[2], c.gw[3]);
	IPAddress dns = dhcp ? IPAddress(0, 0, 0, 0) : IPAddress(c.dns[0], c.dns[1], c.dns[2], c.dns[3]);

	internet2.applyNetCfg(
		dhcp,
		ip,
		mask,
		gw,
		dns,
		up,
		per
	);

	// сразу дать модулю актуальное время (UTC)
	RTCTime nowLocal = rtc.getTime();
	uint32_t unixUtc = unixUtcFromRtcLocal(nowLocal, cfg.tzTargetHours);
	uint16_t ms = (uint16_t)(millis() % 500);
	internet2.setTimeUnixUtc(unixUtc, ms);
}

// -------------------- IP feed update (HOME) --------------------
static void updateIp1Feed1Hz()
{
	static uint32_t t0 = 0;
	uint32_t now = millis();
	if (now - t0 < 1000) return;
	t0 = now;

	IPAddress ip = ntpLan.localIP();
	char buf[16];
	snprintf(buf, sizeof(buf), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);

	if (strcmp(gIp1Str, buf) != 0) {
		strncpy(gIp1Str, buf, sizeof(gIp1Str) - 1);
		gIp1Str[15] = 0;
		gIpUpdated = true;
	}
}

static void updateIp2Feed1Hz()
{
	static uint32_t t0 = 0;
	uint32_t now = millis();
	if (now - t0 < 1000) return;
	t0 = now;

	Internet2Client::Status st;
	if (internet2.readStatus(st)) {
		char buf[16];
		snprintf(buf, sizeof(buf), "%u.%u.%u.%u", st.ip[0], st.ip[1], st.ip[2], st.ip[3]);
		if (strcmp(gIp2Str, buf) != 0) {
			strncpy(gIp2Str, buf, sizeof(gIp2Str) - 1);
			gIp2Str[15] = 0;
			gIpUpdated = true;
		}
	}
}

//================================================================================
static void tickInternet2TimePushEdge()
{
	static uint8_t lastSec = 255;
	static uint32_t lastEdgeMs = 0;
	static uint32_t unixUtcAtEdge = 0;

	RTCTime t = rtc.getTime(); // локальное время
	if (t.second != lastSec) {
		lastSec = t.second;

		// считаем UTC unix по локальному времени и cfg.tzTargetHours
		unixUtcAtEdge = unixUtcFromRtcLocal(t, cfg.tzTargetHours);

		// отметка момента обнаружения границы секунды
		lastEdgeMs = millis();

		// отправляем "ровно начало секунды"
		internet2.setTimeUnixUtc(unixUtcAtEdge, 0);
		return;
	}

	// (опционально) подстраховка:если долго не было отправки,можно обновить ms-фазу
	// например раз в 5 секунд:
	static uint32_t tGuard = 0;
	if (millis() - tGuard > 5000) {
		tGuard = millis();
		if (unixUtcAtEdge != 0) {
			uint32_t d = millis() - lastEdgeMs;
			uint32_t unixUtcNow = unixUtcAtEdge + d / 1000UL;
			uint16_t msNow = (uint16_t)(d % 1000UL);
			internet2.setTimeUnixUtc(unixUtcNow, msNow);
		}
	}
}

//================================================================================


void setup()
{
	// отпустить PWRKEY
	pinMode(PE0, OUTPUT);
	digitalWrite(PE0, LOW);

	Serial.begin(115200);

	// версия
	String ver_soft = __FILE__;
	int val_srt = ver_soft.lastIndexOf('\\');
	if (val_srt >= 0) ver_soft.remove(0, val_srt + 1);
	val_srt = ver_soft.lastIndexOf('.');
	if (val_srt >= 0) ver_soft.remove(val_srt);

	menu_start.setup(ver_soft);

	delay(1300);
	Serial.println("Start system");
	Serial.println(ver_soft);

	// I2C
	Wire.begin();
	Wire.setClock(100000);

	internet2.begin();

	// Stores (SyncSourcesStore:strict,ONLY new BASE)
	syncStore.defaults(syncData);

	if (!syncStore.begin()) {
		Serial.println(F("SyncSourcesStore:EEPROM(0x50) not found (I2C NACK)"));
	}
	else {
		if (!syncStore.load(syncData)) {
			Serial.println(F("SyncSourcesStore:no valid data in NEW area,using defaults"));
			// опционально:один раз зафиксировать дефолты в новой области
			(void)syncStore.save(syncData);
		}
	}

	rtc.begin();

	// EEPROM cfg
	const bool FORCE_EEPROM_INIT = false;
	if (ee.begin()) {
		if (FORCE_EEPROM_INIT) {
			Serial.println(F("FORCE EEPROM INIT"));
			AT24C128Settings::defaults(cfg);
			ee.save(cfg, true);
		}
		ee.load(cfg, false);
	}
	else {
		Serial.println(F("EEPROM AT24C128 not found"));
		AT24C128Settings::defaults(cfg);
	}

	// LAN (Internet1)
	NtpLanService_Generic::NetConfig net;
	net.macIndex = 0;
	net.useDhcp = true;
	net.upstreamIp = IPAddress(162, 159, 200, 123);
	net.syncPeriodMs = 3600000UL;

	ntpLan.begin(net);
	ntpLan.forceSyncNow();

	lanStore.begin();

	// 7-seg + freq input
	pcfHour.begin();
	pcfMin.begin();
	pcfSec.begin();

	pinMode(INPUT_PIN, INPUT);
	attachInterrupt(INPUT_PIN, freqInterrupt, RISING);

	// GPS
	gps_Serial.begin(9600);
	gps.begin(9600); // если в вашем GPSNmeaParser begin не нужен — удалите эту строку

	// SIM
	sim.begin(9600);
	//sim.setDebug(&Serial,false); //
	sim.setHealthPingMs(60000);
	sim.setMaxRecoveryAttempts(5);

	// Если у вас реализован статус-опрос в SIM800TimeAsync — оставьте.
	// Иначе закомментируйте следующую строку.
	//!!sim.setStatusPollMs(5000);

	applyCfgToSimNoConflict(cfg);
	sim.start();

	// Planner
	planner.begin();
	planner.onSettingsChanged();
	planner.triggerImmediate();

	// Применить сохранённые настройки INTERNET1/2 в реальные каналы
	applyInternet1FromStore();
	applyInternet2FromStore();

	// HOME
	menu_start.drawStartPage();
	web.begin();
}

void loop()
{
	// GPS parsing
	gps.tick();

	// Internet1 tick
	ntpLan.tick();

	// IP feeds (for HOME)
	updateIp1Feed1Hz();
	updateIp2Feed1Hz();

	if (gIpUpdated) { gIpUpdated = false; menu_start.invalidateHome(); }

	// Periodically push RTC time to Internet2 (UTC),so it can serve LAN clients
	static uint32_t tSend = 0;
	if (millis() - tSend > 2000)
	{
		tSend = millis();
		RTCTime nowLocal = rtc.getTime();
		uint32_t unixUtc = unixUtcFromRtcLocal(nowLocal, cfg.tzTargetHours);
		internet2.setTimeUnixUtc(unixUtc, 0);
	}

	tickInternet2TimePushEdge();
	// SIM tick
	sim.tick();

	// Planner tick (GPS > NET2 > NET(sim NTP) > GSM(sim))
	planner.tick();

	// --- Frequency calc (1 Hz) ---
	static uint32_t lastFreqUpdate = 0;
	static float lastMeasuredFreq = 0.0f;

	if (millis() - lastFreqUpdate > 1000) {
		noInterrupts();
		uint16_t n = diff_count;
		uint32_t sum = diff_sum;
		diff_count = 0;
		diff_sum = 0;
		interrupts();

		if (n > 0) {
			float Tavg = (float)sum / n;
			frequency = (1e6f / Tavg);
			lastMeasuredFreq = frequency;
		}
		lastFreqUpdate = millis();
	}

	// --- Button “frequency/time” select ---
	static bool lastBtnState = HIGH;
	static uint32_t lastDebounce = 0;
	bool btnState = digitalRead(BUTTON_PIN);

	if (btnState != lastBtnState) {
		lastDebounce = millis();
		lastBtnState = btnState;
	}
	bool btnStableState = lastBtnState;
	if ((millis() - lastDebounce) > 25) btnStableState = btnState;

	static uint8_t prev_hour = 255, prev_min = 255, prev_sec = 255;
	static uint32_t prevFreqInt = 0, prevFreqFrac = 0;

	if (btnStableState == LOW)
	{
		uint32_t curFreqInt = (uint32_t)lastMeasuredFreq;
		uint32_t curFreqFrac = (uint32_t)((lastMeasuredFreq - curFreqInt) * 10000.0);
		if (curFreqInt != prevFreqInt || curFreqFrac != prevFreqFrac) {
			displayFrequency(lastMeasuredFreq);
			prevFreqInt = curFreqInt;
			prevFreqFrac = curFreqFrac;
		}
	}
	else
	{
		RTCTime now = rtc.getTime();
		if (now.hour != prev_hour || now.minute != prev_min || now.second != prev_sec) {
			displayTime(now.hour, now.minute, now.second);
			prev_hour = now.hour;
			prev_min = now.minute;
			prev_sec = now.second;

			// обновляем строки для HOME из RTC
			snprintf(gDateStr, sizeof(gDateStr), "%02u.%02u.%04u",
				(unsigned)now.dayOfMonth, (unsigned)now.month, (unsigned)now.year);
			snprintf(gTimeStr, sizeof(gTimeStr), "%02u:%02u:%02u",
				(unsigned)now.hour, (unsigned)now.minute, (unsigned)now.second);
			gTimeUpdated = true;

			menu_start.invalidateHome();
		}
	}

	// --- Buttons -> Menu (EDGE) ---
	btnSW1.update();
	if (btnSW1.getEdge() == EDGE_PRESSED) menu_start.onButtonPressed(BTN_SW1);
	if (btnSW1.getEdge() == EDGE_RELEASED) menu_start.onButtonReleased(BTN_SW1);

	btnSW2.update();
	if (btnSW2.getEdge() == EDGE_PRESSED) menu_start.onButtonPressed(BTN_SW2);
	if (btnSW2.getEdge() == EDGE_RELEASED) menu_start.onButtonReleased(BTN_SW2);

	btnSW3.update();
	if (btnSW3.getEdge() == EDGE_PRESSED) menu_start.onButtonPressed(BTN_SW3);
	if (btnSW3.getEdge() == EDGE_RELEASED) menu_start.onButtonReleased(BTN_SW3);

	btnSW4.update();
	if (btnSW4.getEdge() == EDGE_PRESSED) menu_start.onButtonPressed(BTN_SW4);
	if (btnSW4.getEdge() == EDGE_RELEASED) menu_start.onButtonReleased(BTN_SW4);

	// --- Menu update (HOME refresh every 500ms inside Menu.cpp) ---
	menu_start.update();

	// --- Time received from SIM -> set RTC (to cfg.tzTargetHours local) ---
	if (sim.hasNewTime()) {
		String raw = sim.lastCCLKRaw();

		int y, mo, d, h, mi, s, tzq_net;
		if (SIM800TimeAsync::parseCCLK(raw, y, mo, d, h, mi, s, tzq_net)) {

			// 1) local(net) -> UTC
			localToUTC(y, mo, d, h, mi, s, tzq_net);

			// 2) UTC -> local(target TZ from cfg)
			int tzq_target = (int)cfg.tzTargetHours * 4;
			utcToLocal(y, mo, d, h, mi, s, tzq_target);

			uint8_t dsDow = ds3231DowFromDow0(dow0_sun(y, mo, d));

			rtc.setTime((uint8_t)s, (uint8_t)mi, (uint8_t)h,
				dsDow, (uint8_t)d, (uint8_t)mo, (uint16_t)y);

			snprintf(gDateStr, sizeof(gDateStr), "%02d.%02d.%04d", d, mo, y);
			snprintf(gTimeStr, sizeof(gTimeStr), "%02d:%02d:%02d", h, mi, s);
			gTimeUpdated = true;
			menu_start.invalidateHome();

			planner.onTimeUpdated();
		}
	}

	// --- Net status cache for display (если у вас есть такой помощник в NetFeed) ---
	NetFeed_UpdateFromSim(sim);
	if (gNetUpdated) {
		gNetUpdated = false;
		menu_start.invalidateHome();
	}

	web.tick();
}


