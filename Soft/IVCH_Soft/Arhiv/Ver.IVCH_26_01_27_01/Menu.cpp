#include "Menu.h"
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <new>
#include <string.h>
#include <Wire.h>
#include "NetHelpers.h"

#include "TimeFeed.h"
#include "NetFeed.h"
#include "AT24C128Settings.h"
#include "SyncSourcesStore.h"
#include "TimeSyncPlanner.h"
#include "LanIfStore.h"

#include "NtpLanService_Generic.h"
#include "Internet2Client.h"

// --- externs from .ino ---
extern AT24C128Settings ee;
extern AT24C128Settings::Config cfg;

extern SyncSourcesStore syncStore;
extern SyncSourcesStore::Data syncData;

extern TimeSyncPlanner planner;

extern LanIfStore lanStore;
extern void applyInternet1FromStore();
extern void applyInternet2FromStore();

extern NtpLanService_Generic ntpLan;
extern Internet2Client internet2;

// LOG mode (from .ino):false=full log,true=time-only
extern bool gLogTimeOnly;

// ---- last TIME OK stamps (millis),written by TimeSyncPlanner/.ino ----
extern volatile uint32_t gTimeOkMsGps;
extern volatile uint32_t gTimeOkMsNet1;
extern volatile uint32_t gTimeOkMsNet2;
extern volatile uint32_t gTimeOkMsGsm; // set in .ino after rtc.setTime() from SIM800 (only for ext request)

// GPS globals
extern uint8_t gGpsSatsUsed;
extern uint8_t gGpsSatsView;
extern bool gGpsFix;
extern uint16_t gGpsHdop_x100;
extern uint32_t gGpsLastRmcMs;
extern uint32_t gGpsLastGgaMs;
extern uint32_t gGpsLastGsvMs;

// Net/GSM globals (declared in NetFeed.h)
extern bool gNetRegistered;
extern uint8_t gSignalBars;
extern int8_t gCsqRssi; // 0..31,99=unknown
extern int16_t gRssiDbm; // dBm

// gDateStr,gIp1Str,gIp2Str are declared in included headers:
// TimeFeed.h:extern char gDateStr[11]; // "dd.mm.yyyy"
// NetFeed.h :extern char gIp1Str[16]; // "192.168.75.231"
// NetFeed.h :extern char gIp2Str[16]; // "0.0.0.0"

// Fonts
#include "fonts\zTimesNRItalic18.h"
#include "fonts\zTimesNRItalic24.h"
#include "fonts\zTimesNRItalic28.h"
#include "fonts\zTimesNRItalic36.h"
#include "fonts\zCalibri36.h"
#include "fonts\zTimesNR28.h"
#include "fonts\zTimesNR14.h"
#include "fonts\zTimesNR18.h"
#include "fonts\zTimesNR24.h"

#define AA_FONT_CALI zCalibri36
#define AA_FONT_TIME18I zTimesNRItalic18
#define AA_FONT_TIME24I zTimesNRItalic24
#define AA_FONT_TIME28I zTimesNRItalic28
#define AA_FONT_TIME36I zTimesNRItalic36
#define AA_FONT_TIME28 zTimesNR28
#define AA_FONT_TIME18 zTimesNR18
#define AA_FONT_TIME24 zTimesNR24

// ---------------- HOME layout for 320x170 ----------------
static const int Y_HOME = 10;

// HOME rows
static const int Y_L1 = 32; // GPS + [T]/[X]
static const int Y_L2 = 52; // IP1 + DHCP + [T]/[X]
static const int Y_L3 = 72; // IP2 + DHCP + [T]/[X]
static const int Y_L4 = 94; // GSM + [T]/[X]
static const int Y_FOOT = 140; // footer:date left + version right

// --- NTP upstream list (only IP,idx 0..4) ---
static const char* kNtpIpStr[5] = {
	"162.159.200.123",
	"162.159.200.1",
	"129.6.15.28",
	"132.163.96.1",
	"216.239.35.0"
};

// --- Period names (idx 0..5) ---
static const char* kPeriodsName[6] = { "1 мин","10 мин","30 мин","1 час","6 часов","12 часов" };

// --- GSM provider names (only 3) ---
static const char* kGsmProviders[3] = { "МТС","МЕГАФОН","БИЛАЙН" };

// Table for Page1 MODE_LIST labels
const char* menu[5][5] = {
	{"GPS","ИНТЕРНЕТ1","ИНТЕРНЕТ2","GSM"," "},
	{"СВОБОДНО","СВОБОДНО","СВОБОДНО","СВОБОДНО","СВОБОДНО"},
	{"СВОБОДНО","СВОБОДНО","СВОБОДНО","СВОБОДНО","СВОБОДНО"},
	{"СВОБОДНО","СВОБОДНО","СВОБОДНО","СВОБОДНО","СВОБОДНО"},
	{"СВОБОДНО","СВОБОДНО","СВОБОДНО","СВОБОДНО","СВОБОДНО"}
};

static constexpr uint8_t PAGE1_LIST_COUNT = 4;

// -------------------- menu inactivity --------------------
static constexpr uint32_t MENU_INACTIVITY_MS = 10UL * 60UL * 1000UL; // 10 minutes
static uint32_t gMenuLastInputMs = 0;
static bool gMenuLastInputInit = false;

static inline void menuMarkUserActivity()
{
	gMenuLastInputMs = millis();
	gMenuLastInputInit = true;
}

// SW5 = PA0,active low (0 = forbid save)
static inline bool menuSaveAllowed()
{
	return (digitalRead(PA0) != LOW);
}

// -------------------- Persist LOG mode in EEPROM (AT24C128 @0x50) --------------------
// NOTE:Menu::setup() is called before Wire.begin() in your .ino.
// Therefore we load lazily in Menu::update() when I2C is ready.
// We store 1 byte at safe high address with magic+ver.
static constexpr uint8_t LOGCFG_I2C_ADDR = AT24C128Settings::I2C_ADDR; // 0x50
static constexpr uint16_t LOGCFG_ADDR_MAGIC = 0x3F00; // near end of 16KB
static constexpr uint16_t LOGCFG_ADDR_VER = LOGCFG_ADDR_MAGIC + 2;
static constexpr uint16_t LOGCFG_ADDR_MODE = LOGCFG_ADDR_MAGIC + 3;
static constexpr uint16_t LOGCFG_MAGIC = 0x4C47; // 'G''L'
static constexpr uint8_t LOGCFG_VER = 1;
static constexpr uint8_t LOGCFG_PAGE = 64;

static bool gLogCfgLoaded = false;
static uint32_t gLogCfgLastTryMs = 0;

static bool eepWaitReady(uint32_t timeoutMs = 50)
{
	uint32_t t0 = millis();
	while ((millis() - t0) < timeoutMs) {
		Wire.beginTransmission(LOGCFG_I2C_ADDR);
		if (Wire.endTransmission() == 0) return true;
		delay(1);
	}
	return false;
}

static bool eepReadBytes(uint16_t memAddr, uint8_t* out, size_t len)
{
	Wire.beginTransmission(LOGCFG_I2C_ADDR);
	Wire.write((uint8_t)(memAddr >> 8));
	Wire.write((uint8_t)(memAddr & 0xFF));
	if (Wire.endTransmission(false) != 0) return false;

	size_t got = Wire.requestFrom((int)LOGCFG_I2C_ADDR, (int)len);
	if (got != len) return false;

	for (size_t i = 0; i < len; i++) {
		int v = Wire.read();
		if (v < 0) return false;
		out[i] = (uint8_t)v;
	}
	return true;
}

static bool eepWritePage(uint16_t memAddr, const uint8_t* data, size_t len)
{
	Wire.beginTransmission(LOGCFG_I2C_ADDR);
	Wire.write((uint8_t)(memAddr >> 8));
	Wire.write((uint8_t)(memAddr & 0xFF));
	for (size_t i = 0; i < len; i++) Wire.write(data[i]);
	return (Wire.endTransmission() == 0);
}

static bool eepWriteBytes(uint16_t memAddr, const uint8_t* data, size_t len)
{
	size_t off = 0;
	while (off < len)
	{
		uint16_t a = (uint16_t)(memAddr + off);
		uint8_t pageOff = (uint8_t)(a % LOGCFG_PAGE);
		size_t chunk = min((size_t)(LOGCFG_PAGE - pageOff), len - off);

		if (!eepWritePage(a, data + off, chunk)) return false;
		if (!eepWaitReady(50)) return false;

		off += chunk;
	}
	return true;
}

static bool logCfgLoadFromEeprom(bool& outTimeOnly)
{
	uint8_t b[4];
	if (!eepReadBytes(LOGCFG_ADDR_MAGIC, b, sizeof(b))) return false;

	uint16_t mg = (uint16_t)b[0] | ((uint16_t)b[1] << 8);
	uint8_t ver = b[2];
	uint8_t mode = b[3];

	if (mg != LOGCFG_MAGIC) return false;
	if (ver != LOGCFG_VER) return false;

	outTimeOnly = (mode != 0);
	return true;
}

static bool logCfgSaveToEeprom(bool timeOnly)
{
	// write MODE+VER first,then MAGIC as commit (like transactional)
	uint8_t mv[2] = { (uint8_t)(timeOnly ? 1 : 0),LOGCFG_VER };
	// write mode at MODE,ver at VER
	if (!eepWriteBytes(LOGCFG_ADDR_MODE, &mv[0], 1)) return false;
	if (!eepWriteBytes(LOGCFG_ADDR_VER, &mv[1], 1)) return false;

	uint8_t mg[2] = { (uint8_t)(LOGCFG_MAGIC & 0xFF),(uint8_t)(LOGCFG_MAGIC >> 8) };
	if (!eepWriteBytes(LOGCFG_ADDR_MAGIC, mg, 2)) return false;

	return true;
}

// lazy loader (retry until success,once per 1s)
static void logCfgEnsureLoadedLazy(uint32_t now)
{
	if (gLogCfgLoaded) return;
	if (now - gLogCfgLastTryMs < 1000) return;
	gLogCfgLastTryMs = now;

	bool v = false;
	if (logCfgLoadFromEeprom(v)) {
		gLogTimeOnly = v;
		gLogCfgLoaded = true;
	}
}

// -------------------- Period helpers --------------------
static inline uint32_t periodIdxToMs(uint8_t idx)
{
	switch (idx % 6) {
	case 0:return 1UL * 60UL * 1000UL;
	case 1:return 10UL * 60UL * 1000UL;
	case 2:return 30UL * 60UL * 1000UL;
	case 3:return 1UL * 60UL * 60UL * 1000UL;
	case 4:return 6UL * 60UL * 60UL * 1000UL;
	default:return 12UL * 60UL * 60UL * 1000UL;
	}
}

static inline bool isFreshByPeriod(uint32_t lastMs, uint32_t periodMs, uint32_t nowMs)
{
	if (lastMs == 0) return false;
	if (periodMs == 0) return false;
	const uint32_t limit = (periodMs * 13UL) / 10UL; // +30%
	return (nowMs - lastMs) <= limit;
}

// -------------------- INTERNET2:real time receive stamp (derived from lastNtpUtc change) --------------------
static uint32_t gNet2TimeRxMs = 0;
static uint32_t gNet2LastNtpUtc = 0;
static uint32_t gNet2PollMs = 0;

static void pollInternet2TimeStatus(uint32_t now)
{
	if (now - gNet2PollMs < 1000) return; // 1 Hz
	gNet2PollMs = now;

	Internet2Client::Status st;
	if (!internet2.readStatus(st)) return;

	// New successful sync with new lastNtpUtc => consider "time arrived now"
	if (st.lastSyncOk && st.lastNtpUtc != 0 && st.lastNtpUtc != gNet2LastNtpUtc) {
		gNet2LastNtpUtc = st.lastNtpUtc;
		gNet2TimeRxMs = now;
	}
}

// -------------------- GSM "arrival" proxy (no explicit time timestamp available here) --------------------
static uint32_t gGsmRxMs = 0;
static int8_t gPrevCsq = -127;
static uint8_t gPrevBars = 255;
static bool gPrevReg = false;

static void updateGsmRxStamp(uint32_t now)
{
	if (gPrevCsq == -127) {
		gPrevCsq = gCsqRssi;
		gPrevBars = gSignalBars;
		gPrevReg = gNetRegistered;
		gGsmRxMs = now;
		return;
	}
	if (gCsqRssi != gPrevCsq || gSignalBars != gPrevBars || gNetRegistered != gPrevReg) {
		gPrevCsq = gCsqRssi;
		gPrevBars = gSignalBars;
		gPrevReg = gNetRegistered;
		gGsmRxMs = now;
	}
}

// -------------------- Channel OK predicates (STRICT:based on TIME ... OK stamps) --------------------
static inline bool chGpsOk(uint32_t now)
{
	return (syncData.gpsEnable != 0) &&
		isFreshByPeriod((uint32_t)gTimeOkMsGps, periodIdxToMs(syncData.gpsPeriodIdx), now);
}

static inline bool chIp1Ok(uint32_t now)
{
	return (syncData.netEnable != 0) &&
		isFreshByPeriod((uint32_t)gTimeOkMsNet1, periodIdxToMs(syncData.netPeriodIdx), now);
}

static inline bool chIp2Ok(uint32_t now)
{
	return (syncData.net2Enable != 0) &&
		isFreshByPeriod((uint32_t)gTimeOkMsNet2, periodIdxToMs(syncData.net2PeriodIdx), now);
}

static inline bool chGsmOk(uint32_t now)
{
	return (syncData.gsmEnable != 0) &&
		isFreshByPeriod((uint32_t)gTimeOkMsGsm, periodIdxToMs(syncData.gsmPeriodIdx), now);
}

// -------------------- Top marker [O]/[X] (SW5 only,anti-flicker) --------------------
static int8_t gSw5Cached = -1;

static inline void drawSw5MarkerTopForce(TFT_eSPI* d, int y)
{
	if (!d) return;
	d->loadFont(AA_FONT_TIME18);

	const bool allowed = menuSaveAllowed();
	const char* mark = allowed ? "[O]" : "[X]";
	const uint16_t col = allowed ? TFT_GREEN : TFT_RED;

	const int w = (int)d->textWidth(mark);
	const int x = 320 - w - 3;

	// clear ONLY marker area
	d->fillRect(x - 1, y, w + 2, 16, TFT_BLACK);

	d->setTextColor(col, TFT_BLACK);
	d->drawString(mark, x, y);
}

static inline void drawSw5MarkerTopIfChanged(TFT_eSPI* d, int y)
{
	if (!d) return;
	const int8_t s = menuSaveAllowed() ? 1 : 0;
	if (gSw5Cached < 0 || gSw5Cached != s) {
		gSw5Cached = s;
		drawSw5MarkerTopForce(d, y);
	}
}

// -------------------- HOME Sprite (dynamic lines) --------------------
static constexpr int HOME_SPR_H = 14; // font ~12 +2
static TFT_eSprite* gHomeLineSpr = nullptr;
static TFT_eSPI* gHomeSprOwner = nullptr;

static void homeSpritesFree()
{
	if (gHomeLineSpr) {
		gHomeLineSpr->unloadFont();
		gHomeLineSpr->deleteSprite();
		delete gHomeLineSpr;
		gHomeLineSpr = nullptr;
	}
	gHomeSprOwner = nullptr;
}

static void homeSpritesEnsure(TFT_eSPI* tft)
{
	if (!tft) return;

	if (gHomeSprOwner && gHomeSprOwner != tft) {
		homeSpritesFree();
	}
	if (!gHomeSprOwner) gHomeSprOwner = tft;

	if (!gHomeLineSpr) {
		gHomeLineSpr = new (std::nothrow) TFT_eSprite(tft);
		if (gHomeLineSpr) {
			gHomeLineSpr->setColorDepth(16);
			if (gHomeLineSpr->createSprite(320, HOME_SPR_H) == nullptr) {
				delete gHomeLineSpr;
				gHomeLineSpr = nullptr;
			}
			else {
				gHomeLineSpr->loadFont(AA_FONT_TIME18);
				gHomeLineSpr->setTextColor(TFT_WHITE, TFT_BLACK);
			}
		}
	}
}

static inline void homeLineBegin() { if (gHomeLineSpr) gHomeLineSpr->fillSprite(TFT_BLACK); }

static inline void homeLineText(int x, uint16_t fg, const char* txt)
{
	if (!gHomeLineSpr || !txt) return;
	gHomeLineSpr->setTextColor(fg, TFT_BLACK);
	gHomeLineSpr->drawString(txt, x, 0);
}

static inline void homeLineTextXY(int x, int y, uint16_t fg, const char* txt)
{
	if (!gHomeLineSpr || !txt) return;
	gHomeLineSpr->setTextColor(fg, TFT_BLACK);
	gHomeLineSpr->drawString(txt, x, y);
}

static inline int homeTextW(const char* txt)
{
	if (!gHomeLineSpr || !txt) return 0;
	return (int)gHomeLineSpr->textWidth(txt);
}

static inline void homeLinePush(int y) { if (gHomeLineSpr) gHomeLineSpr->pushSprite(0, y); }

// -------------------- HOME caches --------------------
static char gHomeLastLine1[160] = { 0 };
static char gHomeLastLine2[240] = { 0 };
static char gHomeLastLine3[240] = { 0 };
static char gHomeLastLine4[160] = { 0 };
static char gHomeLastFooterDate[48] = { 0 };
static char gHomeLastFooterVer[64] = { 0 };

// DHCP cfg caches for HOME
static LanIfStore::IfConfig gHomeIf1Cfg{};
static LanIfStore::IfConfig gHomeIf2Cfg{};
static uint32_t gHomeIfCfgMs = 0;

static void homeMaybeReloadLanCfg()
{
	uint32_t now = millis();
	if (now - gHomeIfCfgMs < 2000) return;
	gHomeIfCfgMs = now;
	lanStore.load(LanIfStore::IF1, gHomeIf1Cfg, false);
	lanStore.load(LanIfStore::IF2, gHomeIf2Cfg, false);
}

// Helper:IP to octets (for pages)
static void ipToOctets(const IPAddress& ip, uint8_t out[4])
{
	out[0] = ip[0];
	out[1] = ip[1];
	out[2] = ip[2];
	out[3] = ip[3];
}

// -------------------- Base for pages --------------------
class PageBase :public IMenuPage {
public:
	explicit PageBase(Menu* m) :_m(m) {}
protected:
	Menu* _m = nullptr;
	TFT_eSPI* tft() const { return _m ? _m->display() : nullptr; }
};

// -------------------- GPS helpers --------------------
static void formatGpsHomeSatsPart(char* out, size_t n)
{
	snprintf(out, n, " Спутники:%u/%u", (unsigned)gGpsSatsUsed, (unsigned)gGpsSatsView);
}

// Menu GPS info:ONLY line1; no "Обновлено"
static void formatGpsMenuLine1(char* out, size_t n)
{
	double hdop = (double)gGpsHdop_x100 / 100.0;
	snprintf(out, n, "Спутники:%u/%u FIX:%s HDOP:%.2f",
		(unsigned)gGpsSatsUsed,
		(unsigned)gGpsSatsView,
		gGpsFix ? "Да" : "Нет",
		hdop);
}

// -------------------- Page1:Sources --------------------
class Page1 :public PageBase {
public:
	using PageBase::PageBase;

	void setup() override {}

	void onActivate() override
	{
		_mode = MODE_LIST;
		_sel = 0;
		_param = 0;

		_tmp = syncData;
		_tzTargetTmp = cfg.tzTargetHours;
		_tzNtpTmp = cfg.tzNtpHours;

		_staticDrawn = false;
		_lastSel = 255;
		_lastParam = 255;
		_lastHash = 0;

		_lastGpsLine1[0] = 0;
		_lastGpsInfoMs = 0;

		draw();
	}

	void update() override
	{
		drawSw5MarkerTopIfChanged(tft(), Y_TITLE);

		if (_mode == MODE_GPS) {
			uint32_t now = millis();
			if (now - _lastGpsInfoMs >= 500) {
				_lastGpsInfoMs = now;
				drawGpsInfo(false);
			}
		}
	}

	void draw() override
	{
		if (!_staticDrawn) {
			drawStatic();
			_staticDrawn = true;
			_lastSel = 255;
			_lastParam = 255;
			_lastHash = 0;
		}
		drawDynamic(true);
	}

	void onButtonPressed(int buttonID) override
	{
		if (_mode == MODE_LIST) {
			if (buttonID == BTN_SW2) { _sel = (uint8_t)((_sel + (PAGE1_LIST_COUNT - 1)) % PAGE1_LIST_COUNT); drawDynamic(false); return; }
			if (buttonID == BTN_SW3) { _sel = (uint8_t)((_sel + 1) % PAGE1_LIST_COUNT); drawDynamic(false); return; }
			if (buttonID == BTN_SW1) {
				if (_sel == 0) _mode = MODE_GPS;
				else if (_sel == 1) _mode = MODE_NET;
				else if (_sel == 2) _mode = MODE_NET2;
				else if (_sel == 3) _mode = MODE_GSM;
				else return;

				_param = 0;
				_tzTargetTmp = cfg.tzTargetHours;
				_tzNtpTmp = cfg.tzNtpHours;

				_staticDrawn = false;
				draw();
				return;
			}
			if (buttonID == BTN_SW4) { _m->backToStart(); return; }
			return;
		}

		// SUBMENU
		if (buttonID == BTN_SW1) { _param = (_param + 1) % paramCount(); drawDynamic(false); return; }
		if (buttonID == BTN_SW2) { changeParam(-1); drawDynamic(false); return; }
		if (buttonID == BTN_SW3) { changeParam(+1); drawDynamic(false); return; }

		if (buttonID == BTN_SW4) {
			if (menuSaveAllowed()) commitSave();
			_mode = MODE_LIST;
			_sel = 0;
			_staticDrawn = false;
			draw();
			return;
		}
	}

	void onButtonReleased(int) override {}

private:
	enum Mode :uint8_t { MODE_LIST = 0, MODE_GPS = 1, MODE_NET = 2, MODE_NET2 = 3, MODE_GSM = 4 };

	static const int X = 10;
	static const int Y_TITLE = 0;
	static const int Y0 = 22;
	static const int DY = 22;
	static const int LINE_W = 320;
	static const int LINE_H = 20;
	static const int Y_HINT1 = 132;
	static const int Y_HINT2 = 148;

	Mode _mode = MODE_LIST;
	bool _staticDrawn = false;
	uint8_t _sel = 0;
	uint8_t _param = 0;

	SyncSourcesStore::Data _tmp{};
	int8_t _tzTargetTmp = 0;
	int8_t _tzNtpTmp = 0;

	uint8_t _lastSel = 255;
	uint8_t _lastParam = 255;
	uint32_t _lastHash = 0;

	uint32_t _lastGpsInfoMs = 0;
	char _lastGpsLine1[80] = { 0 };

	uint8_t paramCount() const {
		if (_mode == MODE_GPS) return 4;
		if (_mode == MODE_NET) return 5;
		if (_mode == MODE_NET2) return 4;
		if (_mode == MODE_GSM) return 5;
		return 0;
	}

	static void clampTz(int8_t& tz) { if (tz < -12) tz = -12; if (tz > 14) tz = 14; }
	static void formatTz(char* out, size_t n, int8_t tzHours) { if (tzHours == 0) snprintf(out, n, "UTC"); else snprintf(out, n, "UTC%+d", (int)tzHours); }

	void drawStatic()
	{
		auto d = tft(); if (!d) return;
		d->loadFont(AA_FONT_TIME18);
		d->fillScreen(TFT_BLACK);

		d->setTextColor(TFT_YELLOW, TFT_BLACK);
		if (_mode == MODE_LIST) d->drawString("ИСТОЧНИКИ СИНХРОНИЗАЦИИ", X, Y_TITLE);
		else if (_mode == MODE_GPS) d->drawString("GPS", X, Y_TITLE);
		else if (_mode == MODE_NET) d->drawString("ИНТЕРНЕТ1", X, Y_TITLE);
		else if (_mode == MODE_NET2) d->drawString("ИНТЕРНЕТ2", X, Y_TITLE);
		else if (_mode == MODE_GSM) d->drawString("GSM", X, Y_TITLE);

		drawSw5MarkerTopForce(d, Y_TITLE);

		for (int i = 0; i < 6; i++) d->fillRect(0, Y0 + i * DY, LINE_W, LINE_H, TFT_BLACK);

		d->setTextColor(TFT_WHITE, TFT_BLACK);
		d->fillRect(0, Y_HINT1, 320, 38, TFT_BLACK);

		if (_mode == MODE_LIST) {
			d->drawString("SW2:UP SW3:DOWN SW1:ENTER", X, Y_HINT1);
			d->drawString("SW4:HOME", X, Y_HINT2);
		}
		else {
			d->drawString("SW1:NEXT SW2:- SW3:+", X, Y_HINT1);
			d->drawString("SW4:SAVE+BACK", X, Y_HINT2);
		}
	}

	void drawDynamic(bool force)
	{
		auto d = tft(); if (!d) return;
		d->loadFont(AA_FONT_TIME18);

		if (_mode == MODE_LIST) {
			if (!force && _lastSel == _sel) return;
			_lastSel = _sel;

			for (int i = 0; i < PAGE1_LIST_COUNT; i++) {
				d->fillRect(0, Y0 + i * DY, LINE_W, LINE_H, TFT_BLACK);
				d->setTextColor((i == _sel) ? TFT_GREEN : TFT_WHITE, TFT_BLACK);
				d->drawString(_m->itemLabel(0, i), X, Y0 + i * DY);
			}
			d->fillRect(0, Y0 + 4 * DY, LINE_W, LINE_H, TFT_BLACK);
			return;
		}

		uint32_t h = calcHash();
		if (!force && _lastParam == _param && _lastHash == h) {
			if (_mode == MODE_GPS) drawGpsInfo(false);
			return;
		}
		_lastParam = _param;
		_lastHash = h;

		uint8_t n = paramCount();
		for (uint8_t i = 0; i < n; i++) {
			d->fillRect(0, Y0 + i * DY, LINE_W, LINE_H, TFT_BLACK);
			d->setTextColor((i == _param) ? TFT_CYAN : TFT_WHITE, TFT_BLACK);
			if (i == _param) d->drawString(">", 0, Y0 + i * DY);

			char line[96];
			buildLine(i, line, sizeof(line));
			d->drawString(line, X, Y0 + i * DY);
		}

		if (_mode == MODE_GPS) drawGpsInfo(true);
	}

	uint32_t calcHash() const
	{
		uint32_t h = 2166136261u;
		auto mix = [&](uint32_t v) { h ^= v; h *= 16777619u; };

		if (_mode == MODE_GPS) { mix(_tmp.gpsEnable); mix(_tmp.gpsPeriodIdx); }
		else if (_mode == MODE_NET) { mix(_tmp.netEnable); mix(_tmp.netProviderIdx); mix(_tmp.netPeriodIdx); }
		else if (_mode == MODE_NET2) { mix(_tmp.net2Enable); mix(_tmp.net2PeriodIdx); }
		else if (_mode == MODE_GSM) { mix(_tmp.gsmEnable); mix(_tmp.gsmProviderIdx); mix(_tmp.gsmPeriodIdx); }

		mix((uint8_t)_tzTargetTmp);
		mix((uint8_t)_tzNtpTmp);
		mix(_param);
		return h;
	}

	void buildLine(uint8_t i, char* out, size_t n) const
	{
		char tzBuf[16];
		formatTz(tzBuf, sizeof(tzBuf), _tzTargetTmp);

		if (_mode == MODE_GPS) {
			if (i == 0) snprintf(out, n, "1) GPS:%s", _tmp.gpsEnable ? "ON" : "OFF");
			else if (i == 1) snprintf(out, n, "2) ПЕРИОД:%s", kPeriodsName[_tmp.gpsPeriodIdx % 6]);
			else if (i == 2) snprintf(out, n, "3) Часовой пояс:%s", tzBuf);
			else snprintf(out, n, "4) TZ NTP:%d", (int)_tzNtpTmp);
			return;
		}
		if (_mode == MODE_NET) {
			if (i == 0) snprintf(out, n, "1) NTP:%s", _tmp.netEnable ? "ON" : "OFF");
			else if (i == 1) snprintf(out, n, "2) NTP IP:%s", kNtpIpStr[_tmp.netProviderIdx % 5]);
			else if (i == 2) snprintf(out, n, "3) ПЕРИОД:%s", kPeriodsName[_tmp.netPeriodIdx % 6]);
			else if (i == 3) snprintf(out, n, "4) Часовой пояс:%s", tzBuf);
			else snprintf(out, n, "5) TZ NTP:%d", (int)_tzNtpTmp);
			return;
		}
		if (_mode == MODE_NET2) {
			if (i == 0) snprintf(out, n, "1) NET2:%s", _tmp.net2Enable ? "ON" : "OFF");
			else if (i == 1) snprintf(out, n, "2) ПЕРИОД:%s", kPeriodsName[_tmp.net2PeriodIdx % 6]);
			else if (i == 2) snprintf(out, n, "3) Часовой пояс:%s", tzBuf);
			else snprintf(out, n, "4) TZ NTP:%d", (int)_tzNtpTmp);
			return;
		}
		// GSM
		if (i == 0) snprintf(out, n, "1) GSM:%s", _tmp.gsmEnable ? "ON" : "OFF");
		else if (i == 1) snprintf(out, n, "2) OP:%s", kGsmProviders[_tmp.gsmProviderIdx % 3]);
		else if (i == 2) snprintf(out, n, "3) ПЕРИОД:%s", kPeriodsName[_tmp.gsmPeriodIdx % 6]);
		else if (i == 3) snprintf(out, n, "4) Часовой пояс:%s", tzBuf);
		else snprintf(out, n, "5) TZ NTP:%d", (int)_tzNtpTmp);
	}

	void changeParam(int delta)
	{
		if (_mode == MODE_GPS) {
			if (_param == 0) _tmp.gpsEnable = _tmp.gpsEnable ? 0 : 1;
			else if (_param == 1) _tmp.gpsPeriodIdx = (uint8_t)((_tmp.gpsPeriodIdx + (delta > 0 ? 1 : 5)) % 6);
			else if (_param == 2) { _tzTargetTmp += (delta > 0 ? 1 : -1); clampTz(_tzTargetTmp); }
			else { _tzNtpTmp += (delta > 0 ? 1 : -1); clampTz(_tzNtpTmp); }
			return;
		}
		if (_mode == MODE_NET) {
			if (_param == 0) _tmp.netEnable = _tmp.netEnable ? 0 : 1;
			else if (_param == 1) _tmp.netProviderIdx = (uint8_t)((_tmp.netProviderIdx + (delta > 0 ? 1 : 4)) % 5);
			else if (_param == 2) _tmp.netPeriodIdx = (uint8_t)((_tmp.netPeriodIdx + (delta > 0 ? 1 : 5)) % 6);
			else if (_param == 3) { _tzTargetTmp += (delta > 0 ? 1 : -1); clampTz(_tzTargetTmp); }
			else { _tzNtpTmp += (delta > 0 ? 1 : -1); clampTz(_tzNtpTmp); }
			return;
		}
		if (_mode == MODE_NET2) {
			if (_param == 0) _tmp.net2Enable = _tmp.net2Enable ? 0 : 1;
			else if (_param == 1) _tmp.net2PeriodIdx = (uint8_t)((_tmp.net2PeriodIdx + (delta > 0 ? 1 : 5)) % 6);
			else if (_param == 2) { _tzTargetTmp += (delta > 0 ? 1 : -1); clampTz(_tzTargetTmp); }
			else { _tzNtpTmp += (delta > 0 ? 1 : -1); clampTz(_tzNtpTmp); }
			return;
		}
		// GSM
		if (_param == 0) _tmp.gsmEnable = _tmp.gsmEnable ? 0 : 1;
		else if (_param == 1) _tmp.gsmProviderIdx = (uint8_t)((_tmp.gsmProviderIdx + (delta > 0 ? 1 : 2)) % 3);
		else if (_param == 2) _tmp.gsmPeriodIdx = (uint8_t)((_tmp.gsmPeriodIdx + (delta > 0 ? 1 : 5)) % 6);
		else if (_param == 3) { _tzTargetTmp += (delta > 0 ? 1 : -1); clampTz(_tzTargetTmp); }
		else { _tzNtpTmp += (delta > 0 ? 1 : -1); clampTz(_tzNtpTmp); }
	}

	void commitSave()
	{
		syncData = _tmp;
		(void)syncStore.save(syncData);

		cfg.tzTargetHours = _tzTargetTmp;
		cfg.tzNtpHours = _tzNtpTmp;
		(void)ee.writeTzTargetHours(cfg.tzTargetHours);
		(void)ee.writeTzNtpHours(cfg.tzNtpHours);

		planner.onSettingsChanged();
	}

	void drawGpsInfo(bool clearLine)
	{
		auto d = tft(); if (!d) return;
		d->loadFont(AA_FONT_TIME18);

		const int y1 = Y0 + 3 * DY;
		const int y2 = Y0 + 4 * DY;

		char line1[80];
		formatGpsMenuLine1(line1, sizeof(line1));

		if (clearLine || strcmp(_lastGpsLine1, line1) != 0) {
			d->fillRect(0, y1, LINE_W, LINE_H, TFT_BLACK);
			d->setTextColor(TFT_GREEN, TFT_BLACK);
			d->drawString(line1, X, y1);
			strncpy(_lastGpsLine1, line1, sizeof(_lastGpsLine1) - 1);
			_lastGpsLine1[sizeof(_lastGpsLine1) - 1] = 0;
		}

		if (clearLine) d->fillRect(0, y2, LINE_W, LINE_H, TFT_BLACK);
	}
};

// -------------------- Internet pages base --------------------
class InternetPageBase :public PageBase {
public:
	using PageBase::PageBase;

protected:
	static const int X = 10;
	static const int Y_TITLE = 0;
	static const int Y0 = 22;
	static const int DY = 20;
	static const int W = 320;
	static const int H = 18;

	static constexpr int TOP_MARKER_SAFE_W = 40;

	enum Param :uint8_t { P_DHCP = 0, P_NTP = 1, P_PERIOD = 2, PARAM_COUNT = 3 };

	void drawIpLineRO(TFT_eSPI* d, const char* name, uint8_t v[4], int row)
	{
		d->loadFont(AA_FONT_TIME18);
		d->setTextColor(TFT_WHITE, TFT_BLACK);

		char s[40];
		snprintf(s, sizeof(s), "%s %u.%u.%u.%u", name, v[0], v[1], v[2], v[3]);
		d->drawString(s, X, Y0 + row * DY);
	}
};

// -------------------- Page2:INTERNET1 --------------------
class Page2 :public InternetPageBase {
public:
	using InternetPageBase::InternetPageBase;

	void setup() override {}

	void onActivate() override
	{
		_staticDrawn = false;
		_param = 0;
		lanStore.load(LanIfStore::IF1, _cfg, false);

		_statLine[0] = 0;
		_lastStatMs = 0;

		draw();
	}

	void update() override
	{
		drawSw5MarkerTopIfChanged(tft(), Y_TITLE);

		uint32_t now = millis();
		if (now - _lastStatMs < 1000) return;
		_lastStatMs = now;

		IPAddress ip = ntpLan.localIP();
		bool ok = !(ip[0] == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] == 0);

		char line[48];
		snprintf(line, sizeof(line), "STATUS :%s", ok ? "OK" : "NO");

		if (strcmp(_statLine, line) != 0) {
			strncpy(_statLine, line, sizeof(_statLine) - 1);
			_statLine[sizeof(_statLine) - 1] = 0;
			drawStatusLine();
		}
	}

	void draw() override
	{
		if (!_staticDrawn) { drawStatic(); _staticDrawn = true; }
		drawDynamic(true);
	}

	void onButtonPressed(int buttonID) override
	{
		if (buttonID == BTN_SW4) {
			if (menuSaveAllowed()) {
				lanStore.save(LanIfStore::IF1, _cfg, true);
				applyInternet1FromStore();
			}
			_m->backToStart();
			return;
		}
		if (buttonID == BTN_SW1) { _param = (_param + 1) % PARAM_COUNT; drawDynamic(false); return; }
		if (buttonID == BTN_SW2 || buttonID == BTN_SW3) { applyDelta(buttonID == BTN_SW3 ? +1 : -1); drawDynamic(false); return; }
	}

	void onButtonReleased(int) override {}

private:
	LanIfStore::IfConfig _cfg{};
	bool _staticDrawn = false;
	uint8_t _param = 0;

	uint32_t _lastStatMs = 0;
	char _statLine[48] = { 0 };

	void drawStatusLine()
	{
		auto d = tft(); if (!d) return;
		d->loadFont(AA_FONT_TIME18);

		d->fillRect(140, 0, 320 - 140 - TOP_MARKER_SAFE_W, 16, TFT_BLACK);

		bool ok = (strstr(_statLine, "STATUS :OK") != nullptr);
		d->setTextColor(ok ? TFT_GREEN : TFT_RED, TFT_BLACK);
		d->drawString(_statLine, 140, Y_TITLE);
	}

	void drawStatic()
	{
		auto d = tft(); if (!d) return;
		d->loadFont(AA_FONT_TIME18);
		d->fillScreen(TFT_BLACK);

		d->setTextColor(TFT_YELLOW, TFT_BLACK);
		d->drawString("INTERNET 1", X, Y_TITLE);

		drawSw5MarkerTopForce(d, Y_TITLE);
		drawStatusLine();
	}

	void drawDynamic(bool)
	{
		auto d = tft(); if (!d) return;
		d->loadFont(AA_FONT_TIME18);

		for (int i = 0; i < 7; i++) d->fillRect(0, Y0 + i * DY, W, H, TFT_BLACK);

		d->setTextColor(_param == P_DHCP ? TFT_CYAN : TFT_WHITE, TFT_BLACK);
		d->drawString(_param == P_DHCP ? ">" : " ", 0, Y0 + 0 * DY);
		d->drawString(_cfg.dhcp ? "DHCP :ON" : "DHCP :OFF", X, Y0 + 0 * DY);

		char b[64];
		d->setTextColor(_param == P_NTP ? TFT_CYAN : TFT_WHITE, TFT_BLACK);
		d->drawString(_param == P_NTP ? ">" : " ", 0, Y0 + 1 * DY);
		snprintf(b, sizeof(b), "NTP :%s", kNtpIpStr[_cfg.ntpIdx % 5]);
		d->drawString(b, X, Y0 + 1 * DY);

		d->setTextColor(_param == P_PERIOD ? TFT_CYAN : TFT_WHITE, TFT_BLACK);
		d->drawString(_param == P_PERIOD ? ">" : " ", 0, Y0 + 2 * DY);
		snprintf(b, sizeof(b), "ПЕРИОД :%s", kPeriodsName[_cfg.periodIdx % 6]);
		d->drawString(b, X, Y0 + 2 * DY);

		uint8_t ipNow[4];
		ipToOctets(ntpLan.localIP(), ipNow);
		drawIpLineRO(d, "IP :", ipNow, 3);
		drawIpLineRO(d, "MASK :", _cfg.mask, 4);
		drawIpLineRO(d, "CW :", _cfg.gw, 5);
		drawIpLineRO(d, "DNS :", _cfg.dns, 6);
	}

	void applyDelta(int delta)
	{
		if (_param == P_DHCP) { _cfg.dhcp = _cfg.dhcp ? 0 : 1; return; }
		if (_param == P_NTP) { _cfg.ntpIdx = (uint8_t)((_cfg.ntpIdx + (delta > 0 ? 1 : 4)) % 5); return; }
		if (_param == P_PERIOD) { _cfg.periodIdx = (uint8_t)((_cfg.periodIdx + (delta > 0 ? 1 : 5)) % 6); return; }
	}
};

// -------------------- Page3:INTERNET2 --------------------
class Page3 :public InternetPageBase {
public:
	using InternetPageBase::InternetPageBase;

	void setup() override {}

	void onActivate() override
	{
		_staticDrawn = false;
		_param = 0;
		lanStore.load(LanIfStore::IF2, _cfg, false);

		_statLine[0] = 0;
		_lastStatMs = 0;

		draw();
	}

	void update() override
	{
		drawSw5MarkerTopIfChanged(tft(), Y_TITLE);

		uint32_t now = millis();
		if (now - _lastStatMs < 1000) return;
		_lastStatMs = now;

		Internet2Client::Status st;
		bool okRead = internet2.readStatus(st);
		_stValid = okRead;
		if (okRead) _st = st;

		IPAddress ip = okRead ? st.ip : IPAddress(0, 0, 0, 0);
		bool ok = okRead && !(ip[0] == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] == 0);

		char line[48];
		snprintf(line, sizeof(line), "STATUS :%s", ok ? "OK" : "NO");

		if (strcmp(_statLine, line) != 0) {
			strncpy(_statLine, line, sizeof(_statLine) - 1);
			_statLine[sizeof(_statLine) - 1] = 0;
			drawStatusLine();
		}
	}

	void draw() override
	{
		if (!_staticDrawn) { drawStatic(); _staticDrawn = true; }
		drawDynamic(true);
	}

	void onButtonPressed(int buttonID) override
	{
		if (buttonID == BTN_SW4) {
			if (menuSaveAllowed()) {
				lanStore.save(LanIfStore::IF2, _cfg, true);
				applyInternet2FromStore();
			}
			_m->backToStart();
			return;
		}
		if (buttonID == BTN_SW1) { _param = (_param + 1) % PARAM_COUNT; drawDynamic(false); return; }
		if (buttonID == BTN_SW2 || buttonID == BTN_SW3) { applyDelta(buttonID == BTN_SW3 ? +1 : -1); drawDynamic(false); return; }
	}

	void onButtonReleased(int) override {}

private:
	LanIfStore::IfConfig _cfg{};
	bool _staticDrawn = false;
	uint8_t _param = 0;

	uint32_t _lastStatMs = 0;
	char _statLine[48] = { 0 };

	Internet2Client::Status _st{};
	bool _stValid = false;

	void drawStatusLine()
	{
		auto d = tft(); if (!d) return;
		d->loadFont(AA_FONT_TIME18);

		d->fillRect(140, 0, 320 - 140 - TOP_MARKER_SAFE_W, 16, TFT_BLACK);

		bool ok = (strstr(_statLine, "STATUS :OK") != nullptr);
		d->setTextColor(ok ? TFT_GREEN : TFT_RED, TFT_BLACK);
		d->drawString(_statLine, 140, Y_TITLE);
	}

	void drawStatic()
	{
		auto d = tft(); if (!d) return;
		d->loadFont(AA_FONT_TIME18);
		d->fillScreen(TFT_BLACK);

		d->setTextColor(TFT_YELLOW, TFT_BLACK);
		d->drawString("INTERNET 2", X, Y_TITLE);

		drawSw5MarkerTopForce(d, Y_TITLE);
		drawStatusLine();
	}

	void drawDynamic(bool)
	{
		auto d = tft(); if (!d) return;
		d->loadFont(AA_FONT_TIME18);

		for (int i = 0; i < 7; i++) d->fillRect(0, Y0 + i * DY, W, H, TFT_BLACK);

		d->setTextColor(_param == P_DHCP ? TFT_CYAN : TFT_WHITE, TFT_BLACK);
		d->drawString(_param == P_DHCP ? ">" : " ", 0, Y0 + 0 * DY);
		d->drawString(_cfg.dhcp ? "DHCP :ON" : "DHCP :OFF", X, Y0 + 0 * DY);

		char b[64];
		d->setTextColor(_param == P_NTP ? TFT_CYAN : TFT_WHITE, TFT_BLACK);
		d->drawString(_param == P_NTP ? ">" : " ", 0, Y0 + 1 * DY);
		snprintf(b, sizeof(b), "NTP :%s", kNtpIpStr[_cfg.ntpIdx % 5]);
		d->drawString(b, X, Y0 + 1 * DY);

		d->setTextColor(_param == P_PERIOD ? TFT_CYAN : TFT_WHITE, TFT_BLACK);
		d->drawString(_param == P_PERIOD ? ">" : " ", 0, Y0 + 2 * DY);
		snprintf(b, sizeof(b), "ПЕРИОД :%s", kPeriodsName[_cfg.periodIdx % 6]);
		d->drawString(b, X, Y0 + 2 * DY);

		uint8_t ipNow[4];
		IPAddress ip = (_stValid ? _st.ip : IPAddress(0, 0, 0, 0));
		ipToOctets(ip, ipNow);
		drawIpLineRO(d, "IP :", ipNow, 3);
		drawIpLineRO(d, "MASK :", _cfg.mask, 4);
		drawIpLineRO(d, "CW :", _cfg.gw, 5);
		drawIpLineRO(d, "DNS :", _cfg.dns, 6);
	}

	void applyDelta(int delta)
	{
		if (_param == P_DHCP) { _cfg.dhcp = _cfg.dhcp ? 0 : 1; return; }
		if (_param == P_NTP) { _cfg.ntpIdx = (uint8_t)((_cfg.ntpIdx + (delta > 0 ? 1 : 4)) % 5); return; }
		if (_param == P_PERIOD) { _cfg.periodIdx = (uint8_t)((_cfg.periodIdx + (delta > 0 ? 1 : 5)) % 6); return; }
	}
};

// -------------------- Page4:НАСТРОЙКИ GSM (stub title) --------------------
class Page4 :public PageBase {
public:
	using PageBase::PageBase;
	void setup() override {}
	void onActivate() override { draw(); }
	void update() override { drawSw5MarkerTopIfChanged(tft(), 0); }
	void draw() override {
		auto d = tft(); if (!d) return;
		d->loadFont(AA_FONT_TIME18);
		d->fillScreen(TFT_BLACK);
		d->setTextColor(TFT_YELLOW, TFT_BLACK);
		d->drawString("НАСТРОЙКИ GSM", 10, 0);
		drawSw5MarkerTopForce(d, 0);
	}
	void onButtonPressed(int buttonID) override {
		// пока страница-заглушка,выход по SW4
		if (buttonID == BTN_SW4) _m->backToStart();
	}
	void onButtonReleased(int) override {}
};

// -------------------- Page5:НАСТРОЙКИ ЛОГА --------------------
class Page5 :public PageBase {
public:
	using PageBase::PageBase;

	void setup() override {}

	void onActivate() override
	{
		// ensure we have loaded persisted value (if possible)
		logCfgEnsureLoadedLazy(millis());

		_tmpTimeOnly = gLogTimeOnly ? 1 : 0;
		_staticDrawn = false;
		draw();
	}

	void update() override
	{
		drawSw5MarkerTopIfChanged(tft(), Y_TITLE);
	}

	void draw() override
	{
		if (!_staticDrawn) {
			drawStatic();
			_staticDrawn = true;
		}
		drawDynamic();
	}

	void onButtonPressed(int buttonID) override
	{
		if (buttonID == BTN_SW1 || buttonID == BTN_SW2 || buttonID == BTN_SW3) {
			// toggle
			_tmpTimeOnly = _tmpTimeOnly ? 0 : 1;
			drawDynamic();
			return;
		}

		if (buttonID == BTN_SW4) {
			// apply only if save allowed
			if (menuSaveAllowed()) {
				gLogTimeOnly = (_tmpTimeOnly != 0);

				// persist to EEPROM (best effort)
				(void)logCfgSaveToEeprom(gLogTimeOnly);
				gLogCfgLoaded = true;
			}
			_m->backToStart();
			return;
		}
	}

	void onButtonReleased(int) override {}

private:
	static const int X = 10;
	static const int Y_TITLE = 0;
	static const int Y0 = 30;
	static const int DY = 22;
	static const int Y_HINT1 = 132;
	static const int Y_HINT2 = 148;

	bool _staticDrawn = false;
	uint8_t _tmpTimeOnly = 0;

	void drawStatic()
	{
		auto d = tft(); if (!d) return;
		d->loadFont(AA_FONT_TIME18);
		d->fillScreen(TFT_BLACK);

		d->setTextColor(TFT_YELLOW, TFT_BLACK);
		d->drawString("НАСТРОЙКИ ЛОГА", X, Y_TITLE);

		drawSw5MarkerTopForce(d, Y_TITLE);

		d->setTextColor(TFT_WHITE, TFT_BLACK);
		d->fillRect(0, Y_HINT1, 320, 38, TFT_BLACK);
		d->drawString("SW1/SW2/SW3:TOGGLE", X, Y_HINT1);
		d->drawString("SW4:SAVE+HOME", X, Y_HINT2);
	}

	void drawDynamic()
	{
		auto d = tft(); if (!d) return;
		d->loadFont(AA_FONT_TIME18);

		// clear area
		d->fillRect(0, Y0, 320, 80, TFT_BLACK);

		d->setTextColor(TFT_CYAN, TFT_BLACK);
		d->drawString("РЕЖИМ:", X, Y0);

		if (_tmpTimeOnly) {
			d->setTextColor(TFT_GREEN, TFT_BLACK);
			d->drawString("ТОЛЬКО TIME", X, Y0 + DY);
			d->setTextColor(TFT_WHITE, TFT_BLACK);
			d->drawString("Пишутся только TIME REQ/OK/FAIL", X, Y0 + 2 * DY);
		}
		else {
			d->setTextColor(TFT_GREEN, TFT_BLACK);
			d->drawString("ПОЛНЫЙ", X, Y0 + DY);
			d->setTextColor(TFT_WHITE, TFT_BLACK);
			d->drawString("Пишутся все события системы", X, Y0 + 2 * DY);
		}
	}
};

// ---------------------------------------------------------

Menu::Menu(TFT_eSPI* disp) :tft(disp) {}

void Menu::attachPages()
{
	static Page1 p1(this);
	static Page2 p2(this);
	static Page3 p3(this);
	static Page4 p4(this); // GSM
	static Page5 p5(this); // LOG

	pages[0] = &p1; // SOURCES
	pages[1] = &p2; // INTERNET1
	pages[2] = &p3; // INTERNET2
	pages[3] = &p4; // GSM SETTINGS
	pages[4] = &p5; // LOG SETTINGS
}

void Menu::setup(const String& ver)
{
	versionString = ver;

	tft->init();
	tft->setRotation(3);
	tft->fillScreen(TFT_NAVY);
	tft->setTextColor(TFT_WHITE, TFT_BLACK);

	// start screen (kept)
	tft->loadFont(AA_FONT_CALI);
	tft->setCursor(120, 20); tft->println("IVCH");
	tft->setCursor(100, 50); tft->println("DECIMA");

	tft->loadFont(AA_FONT_TIME24I);
	tft->setTextColor(TFT_YELLOW, TFT_BLACK);
	tft->setCursor(84, 90); tft->println("ВКЛЮЧЕНИЕ");

	tft->loadFont(AA_FONT_TIME18);
	tft->setTextColor(TFT_WHITE, TFT_BLACK);
	tft->setCursor(5, 130); tft->println("(C) 2026");
	tft->setCursor(5, 150); tft->println("www.decima.ru");

	uint16_t tbw = tft->textWidth(versionString);
	uint16_t x = (tft->width() - tbw) - 4;
	tft->setCursor(x, 150);
	tft->print(versionString);

	attachPages();
	for (int i = 0; i < 5; i++) if (pages[i]) pages[i]->setup();

	// init SW5 cache (anti-flicker)
	gSw5Cached = menuSaveAllowed() ? 1 : 0;

	// log cfg will be loaded lazily in update() after Wire.begin()
	gLogCfgLoaded = false;
	gLogCfgLastTryMs = 0;

	drawStartPage();
}

void Menu::update()
{
	const uint32_t now = millis();

	// Try load persisted LOG mode (once Wire is ready)
	logCfgEnsureLoadedLazy(now);

	// (legacy) Poll INTERNET2 status + GSM proxy; not used for [T]/[X] anymore,kept harmless
	pollInternet2TimeStatus(now);
	updateGsmRxStamp(now);

	// inactivity auto-home
	if (state != MENU_IDLE) {
		if (gMenuLastInputInit && (now - gMenuLastInputMs > MENU_INACTIVITY_MS)) {
			backToStart();
			return;
		}
	}

	if (state == MENU_IDLE) {
		if (!_homeStaticDrawn) {
			drawHomeStatic();
			_homeStaticDrawn = true;
			_homeDirty = true;
		}

		if (_homeDirty || (now - _lastHomeTickMs) >= 500) {
			_lastHomeTickMs = now;
			drawHomeDynamic();
			_homeDirty = false;
		}
		return;
	}

	if (state == MENU_PAGE_SELECT) {
		drawSw5MarkerTopIfChanged(tft, 10);
		return;
	}

	if (state == MENU_ITEM_SELECT) {
		if (pages[page]) pages[page]->update();
	}
	else if (state == MENU_TEST) {
		drawSw5MarkerTopIfChanged(tft, 10);
	}
}

void Menu::invalidateHome() { _homeDirty = true; }

void Menu::drawStartPage()
{
	state = MENU_IDLE;
	_homeStaticDrawn = false;
	_homeDirty = true;
}

void Menu::drawHomeStatic()
{
	tft->loadFont(AA_FONT_TIME18);
	tft->fillScreen(TFT_BLACK);

	tft->setTextColor(TFT_CYAN, TFT_BLACK);
	tft->drawString("СОСТОЯНИЕ СИСТЕМЫ", 10, Y_HOME);

	// reset caches
	gHomeLastLine1[0] = 0;
	gHomeLastLine2[0] = 0;
	gHomeLastLine3[0] = 0;
	gHomeLastLine4[0] = 0;
	gHomeLastFooterDate[0] = 0;
	gHomeLastFooterVer[0] = 0;

	gHomeIfCfgMs = 0;

	homeSpritesEnsure(tft);

	// force marker on first draw
	drawSw5MarkerTopForce(tft, Y_HOME);
}

void Menu::drawHomeDynamic()
{
	homeSpritesEnsure(tft);
	const uint32_t now = millis();

	drawSw5MarkerTopIfChanged(tft, Y_HOME);

	homeMaybeReloadLanCfg();

	const bool gpsOk = chGpsOk(now);
	const bool ip1Ok = chIp1Ok(now);
	const bool ip2Ok = chIp2Ok(now);
	const bool gsmOk = chGsmOk(now);

	// LINE1 GPS
	{
		const char* fixStr = gGpsFix ? "FIX" : "NOFIX";
		const uint16_t fixCol = gGpsFix ? TFT_GREEN : TFT_RED;

		char sats[64];
		formatGpsHomeSatsPart(sats, sizeof(sats));

		char cache[160];
		snprintf(cache, sizeof(cache), "GPS:%s%s|%u", fixStr, sats, (unsigned)gpsOk);
		if (strcmp(gHomeLastLine1, cache) != 0) {
			strncpy(gHomeLastLine1, cache, sizeof(gHomeLastLine1) - 1);
			gHomeLastLine1[sizeof(gHomeLastLine1) - 1] = 0;

			homeLineBegin();
			int x = 10;
			homeLineText(x, TFT_WHITE, "GPS:");
			x += homeTextW("GPS:");
			homeLineText(x, fixCol, fixStr);
			x += homeTextW(fixStr);
			homeLineText(x, TFT_WHITE, sats);

			const char* ico = gpsOk ? "[T]" : "[X]";
			uint16_t icoCol = gpsOk ? TFT_GREEN : TFT_RED;
			int icoW = homeTextW(ico);
			int icoX = 320 - icoW - 4;
			homeLineText(icoX, icoCol, ico);

			homeLinePush(Y_L1);
		}
	}

	// helper for IP lines 2/3
	auto drawIpLine = [&](int y, const char* tag, const char* ipStr, bool dhcpEnabled, bool ok, char* last, size_t lastN) {
		char cache[240];
		snprintf(cache, sizeof(cache), "%s|%s|%u|%u", tag, ipStr, (unsigned)dhcpEnabled, (unsigned)ok);
		if (strcmp(last, cache) == 0) return;
		strncpy(last, cache, lastN - 1);
		last[lastN - 1] = 0;

		const bool ipUnknown = isZeroIpStr(ipStr);
		const uint16_t ipCol = ipUnknown ? TFT_RED : TFT_WHITE;
		const char* ipShow = ipUnknown ? "НЕ ОПРЕДЕЛЕН" : ipStr;

		homeLineBegin();

		int x = 10;
		homeLineText(x, TFT_WHITE, tag);
		x += homeTextW(tag) + 6;

		if (ipUnknown) homeLineTextXY(x, 1, ipCol, ipShow); else homeLineText(x, ipCol, ipShow);
		x += homeTextW(ipShow) + 8;

		if (!dhcpEnabled) {
			homeLineText(x, TFT_CYAN, "STATIC");
		}
		else {
			homeLineText(x, TFT_CYAN, "DHCP:");
			x += homeTextW("DHCP:");
			const char* st = ipUnknown ? "NO" : "OK";
			const uint16_t stCol = ipUnknown ? TFT_RED : TFT_GREEN;
			if (ipUnknown) homeLineTextXY(x, 1, stCol, st); else homeLineText(x, stCol, st);
		}

		const char* ico = ok ? "[T]" : "[X]";
		const uint16_t icoCol = ok ? TFT_GREEN : TFT_RED;
		const int icoW = homeTextW(ico);
		const int icoX = 320 - icoW - 4;
		if (!ok) homeLineTextXY(icoX, 1, icoCol, ico); else homeLineText(icoX, icoCol, ico);

		homeLinePush(y);
	};

	drawIpLine(Y_L2, "IP1", gIp1Str, (gHomeIf1Cfg.dhcp != 0), ip1Ok, gHomeLastLine2, sizeof(gHomeLastLine2));
	drawIpLine(Y_L3, "IP2", gIp2Str, (gHomeIf2Cfg.dhcp != 0), ip2Ok, gHomeLastLine3, sizeof(gHomeLastLine3));

	// LINE4 GSM
	{
		const char* st = gNetRegistered ? "CONNECTED" : "NO";
		char cache[160];
		snprintf(cache, sizeof(cache), "GSM:%s|%u", st, (unsigned)gsmOk);
		if (strcmp(gHomeLastLine4, cache) != 0) {
			strncpy(gHomeLastLine4, cache, sizeof(gHomeLastLine4) - 1);
			gHomeLastLine4[sizeof(gHomeLastLine4) - 1] = 0;

			homeLineBegin();
			int x = 10;
			homeLineText(x, TFT_WHITE, "GSM:");
			x += homeTextW("GSM:");
			homeLineText(x, gNetRegistered ? TFT_GREEN : TFT_RED, st);

			const char* ico = gsmOk ? "[T]" : "[X]";
			uint16_t icoCol = gsmOk ? TFT_GREEN : TFT_RED;
			int icoW = homeTextW(ico);
			int icoX = 320 - icoW - 4;
			homeLineText(icoX, icoCol, ico);

			homeLinePush(Y_L4);
		}
	}

	// FOOTER
	{
		if (strcmp(gHomeLastFooterDate, gDateStr) != 0 || strcmp(gHomeLastFooterVer, versionString.c_str()) != 0) {
			strncpy(gHomeLastFooterDate, gDateStr, sizeof(gHomeLastFooterDate) - 1);
			gHomeLastFooterDate[sizeof(gHomeLastFooterDate) - 1] = 0;

			strncpy(gHomeLastFooterVer, versionString.c_str(), sizeof(gHomeLastFooterVer) - 1);
			gHomeLastFooterVer[sizeof(gHomeLastFooterVer) - 1] = 0;

			homeLineBegin();
			homeLineText(0, TFT_WHITE, gHomeLastFooterDate);

			int verW = homeTextW(gHomeLastFooterVer);
			int xVer = 320 - verW - 2;
			homeLineText(xVer, TFT_YELLOW, gHomeLastFooterVer);

			homeLinePush(Y_FOOT);
		}
	}
}

void Menu::activate()
{
	page = 0;
	item = 0;
	state = MENU_PAGE_SELECT;
	menuMarkUserActivity();
	draw();
}

void Menu::deactivate()
{
	drawStartPage();
}

bool Menu::isActive() const { return state != MENU_IDLE; }
MenuState Menu::getState() const { return state; }

void Menu::prevPage() { if (state == MENU_PAGE_SELECT) { page = (page + 4) % 5; drawPageSelect(); } }
void Menu::nextPage() { if (state == MENU_PAGE_SELECT) { page = (page + 1) % 5; drawPageSelect(); } }

void Menu::prevItem() { if (state == MENU_ITEM_SELECT) { item = (item + 4) % 5; drawActivePage(); } }
void Menu::nextItem() { if (state == MENU_ITEM_SELECT) { item = (item + 1) % 5; drawActivePage(); } }

void Menu::fixPage()
{
	if (state == MENU_PAGE_SELECT) {
		item = 0;
		state = MENU_ITEM_SELECT;
		menuMarkUserActivity();
		if (pages[page]) pages[page]->onActivate();
		else drawActivePage();
	}
}

void Menu::fixItem()
{
	// legacy
}

void Menu::backToStart() { deactivate(); }

void Menu::select() {}
void Menu::runTest() { state = MENU_TEST; draw(); }

void Menu::draw()
{
	if (state == MENU_PAGE_SELECT) drawPageSelect();
	else if (state == MENU_ITEM_SELECT) drawActivePage();
	else if (state == MENU_TEST) drawTestPage();
	else drawStartPage();
}

void Menu::drawPageSelect()
{
	static const char* topMenuNames[5] = {
		"ИСТОЧНИКИ СИНХРОНИЗАЦИИ",
		"НАСТРОЙКИ ИНТЕРНЕТ1",
		"НАСТРОЙКИ ИНТЕРНЕТ2",
		"НАСТРОЙКИ GSM",
		"НАСТРОЙКИ ЛОГА"
	};

	tft->loadFont(AA_FONT_TIME18);
	tft->fillScreen(TFT_BLACK);

	tft->setTextColor(TFT_CYAN, TFT_BLACK);
	tft->drawString("ВЫБОР МЕНЮ", 10, 10);

	drawSw5MarkerTopForce(tft, 10);

	for (int i = 0; i < 5; i++) {
		bool sel = (i == page);
		tft->setTextColor(sel ? TFT_GREEN : TFT_WHITE, sel ? TFT_BLUE : TFT_BLACK);
		tft->fillRect(10, 35 + i * 24, 310, 22, TFT_BLACK);
		tft->drawString(topMenuNames[i], 10, 35 + i * 24);
	}
}

void Menu::drawActivePage()
{
	if (pages[page]) pages[page]->draw();
}

void Menu::onButtonPressed(int buttonID)
{
	menuMarkUserActivity();

	// HOME:enter menu by SW1
	if (state == MENU_IDLE) {
		if (buttonID == BTN_SW1) activate();
		return;
	}

	// MENU_PAGE_SELECT
	if (state == MENU_PAGE_SELECT) {
		if (buttonID == BTN_SW2) prevPage();
		else if (buttonID == BTN_SW3) nextPage();
		else if (buttonID == BTN_SW1) fixPage();
		else if (buttonID == BTN_SW4) backToStart();
		return;
	}

	// MENU_ITEM_SELECT
	if (state == MENU_ITEM_SELECT) {
		if (pages[page]) {
			pages[page]->onButtonPressed(buttonID);
			return;
		}
	}

	// MENU_TEST
	if (state == MENU_TEST) {
		if (buttonID == BTN_SW4 || buttonID == BTN_SW1) backToStart();
		return;
	}
}

void Menu::onButtonReleased(int buttonID)
{
	menuMarkUserActivity();
	if (state == MENU_ITEM_SELECT && pages[page]) pages[page]->onButtonReleased(buttonID);
}

const char* Menu::itemLabel(int p, int i) const
{
	if (p < 0 || p > 4 || i < 0 || i > 4) return "";
	return menu[p][i];
}

void Menu::drawTestPage()
{
	tft->loadFont(AA_FONT_TIME18);
	tft->fillScreen(TFT_BLACK);
	tft->setTextColor(TFT_RED, TFT_BLACK);
	tft->drawString("TEST (legacy)", 20, 60);
	tft->setTextColor(TFT_WHITE, TFT_BLACK);
	tft->drawString("SW4 - exit", 20, 120);

	drawSw5MarkerTopForce(tft, 10);
}

