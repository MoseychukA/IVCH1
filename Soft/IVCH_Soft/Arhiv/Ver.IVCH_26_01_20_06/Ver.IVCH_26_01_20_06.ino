/*
 Программа модуля часов и измерения питающей сети.
 Дисплей ST7789 SPI 170x320
 Микроконтроллер STM32F103VGT6
 Среда программирования Arduino IDE
 Generic STM32F1 series
*/

#include "Arduino.h"

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
#include <Ethernet.h>
#include "NtpLanService.h"
#include <utility/w5100.h> // в Ethernet/Ethernet2 обычно есть

static const char* wizLinkStr()
{
#if defined(W5500)
	// W5500 PHYCFGR bit0 = LNK (1=up)
	uint8_t phy = W5100.readPHYCFGR();
	return (phy & 0x01) ? "ON" : "OFF";
#else
	return "N/A";
#endif
}

// -------------------- I2C EEPROM stores --------------------
// ГЛОБАЛЬНО (не static),чтобы Menu.cpp мог сделать extern
AT24C128Settings ee(Wire, 0x50);
AT24C128Settings::Config cfg;

SyncSourcesStore syncStore(Wire, 0x50);
SyncSourcesStore::Data syncData;
RealtimeClock rtc;
NtpLanService ntpLan(rtc, cfg);
/*
 PA3 = USART2_RX (сюда подключить GPS TX)
 PA2 = USART2_TX (обычно не нужен; если надо конфигурировать GPS — подключить GPS RX)
*/

// USART2 обычно Serial2 (PA3 RX)
HardwareSerial gps_Serial(PA3,PA2);

GPSNmeaParser gps(gps_Serial);


// -------------------- SIM800 UART --------------------
HardwareSerial MODEM(PC11, PC10); // RX,TX (UART4 типично PC11/PC10)

static const uint8_t PWRKEY_PIN = PE0; // через NPN на землю
static const bool PWRKEY_ACTIVE_HIGH = true; // "нажать"=HIGH

SIM800TimeAsync sim(MODEM, PWRKEY_PIN, PWRKEY_ACTIVE_HIGH);

// -------------------- Planner (GPS > Internet > GSM) --------------------
//TimeSyncPlanner planner(sim, ee, cfg, syncStore, syncData);
TimeSyncPlanner planner(sim, gps, rtc, cfg, syncData);
// -------------------- RTC --------------------

// --- DOW:0=Sunday..6=Saturday
static uint8_t dow0_sun(int y, int m, int d) {
	static int t[] = { 0,3,2,5,0,3,5,1,4,6,2,4 };
	if (m < 3) y -= 1;
	return (uint8_t)((y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7);
}
// DS3231/RTCSupport:1=Mon..6=Sat,7=Sun
static uint8_t ds3231DowFromDow0(uint8_t dow0) {
	return (dow0 == 0) ? 7 : dow0;
}

// ---------- Calendar <-> epoch helpers (для TZ пересчёта) ----------
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

// -------------------- Pins / UI --------------------
#define SW1_PIN PC3
#define SW2_PIN PC2
#define SW3_PIN PC1
#define SW4_PIN PC0

TFT_eSPI tft = TFT_eSPI();
Menu menu_start(&tft);

Button btnSW1(SW1_PIN);
Button btnSW2(SW2_PIN);
Button btnSW3(SW3_PIN);
Button btnSW4(SW4_PIN);

// 7-seg via PCF8575
#define PCF_HOUR 0x21
#define PCF_MIN 0x22
#define PCF_SEC 0x23
#define BUTTON_PIN PC0 // ВНИМАНИЕ:у вас совпадает с SW4_PIN — оставляю как было

PCF8575_simple pcfHour(PCF_HOUR);
PCF8575_simple pcfMin(PCF_MIN);
PCF8575_simple pcfSec(PCF_SEC);

const uint8_t segTable[10] = {
 0b00111111,0b00000110,0b01011011,0b01001111,0b01100110,
 0b01101101,0b01111101,0b00000111,0b01111111,0b01101111
};

#define INPUT_PIN PA1

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

// --- Делитель частоты на 2 ---
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

	// ВАЖНО:чтобы не было конфликта с планировщиком,задвигаем внутренний time-poll далеко.
	// Реальные старты времени выполняет planner.tick() через requestTimeNow()/requestNtpNow().
	sim.setPeriodMs(86400000UL); // 1 сутки
}

//=============================== GPS test ===================================================

static void gpsDebugPrint1Hz()
{
	static uint32_t t0 = 0;
	uint32_t now = millis();
	if (now - t0 < 1000) return;
	t0 = now;

	// “свежесть” данных
	uint32_t ageR = (gGpsLastRmcMs == 0) ? 0xFFFFFFFFUL : (now - gGpsLastRmcMs);
	uint32_t ageG = (gGpsLastGgaMs == 0) ? 0xFFFFFFFFUL : (now - gGpsLastGgaMs);
	uint32_t ageV = (gGpsLastGsvMs == 0) ? 0xFFFFFFFFUL : (now - gGpsLastGsvMs);

	Serial.print(F("[GPS] fix=")); Serial.print(gGpsFix ? 1 : 0);
	Serial.print(F(" used/view=")); Serial.print(gGpsSatsUsed);
	Serial.print(F("/")); Serial.print(gGpsSatsView);

	Serial.print(F(" SNR(avg/max)=")); Serial.print(gGpsSnrAvg);
	Serial.print(F("/")); Serial.print(gGpsSnrMax);

	Serial.print(F(" HDOP="));
	Serial.print((double)gGpsHdop_x100 / 100.0, 2);

	Serial.print(F(" age(ms) RMC=")); Serial.print(ageR);
	Serial.print(F(" GGA=")); Serial.print(ageG);
	Serial.print(F(" GSV=")); Serial.println(ageV);
}

static void gpsUtcPrint1Hz(GPSNmeaParser& gps)
{
	static uint32_t t0 = 0;
	uint32_t now = millis();
	if (now - t0 < 1000) return;
	t0 = now;

	int y, mo, d, h, mi, s;
	if (gps.getUtc(y, mo, d, h, mi, s)) {
		Serial.print(F("[GPS UTC] "));
		Serial.print(y); Serial.print("-");
		if (mo < 10) Serial.print("0"); Serial.print(mo); Serial.print("-");
		if (d < 10) Serial.print("0"); Serial.print(d); Serial.print(" ");
		if (h < 10) Serial.print("0"); Serial.print(h); Serial.print(":");
		if (mi < 10) Serial.print("0"); Serial.print(mi); Serial.print(":");
		if (s < 10) Serial.print("0"); Serial.println(s);
	}
	else {
		Serial.println(F("[GPS UTC] (no RMC yet)"));
	}
}

//====================================================================================

static void printEthernetNtpStatus1Hz(NtpLanService& ntpLan)
{
	static uint32_t t0 = 0;
	uint32_t now = millis();
	if (now - t0 < 1000) return;
	t0 = now;

	// IP
	IPAddress ip = Ethernet.localIP();
	Serial.print(F("[ETH] IP="));
	Serial.print(ip[0]); Serial.print('.');
	Serial.print(ip[1]); Serial.print('.');
	Serial.print(ip[2]); Serial.print('.');
	Serial.print(ip[3]);

	// LINK (если доступно)
	Serial.print(F(" LINK="));
	Serial.print(wizLinkStr());
#if defined(ETHERNET_LINK_STATUS)
	auto ls = Ethernet.linkStatus();
	if (ls == LinkON) Serial.print(F("ON"));
	else if (ls == LinkOFF) Serial.print(F("OFF"));
	else Serial.print(F("UNKNOWN"));
#else
	Serial.print(F("N/A"));
#endif

	// Last sync
	Serial.print(F(" NTPsync="));
	Serial.print(ntpLan.lastSyncOk() ? F("OK") : F("FAIL"));
	//Serial.print(F(" lastMs="));
	//Serial.println(ntpLan.lastSyncMs());
	Serial.print(F(" attemptMs="));
	Serial.print(ntpLan.lastAttemptMs());

	Serial.print(F(" successMs="));
	uint32_t s = ntpLan.lastSuccessMs();
	if (s == 0) Serial.print(F("never"));
	else Serial.print(s);
	Serial.println();
}



void setup()
{
	// отпустить PWRKEY
	pinMode(PE0, OUTPUT);
	digitalWrite(PE0, LOW);

	Serial.begin(115200);

	// Версия
	String ver_soft = __FILE__;
	int val_srt = ver_soft.lastIndexOf('\\');
	ver_soft.remove(0, val_srt + 1);
	val_srt = ver_soft.lastIndexOf('.');
	ver_soft.remove(val_srt);

	menu_start.setup(ver_soft);

	gps_Serial.begin(9600); // USART2
	gps.begin(9600);
	

	delay(1500);
	Serial.println("Start system");
	Serial.println(ver_soft);

	Wire.begin();

	// Источники синхронизации (EEPROM block 0x0200)
	syncStore.begin();
	syncStore.loadLoose(syncData);

	// RTC
	rtc.begin();

	// EEPROM cfg (apn/user/pass/server/tz/period etc.)
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

	//======================= Интернет =======================
	NtpLanService::NetConfig net;

	// MAC обязательно уникальный
	net.mac[0] = 0x02; net.mac[1] = 0x12; net.mac[2] = 0x34; net.mac[3] = 0x56; net.mac[4] = 0x78; net.mac[5] = 0x9A;

	// Статический IP вашей LAN (пример)
	net.ip = IPAddress(192, 168, 75, 55);
	net.gw = IPAddress(192, 168, 75, 1);
	net.mask = IPAddress(255, 255, 255, 0);
	net.dns = IPAddress(192, 168, 75, 1);

	// Внешний NTP (IP чтобы без DNS)
	//net.upstreamIp = IPAddress(129, 6, 15, 28); //  
	net.upstreamIp = IPAddress(217,162,232,173); //  
	net.syncPeriodMs = 3600000UL; // 1 час

	// CS = PA4
	bool ok = ntpLan.begin(net, PA4);
	Serial.print("NTP LAN service:");
	Serial.println(ok ? "OK" : "FAIL");

	// Немедленно синхронизироваться из интернета при старте
	ntpLan.forceSyncNow();

	//=======================================================




	// PCF8575 + freq input
	pcfHour.begin();
	pcfMin.begin();
	pcfSec.begin();

	pinMode(INPUT_PIN, INPUT);
	attachInterrupt(INPUT_PIN, freqInterrupt, RISING);

	// SIM
	sim.begin(9600);
	sim.setHealthPingMs(60000);
	sim.setStatusPollMs(5000);
	sim.setMaxRecoveryAttempts(5);

	applyCfgToSimNoConflict(cfg);

	sim.start();

	// Планировщик
	planner.begin();
	planner.onSettingsChanged();
	planner.triggerImmediate();

	// HOME
	menu_start.drawStartPage();
}

void loop()
{
	gps.tick();
	//gpsDebugPrint1Hz(); // отладка
	//gpsUtcPrint1Hz(gps);// отладка
	ntpLan.tick();
	printEthernetNtpStatus1Hz(ntpLan);// отладка
	sim.tick();

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

	// --- Button “frequency/time” select (как было) ---
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

	// --- Time received from SIM -> set RTC ---
	if (sim.hasNewTime()) {
		String raw = sim.lastCCLKRaw();

		int y, mo, d, h, mi, s, tzq_net;
		if (SIM800TimeAsync::parseCCLK(raw, y, mo, d, h, mi, s, tzq_net)) {

			// 1) local(net) -> UTC
			localToUTC(y, mo, d, h, mi, s, tzq_net);

			// 2) UTC -> local(target TZ from cfg)
			int tzq_target = (int)cfg.tzTargetHours * 4;
			utcToLocal(y, mo, d, h, mi, s, tzq_target);

			uint8_t dow0 = dow0_sun(y, mo, d);
			uint8_t dsDow = ds3231DowFromDow0(dow0);

			rtc.setTime((uint8_t)s, (uint8_t)mi, (uint8_t)h, dsDow, (uint8_t)d, (uint8_t)mo, (uint16_t)y);

			// сразу обновим строки для HOME
			snprintf(gDateStr, sizeof(gDateStr), "%02d.%02d.%04d", d, mo, y);
			snprintf(gTimeStr, sizeof(gTimeStr), "%02d:%02d:%02d", h, mi, s);
			gTimeUpdated = true;
			menu_start.invalidateHome();

			planner.onTimeUpdated();
		}
	}

	// --- Net status cache for display ---
	NetFeed_UpdateFromSim(sim);
	// Menu HOME сам берёт gNetRegistered/gSignalBars и перерисует при invalidateHome()
	if (gNetUpdated) {
		gNetUpdated = false;
		menu_start.invalidateHome();
	}
}