#include "Menu.h"
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <new>
#include <string.h>
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

// GPS globals
extern uint8_t gGpsSatsUsed;
extern uint8_t gGpsSatsView;
extern bool gGpsFix;
extern uint16_t gGpsHdop_x100;
extern uint32_t gGpsLastRmcMs;
extern uint32_t gGpsLastGgaMs;
extern uint32_t gGpsLastGsvMs;

// Net/GSM globals (already declared in project headers)
extern bool gNetRegistered;
extern uint8_t gSignalBars;
extern int8_t gCsqRssi; // 0..31,99=unknown (NetFeed.h)
extern int16_t gRssiDbm; // dBm

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

// HOME rows (meaning)
static const int Y_L1 = 32; // row1:GPS status + [T]/[X]
static const int Y_L2 = 52; // row2:IP1 + DHCP + [T]/[X]
static const int Y_L3 = 72; // row3:IP2 + DHCP + [T]/[X]
static const int Y_L4 = 94; // row4:GSM status + [T]/[X]
static const int Y_FOOT = 140; // footer:date (left) + version (right)

// --- NTP upstream list (только IP,индекс 0..4) ---
static const char* kNtpIpStr[5] = {
	"162.159.200.123",
	"162.159.200.1",
	"216.239.35.0",
	"216.239.35.0",
	"216.239.35.0"
};

// --- Period names (idx 0..5) ---
static const char* kPeriodsName[6] = { "1 мин","10 мин","30 мин","1 час","6 часов","12 часов" };

// --- GSM provider names (только 3) ---
static const char* kGsmProviders[3] = { "МТС","МЕГАФОН","БИЛАЙН" };

// Таблица строк для Page1->MODE_LIST (page=0,row=item)
const char* menu[5][5] = {
	{"GPS","ИНТЕРНЕТ1","ИНТЕРНЕТ2","GSM"," "},
	{"СВОБОДНО","СВОБОДНО","СВОБОДНО","СВОБОДНО","СВОБОДНО"},
	{"СВОБОДНО","СВОБОДНО","СВОБОДНО","СВОБОДНО","СВОБОДНО"},
	{"СВОБОДНО","СВОБОДНО","СВОБОДНО","СВОБОДНО","СВОБОДНО"},
	{"СВОБОДНО","СВОБОДНО","СВОБОДНО","СВОБОДНО","СВОБОДНО"}
};

static constexpr uint8_t PAGE1_LIST_COUNT = 4;

// -------------------- menu inactivity + save lock --------------------
static constexpr uint32_t MENU_INACTIVITY_MS = 10UL * 60UL * 1000UL; // 10 minutes
static uint32_t gMenuLastInputMs = 0;
static bool gMenuLastInputInit = false;

static inline void menuMarkUserActivity()
{
	gMenuLastInputMs = millis();
	gMenuLastInputInit = true;
}

// SW5 = PA0,active low (0 = запрет сохранения)
static inline bool menuSaveAllowed()
{
	// Требуется pinMode(PA0,INPUT_PULLUP) в setup()
	return (digitalRead(PA0) != LOW);
}

// -------------------- Top-line [O]/[X] marker for menu pages --------------------
static inline void drawSaveMarkerTop(TFT_eSPI* d, int y)
{
	if (!d) return;
	d->loadFont(AA_FONT_TIME18);

	const bool allowed = menuSaveAllowed(); // SW5==0 -> false
	const char* mark = allowed ? "[O]" : "[X]"; // SW5==0 -> [X]
	const uint16_t col = allowed ? TFT_GREEN : TFT_RED;

	const int w = (int)d->textWidth(mark);
	const int x = 320 - w - 3;

	// clear only right zone
	d->fillRect(240, y, 80, 16, TFT_BLACK);

	d->setTextColor(col, TFT_BLACK);
	d->drawString(mark, x, y);
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

// "Если за период + 30% данные не поступили -> [X],иначе [T]"
static inline bool isFreshByPeriod(uint32_t lastMs, uint32_t periodMs, uint32_t nowMs)
{
	if (lastMs == 0) return false;
	if (periodMs == 0) return false;
	uint32_t limit = (periodMs * 13UL) / 10UL; // +30%
	return (nowMs - lastMs) <= limit;
}

// -------------------- HOME Sprite (dynamic lines) --------------------
// IMPORTANT:font height ~12px,sprite height must be 12+2 = 14px to avoid clearing neighbor lines.
static constexpr int HOME_SPR_H = 14;
static TFT_eSprite* gHomeLineSpr = nullptr; // 320x14
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

static inline void homeLineBegin()
{
	if (!gHomeLineSpr) return;
	gHomeLineSpr->fillSprite(TFT_BLACK);
}

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

static inline void homeLinePush(int y)
{
	if (!gHomeLineSpr) return;
	gHomeLineSpr->pushSprite(0, y);
}

// -------------------- HOME caches & RX timestamps --------------------
static char gHomeLastLine1[160] = { 0 };
static char gHomeLastLine2[180] = { 0 };
static char gHomeLastLine3[180] = { 0 };
static char gHomeLastLine4[128] = { 0 };
static char gHomeLastFooterDate[48] = { 0 };
static char gHomeLastFooterVer[64] = { 0 };
static uint8_t gHomeLastSaveAllowed = 2; // 0/1 valid,2=init

static LanIfStore::IfConfig gHomeIf1Cfg{};
static LanIfStore::IfConfig gHomeIf2Cfg{};
static uint32_t gHomeIfCfgMs = 0;

// last "data arrived" timestamps (ms)
static uint32_t gHomeLastIp1RxMs = 0;
static uint32_t gHomeLastIp2RxMs = 0;
static uint32_t gHomeLastGsmRxMs = 0;

// for GSM change detection (if value remains same,we cannot detect new packets)
static int8_t gHomePrevCsq = -127;
static uint8_t gHomePrevBars = 255;
static bool gHomePrevReg = false;

// -------------------- Base for pages --------------------
class PageBase :public IMenuPage {
public:
	explicit PageBase(Menu* m) :_m(m) {}
protected:
	Menu* _m = nullptr;
	TFT_eSPI* tft() const { return _m ? _m->display() : nullptr; }
};

static void ipToOctets(const IPAddress& ip, uint8_t out[4])
{
	out[0] = ip[0];
	out[1] = ip[1];
	out[2] = ip[2];
	out[3] = ip[3];
}

// -------------------- Helpers for GPS info --------------------
// HOME line1:HDOP removed; FIX/NOFIX colored later
static void formatGpsHomeSatsPart(char* out, size_t n)
{
	snprintf(out, n, " Спутники:%u/%u", (unsigned)gGpsSatsUsed, (unsigned)gGpsSatsView);
}

// Page1->GPS info:readable,no GPS signal level "Sig"
static void formatGpsMenuLine1(char* out, size_t n)
{
	double hdop = (double)gGpsHdop_x100 / 100.0;
	snprintf(out, n, "Спутники:%u/%u FIX:%s HDOP:%.2f",
		(unsigned)gGpsSatsUsed,
		(unsigned)gGpsSatsView,
		gGpsFix ? "Да" : "Нет",
		hdop);
}

static void formatGpsMenuLine2(char* out, size_t n)
{
	uint32_t now = millis();
	uint32_t ageR = (gGpsLastRmcMs == 0) ? 99999UL : (now - gGpsLastRmcMs);
	uint32_t ageG = (gGpsLastGgaMs == 0) ? 99999UL : (now - gGpsLastGgaMs);
	uint32_t ageV = (gGpsLastGsvMs == 0) ? 99999UL : (now - gGpsLastGsvMs);

	auto toTenth = [](uint32_t ms)->uint32_t { return (ms + 50) / 100; };
	uint32_t r = toTenth(ageR);
	uint32_t g = toTenth(ageG);
	uint32_t v = toTenth(ageV);

	snprintf(out, n, "Обновлено:RMC %u.%uс GGA %u.%uс GSV %u.%uс",
		(unsigned)(r / 10), (unsigned)(r % 10),
		(unsigned)(g / 10), (unsigned)(g % 10),
		(unsigned)(v / 10), (unsigned)(v % 10));
}

// -------------------- Page1:ИСТОЧНИКИ СИНХРОНИЗАЦИИ --------------------
class Page1 :public PageBase {
public:
	using PageBase::PageBase;

	void setup() override {}

	void onActivate() override {
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
		_lastGpsLine2[0] = 0;
		_lastGpsInfoMs = 0;

		draw();
	}

	void update() override {
		if (_mode == MODE_GPS) {
			uint32_t now = millis();
			if (now - _lastGpsInfoMs >= 500) {
				_lastGpsInfoMs = now;
				drawGpsInfo(false);
			}
		}
		// keep [O]/[X] updated on title line if user toggles SW5 while page is open
		if (_staticDrawn) {
			drawSaveMarkerTop(tft(), Y_TITLE);
		}
	}

	void draw() override {
		if (!_staticDrawn) {
			drawStatic();
			_staticDrawn = true;
			_lastSel = 255;
			_lastParam = 255;
			_lastHash = 0;
		}
		drawDynamic(true);
	}

	void onButtonPressed(int buttonID) override {
		// LIST
		if (_mode == MODE_LIST) {
			if (buttonID == BTN_SW2) { _sel = (uint8_t)((_sel + (PAGE1_LIST_COUNT - 1)) % PAGE1_LIST_COUNT); drawDynamic(false); return; }
			if (buttonID == BTN_SW3) { _sel = (uint8_t)((_sel + 1) % PAGE1_LIST_COUNT); drawDynamic(false); return; }
			if (buttonID == BTN_SW1) { // ENTER
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

		if (buttonID == BTN_SW4) { // SAVE + BACK
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
	char _lastGpsLine2[96] = { 0 };

	uint8_t paramCount() const {
		if (_mode == MODE_GPS) return 4;
		if (_mode == MODE_NET) return 5;
		if (_mode == MODE_NET2) return 4;
		if (_mode == MODE_GSM) return 5;
		return 0;
	}

	static void clampTz(int8_t& tz) { if (tz < -12) tz = -12; if (tz > 14) tz = 14; }
	static void formatTz(char* out, size_t n, int8_t tzHours) { if (tzHours == 0) snprintf(out, n, "UTC"); else snprintf(out, n, "UTC%+d", (int)tzHours); }

	void drawStatic() {
		TFT_eSPI* d = tft(); if (!d) return;
		d->loadFont(AA_FONT_TIME18);

		d->fillScreen(TFT_BLACK);
		d->setTextColor(TFT_YELLOW, TFT_BLACK);

		if (_mode == MODE_LIST) d->drawString("ИСТОЧНИКИ СИНХРОНИЗАЦИИ", X, Y_TITLE);
		else if (_mode == MODE_GPS) d->drawString("GPS", X, Y_TITLE);
		else if (_mode == MODE_NET) d->drawString("ИНТЕРНЕТ1", X, Y_TITLE);
		else if (_mode == MODE_NET2) d->drawString("ИНТЕРНЕТ2", X, Y_TITLE);
		else if (_mode == MODE_GSM) d->drawString("GSM", X, Y_TITLE);

		drawSaveMarkerTop(d, Y_TITLE);

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

	void drawDynamic(bool force) {
		TFT_eSPI* d = tft(); if (!d) return;
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

	uint32_t calcHash() const {
		uint32_t h = 2166136261u;
		auto mix = [&](uint32_t v) { h ^= v; h *= 16777619u; };

		if (_mode == MODE_GPS) { mix(_tmp.gpsEnable); mix(_tmp.gpsPeriodIdx); mix((uint8_t)_tzTargetTmp); mix((uint8_t)_tzNtpTmp); }
		else if (_mode == MODE_NET) { mix(_tmp.netEnable); mix(_tmp.netProviderIdx); mix(_tmp.netPeriodIdx); mix((uint8_t)_tzTargetTmp); mix((uint8_t)_tzNtpTmp); }
		else if (_mode == MODE_NET2) { mix(_tmp.net2Enable); mix(_tmp.net2PeriodIdx); mix((uint8_t)_tzTargetTmp); mix((uint8_t)_tzNtpTmp); }
		else if (_mode == MODE_GSM) { mix(_tmp.gsmEnable); mix(_tmp.gsmProviderIdx); mix(_tmp.gsmPeriodIdx); mix((uint8_t)_tzTargetTmp); mix((uint8_t)_tzNtpTmp); }
		mix(_param);
		return h;
	}

	void buildLine(uint8_t i, char* out, size_t n) const {
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

		// MODE_GSM
		if (i == 0) snprintf(out, n, "1) GSM:%s", _tmp.gsmEnable ? "ON" : "OFF");
		else if (i == 1) snprintf(out, n, "2) OP:%s", kGsmProviders[_tmp.gsmProviderIdx % 3]);
		else if (i == 2) snprintf(out, n, "3) ПЕРИОД:%s", kPeriodsName[_tmp.gsmPeriodIdx % 6]);
		else if (i == 3) snprintf(out, n, "4) Часовой пояс:%s", tzBuf);
		else snprintf(out, n, "5) TZ NTP:%d", (int)_tzNtpTmp);
	}

	void changeParam(int delta) {
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

	void commitSave() {
		syncData = _tmp;
		(void)syncStore.save(syncData);

		cfg.tzTargetHours = _tzTargetTmp;
		cfg.tzNtpHours = _tzNtpTmp;
		(void)ee.writeTzTargetHours(cfg.tzTargetHours);
		(void)ee.writeTzNtpHours(cfg.tzNtpHours);

		planner.onSettingsChanged();
	}

	void drawGpsInfo(bool clearLine) {
		TFT_eSPI* d = tft(); if (!d) return;

		int y1 = Y0 + 3 * DY;
		int y2 = Y0 + 4 * DY;

		char line1[80];
		char line2[96];
		formatGpsMenuLine1(line1, sizeof(line1));
		formatGpsMenuLine2(line2, sizeof(line2));

		bool changed1 = (strcmp(_lastGpsLine1, line1) != 0);
		bool changed2 = (strcmp(_lastGpsLine2, line2) != 0);

		d->loadFont(AA_FONT_TIME18);

		if (clearLine || changed1) {
			d->fillRect(0, y1, LINE_W, LINE_H, TFT_BLACK);
			d->setTextColor(TFT_GREEN, TFT_BLACK);
			d->drawString(line1, X, y1);
			strncpy(_lastGpsLine1, line1, sizeof(_lastGpsLine1) - 1);
			_lastGpsLine1[sizeof(_lastGpsLine1) - 1] = 0;
		}

		if (clearLine || changed2) {
			d->fillRect(0, y2, LINE_W, LINE_H, TFT_BLACK);
			d->setTextColor(TFT_CYAN, TFT_BLACK);
			d->drawString(line2, X, y2);
			strncpy(_lastGpsLine2, line2, sizeof(_lastGpsLine2) - 1);
			_lastGpsLine2[sizeof(_lastGpsLine2) - 1] = 0;
		}
	}
};

// -------------------- Internet pages base --------------------
class InternetPageBase :public PageBase {
public:
	using PageBase::PageBase;

protected:
	static const int X = 10, Y_TITLE = 0, Y0 = 22, DY = 20, W = 320, H = 18;

	enum Param :uint8_t { P_DHCP = 0, P_NTP = 1, P_PERIOD = 2, PARAM_COUNT = 3 };

	void drawIpLineRO(TFT_eSPI* d, const char* name, uint8_t v[4], int row)
	{
		d->loadFont(AA_FONT_TIME18);
		d->setTextColor(TFT_WHITE, TFT_BLACK);
		d->drawString(" ", 0, Y0 + row * DY);

		char s[40];
		snprintf(s, sizeof(s), "%s %u.%u.%u.%u", name, v[0], v[1], v[2], v[3]);
		d->drawString(s, X, Y0 + row * DY);
	}
};

// -------------------- Page2:ИНТЕРНЕТ1 --------------------
class Page2 :public InternetPageBase {
public:
	using InternetPageBase::InternetPageBase;

	void setup() override {}

	void onActivate() override {
		_staticDrawn = false;
		_param = 0;
		lanStore.load(LanIfStore::IF1, _cfg, false);

		_statLine[0] = 0;
		_lastStatMs = 0;

		draw();
	}

	void update() override
	{
		uint32_t now = millis();
		if (now - _lastStatMs < 1000) return;
		_lastStatMs = now;

		IPAddress ip = ntpLan.localIP();
		bool ok = !(ip[0] == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] == 0);

		char line[64];
		snprintf(line, sizeof(line), "STATUS :%s", ok ? "OK" : "NO");

		if (strcmp(_statLine, line) != 0)
		{
			strncpy(_statLine, line, sizeof(_statLine) - 1);
			_statLine[sizeof(_statLine) - 1] = 0;
			drawStatusLine();
		}
		else {
			// still allow marker update if SW5 changed
			drawSaveMarkerTop(tft(), Y_TITLE);
		}
	}

	void draw() override
	{
		if (!_staticDrawn) { drawStatic(); _staticDrawn = true; }
		drawDynamic(true);
	}

	void onButtonPressed(int buttonID) override
	{
		if (buttonID == BTN_SW4) { // SAVE+HOME
			if (menuSaveAllowed()) {
				lanStore.save(LanIfStore::IF1, _cfg, true);
				applyInternet1FromStore();
			}
			_m->backToStart();
			return;
		}

		if (buttonID == BTN_SW1) { // NEXT
			_param = (_param + 1) % PARAM_COUNT;
			drawDynamic(false);
			return;
		}

		if (buttonID == BTN_SW2 || buttonID == BTN_SW3)
		{
			int delta = (buttonID == BTN_SW3) ? +1 : -1;
			applyDelta(delta);
			drawDynamic(false);
			return;
		}
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
		d->fillRect(140, 0, 170, 16, TFT_BLACK);

		bool ok = (strstr(_statLine, "STATUS :OK") != nullptr);
		d->setTextColor(ok ? TFT_GREEN : TFT_RED, TFT_BLACK);
		d->drawString(_statLine, 140, Y_TITLE);

		// keep marker visible even when STATUS updates
		drawSaveMarkerTop(d, Y_TITLE);
	}

	void drawStatic()
	{
		auto d = tft(); if (!d) return;

		d->loadFont(AA_FONT_TIME18);
		d->fillScreen(TFT_BLACK);
		d->setTextColor(TFT_YELLOW, TFT_BLACK);
		d->drawString("INTERNET 1", X, Y_TITLE);

		drawStatusLine();
		drawSaveMarkerTop(d, Y_TITLE);
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

	void applyDelta(int delta) {
		if (_param == P_DHCP) { _cfg.dhcp = _cfg.dhcp ? 0 : 1; return; }
		if (_param == P_NTP) { _cfg.ntpIdx = (uint8_t)((_cfg.ntpIdx + (delta > 0 ? 1 : 4)) % 5); return; }
		if (_param == P_PERIOD) { _cfg.periodIdx = (uint8_t)((_cfg.periodIdx + (delta > 0 ? 1 : 5)) % 6); return; }
	}
};

// -------------------- Page3:ИНТЕРНЕТ2 --------------------
class Page3 :public InternetPageBase {
public:
	using InternetPageBase::InternetPageBase;

	void setup() override {}

	void onActivate() override {
		_staticDrawn = false;
		_param = 0;
		lanStore.load(LanIfStore::IF2, _cfg, false);

		_statLine[0] = 0;
		_lastStatMs = 0;

		draw();
	}

	void update() override
	{
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
		else {
			drawSaveMarkerTop(tft(), Y_TITLE);
		}
	}

	void draw() override {
		if (!_staticDrawn) { drawStatic(); _staticDrawn = true; }
		drawDynamic(true);
	}

	void onButtonPressed(int buttonID) override {
		if (buttonID == BTN_SW4) { // SAVE+HOME
			if (menuSaveAllowed()) {
				lanStore.save(LanIfStore::IF2, _cfg, true);
				applyInternet2FromStore();
			}
			_m->backToStart();
			return;
		}

		if (buttonID == BTN_SW1) {
			_param = (_param + 1) % PARAM_COUNT;
			drawDynamic(false);
			return;
		}

		if (buttonID == BTN_SW2 || buttonID == BTN_SW3) {
			int delta = (buttonID == BTN_SW3) ? +1 : -1;
			applyDelta(delta);
			drawDynamic(false);
			return;
		}
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
		d->fillRect(140, 0, 170, 16, TFT_BLACK);

		bool ok = (strstr(_statLine, "STATUS :OK") != nullptr);
		d->setTextColor(ok ? TFT_GREEN : TFT_RED, TFT_BLACK);
		d->drawString(_statLine, 140, Y_TITLE);

		// keep marker visible even when STATUS updates
		drawSaveMarkerTop(d, Y_TITLE);
	}

	void drawStatic()
	{
		auto d = tft(); if (!d) return;

		d->loadFont(AA_FONT_TIME18);
		d->fillScreen(TFT_BLACK);
		d->setTextColor(TFT_YELLOW, TFT_BLACK);
		d->drawString("INTERNET 2", X, Y_TITLE);

		drawStatusLine();
		drawSaveMarkerTop(d, Y_TITLE);
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

	void applyDelta(int delta) {
		if (_param == P_DHCP) { _cfg.dhcp = _cfg.dhcp ? 0 : 1; return; }
		if (_param == P_NTP) { _cfg.ntpIdx = (uint8_t)((_cfg.ntpIdx + (delta > 0 ? 1 : 4)) % 5); return; }
		if (_param == P_PERIOD) { _cfg.periodIdx = (uint8_t)((_cfg.periodIdx + (delta > 0 ? 1 : 5)) % 6); return; }
	}
};

// -------------------- Page4/5 stubs --------------------
class Page4 :public PageBase {
public:
	using PageBase::PageBase;
	void setup() override {}
	void onActivate() override { draw(); }
	void update() override { drawSaveMarkerTop(tft(), 0); }
	void draw() override {
		auto d = tft(); if (!d) return;
		d->loadFont(AA_FONT_TIME18);
		d->fillScreen(TFT_BLACK);
		d->setTextColor(TFT_YELLOW, TFT_BLACK);
		d->drawString("GSM", 10, 0);
		drawSaveMarkerTop(d, 0);
	}
	void onButtonPressed(int) override {}
	void onButtonReleased(int) override {}
};

class Page5 :public PageBase {
public:
	using PageBase::PageBase;
	void setup() override {}
	void onActivate() override { draw(); }
	void update() override { drawSaveMarkerTop(tft(), 0); }
	void draw() override {
		auto d = tft(); if (!d) return;
		d->loadFont(AA_FONT_TIME18);
		d->fillScreen(TFT_BLACK);
		d->setTextColor(TFT_YELLOW, TFT_BLACK);
		d->drawString("PAGE 5", 10, 0);
		drawSaveMarkerTop(d, 0);
	}
	void onButtonPressed(int) override {}
	void onButtonReleased(int) override {}
};

// ---------------------------------------------------------

Menu::Menu(TFT_eSPI* disp) :tft(disp) {}

void Menu::attachPages()
{
	static Page1 p1(this);
	static Page2 p2(this);
	static Page3 p3(this);
	static Page4 p4(this);
	static Page5 p5(this);

	pages[0] = &p1; // SOURCES
	pages[1] = &p2; // INTERNET1
	pages[2] = &p3; // INTERNET2
	pages[3] = &p4;
	pages[4] = &p5;
}

void Menu::setup(const String& ver)
{
	versionString = ver;

	tft->init();
	tft->setRotation(3);
	tft->fillScreen(TFT_NAVY);
	tft->setTextColor(TFT_WHITE, TFT_BLACK);

	// стартовый экран
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

	drawStartPage();
}

void Menu::update()
{
	const uint32_t now = millis();

	// автовозврат на HOME если в меню и нет нажатий 10 минут
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

	if (state == MENU_ITEM_SELECT) {
		if (pages[page]) pages[page]->update();
	}
}

void Menu::invalidateHome() { _homeDirty = true; }

void Menu::drawStartPage()
{
	state = MENU_IDLE;
	_homeStaticDrawn = false;
	_homeDirty = true;
}

// ---- HOME helpers ----
static void homeDrawSaveMarker(TFT_eSPI* tft)
{
	if (!tft) return;
	tft->loadFont(AA_FONT_TIME18);

	bool allowed = menuSaveAllowed();
	uint8_t a = allowed ? 1 : 0;
	if (gHomeLastSaveAllowed == a) return;
	gHomeLastSaveAllowed = a;

	const char* mark = allowed ? "[O]" : "[X]";
	uint16_t col = allowed ? TFT_GREEN : TFT_RED;

	int w = (int)tft->textWidth(mark);
	int x = 320 - w - 3;

	tft->fillRect(240, Y_HOME, 80, 16, TFT_BLACK);
	tft->setTextColor(col, TFT_BLACK);
	tft->drawString(mark, x, Y_HOME);
}

static void homeMaybeReloadLanCfg()
{
	uint32_t now = millis();
	if (now - gHomeIfCfgMs < 2000) return; // every 2s
	gHomeIfCfgMs = now;

	lanStore.load(LanIfStore::IF1, gHomeIf1Cfg, false);
	lanStore.load(LanIfStore::IF2, gHomeIf2Cfg, false);
}

static void homeUpdateRxStamps(uint32_t now)
{
	// IP1/IP2:timestamp updates when IP is not 0.0.0.0
	if (!isZeroIpStr(gIp1Str)) gHomeLastIp1RxMs = now;
	if (!isZeroIpStr(gIp2Str)) gHomeLastIp2RxMs = now;

	// GSM:detect any change in CSQ/bars/registration as "data arrived"
	if (gHomePrevCsq == -127) {
		gHomePrevCsq = gCsqRssi;
		gHomePrevBars = gSignalBars;
		gHomePrevReg = gNetRegistered;
		gHomeLastGsmRxMs = now; // first init
		return;
	}

	if (gCsqRssi != gHomePrevCsq || gSignalBars != gHomePrevBars || gNetRegistered != gHomePrevReg) {
		gHomePrevCsq = gCsqRssi;
		gHomePrevBars = gSignalBars;
		gHomePrevReg = gNetRegistered;
		gHomeLastGsmRxMs = now;
	}
}

static void homeDrawRightTX(bool ok)
{
	const char* ico = ok ? "[T]" : "[X]";
	uint16_t icoCol = ok ? TFT_GREEN : TFT_RED;

	int icoW = homeTextW(ico);
	int icoX = 320 - icoW - 4;
	homeLineText(icoX, icoCol, ico);
}

static void homeDrawLineGPS(int y, bool ok)
{
	// FIX/NOFIX must be colored (green/red). HDOP removed on HOME.
	const char* fixStr = gGpsFix ? "FIX" : "NOFIX";
	const uint16_t fixCol = gGpsFix ? TFT_GREEN : TFT_RED;

	char sats[64];
	formatGpsHomeSatsPart(sats, sizeof(sats));

	// cache key includes fix + sats + ok
	char cache[160];
	snprintf(cache, sizeof(cache), "GPS:%s%s|%u", fixStr, sats, (unsigned)ok);
	if (strcmp(gHomeLastLine1, cache) == 0) return;
	strncpy(gHomeLastLine1, cache, sizeof(gHomeLastLine1) - 1);
	gHomeLastLine1[sizeof(gHomeLastLine1) - 1] = 0;

	homeLineBegin();

	int x = 10;
	homeLineText(x, TFT_WHITE, "GPS:");
	x += homeTextW("GPS:");

	homeLineText(x, fixCol, fixStr);
	x += homeTextW(fixStr);

	homeLineText(x, TFT_WHITE, sats);

	homeDrawRightTX(ok);
	homeLinePush(y);
}

static void homeDrawLineIP(int y, const char* tag, const char* ipStr, bool dhcpEnabled, bool ok, char* last, size_t lastN)
{
	char cache[200];
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

	// IP string:"НЕ ОПРЕДЕЛЕН" опускаем на 1px
	if (ipUnknown) homeLineTextXY(x, 1, ipCol, ipShow);
	else homeLineText(x, ipCol, ipShow);
	x += homeTextW(ipShow) + 8;

	// DHCP segment with colored OK/NO
	if (!dhcpEnabled) {
		homeLineText(x, TFT_CYAN, "STATIC");
	}
	else {
		homeLineText(x, TFT_CYAN, "DHCP:");
		x += homeTextW("DHCP:");

		const char* st = ipUnknown ? "NO" : "OK";
		const uint16_t stCol = ipUnknown ? TFT_RED : TFT_GREEN;

		// "NO" при неопределенном IP опускаем на 1px
		if (ipUnknown) homeLineTextXY(x, 1, stCol, st);
		else homeLineText(x, stCol, st);
	}

	// Right icon [T]/[X],and [X] on IP lines must be lower by 1px
	const char* ico = ok ? "[T]" : "[X]";
	const uint16_t icoCol = ok ? TFT_GREEN : TFT_RED;
	const int icoW = homeTextW(ico);
	const int icoX = 320 - icoW - 4;

	if (!ok) homeLineTextXY(icoX, 1, icoCol, ico); // [X] вниз на 1px
	else homeLineText(icoX, icoCol, ico);

	homeLinePush(y);
}

static void homeDrawLineGSM(int y, bool ok)
{
	// cache key:status text + ok
	const char* st = gNetRegistered ? "CONNECTED" : "NO";

	char cache[128];
	snprintf(cache, sizeof(cache), "GSM:%s|%u", st, (unsigned)ok);

	if (strcmp(gHomeLastLine4, cache) == 0) return;
	strncpy(gHomeLastLine4, cache, sizeof(gHomeLastLine4) - 1);
	gHomeLastLine4[sizeof(gHomeLastLine4) - 1] = 0;

	homeLineBegin();

	int x = 10;
	homeLineText(x, TFT_WHITE, "GSM:");
	x += homeTextW("GSM:");

	homeLineText(x, gNetRegistered ? TFT_GREEN : TFT_RED, st);

	homeDrawRightTX(ok);
	homeLinePush(y);
}

static void homeDrawFooter(int y, const String& ver)
{
	const char* date = gDateStr; // from TimeFeed.h

	if (strcmp(gHomeLastFooterDate, date) == 0 && strcmp(gHomeLastFooterVer, ver.c_str()) == 0) return;

	strncpy(gHomeLastFooterDate, date, sizeof(gHomeLastFooterDate) - 1);
	gHomeLastFooterDate[sizeof(gHomeLastFooterDate) - 1] = 0;

	strncpy(gHomeLastFooterVer, ver.c_str(), sizeof(gHomeLastFooterVer) - 1);
	gHomeLastFooterVer[sizeof(gHomeLastFooterVer) - 1] = 0;

	homeLineBegin();

	homeLineText(0, TFT_WHITE, gHomeLastFooterDate);

	int verW = homeTextW(gHomeLastFooterVer);
	int xVer = 320 - verW - 2;
	homeLineText(xVer, TFT_YELLOW, gHomeLastFooterVer);

	homeLinePush(y);
}

void Menu::drawHomeStatic()
{
	tft->loadFont(AA_FONT_TIME18);
	tft->fillScreen(TFT_BLACK);

	// Title
	tft->setTextColor(TFT_CYAN, TFT_BLACK);
	tft->drawString("СОСТОЯНИЕ СИСТЕМЫ", 10, Y_HOME);

	// Right area for save marker
	tft->fillRect(240, Y_HOME, 80, 16, TFT_BLACK);

	// reset caches
	gHomeLastLine1[0] = 0;
	gHomeLastLine2[0] = 0;
	gHomeLastLine3[0] = 0;
	gHomeLastLine4[0] = 0;
	gHomeLastFooterDate[0] = 0;
	gHomeLastFooterVer[0] = 0;
	gHomeLastSaveAllowed = 2;

	// reset rx stamps (so [X] until first data)
	gHomeLastIp1RxMs = 0;
	gHomeLastIp2RxMs = 0;
	gHomeLastGsmRxMs = 0;
	gHomePrevCsq = -127;
	gHomePrevBars = 255;
	gHomePrevReg = false;

	gHomeIfCfgMs = 0;

	homeSpritesEnsure(tft);
}

void Menu::drawHomeDynamic()
{
	homeSpritesEnsure(tft);

	const uint32_t now = millis();

	// save marker [O]/[X] on title line
	homeDrawSaveMarker(tft);

	// refresh DHCP configs
	homeMaybeReloadLanCfg();

	// update "data arrived" stamps
	homeUpdateRxStamps(now);

	// compute freshness by period+30%
	const bool gpsOk = (syncData.gpsEnable != 0) &&
		isFreshByPeriod(gGpsLastRmcMs, periodIdxToMs(syncData.gpsPeriodIdx), now);

	const bool ip1Ok = (syncData.netEnable != 0) &&
		isFreshByPeriod(gHomeLastIp1RxMs, periodIdxToMs(syncData.netPeriodIdx), now);

	const bool ip2Ok = (syncData.net2Enable != 0) &&
		isFreshByPeriod(gHomeLastIp2RxMs, periodIdxToMs(syncData.net2PeriodIdx), now);

	const bool gsmOk = (syncData.gsmEnable != 0) &&
		isFreshByPeriod(gHomeLastGsmRxMs, periodIdxToMs(syncData.gsmPeriodIdx), now);

	// Line1 GPS + [T]/[X] (FIX/NOFIX colored,HDOP removed)
	homeDrawLineGPS(Y_L1, gpsOk);

	// Line2 IP1 + DHCP + [T]/[X]
	homeDrawLineIP(Y_L2, "IP1", gIp1Str, (gHomeIf1Cfg.dhcp != 0), ip1Ok, gHomeLastLine2, sizeof(gHomeLastLine2));

	// Line3 IP2 + DHCP + [T]/[X]
	homeDrawLineIP(Y_L3, "IP2", gIp2Str, (gHomeIf2Cfg.dhcp != 0), ip2Ok, gHomeLastLine3, sizeof(gHomeLastLine3));

	// Line4 GSM + [T]/[X] (text forced WHITE)
	homeDrawLineGSM(Y_L4, gsmOk);

	// Footer:date (left) + version (right)
	homeDrawFooter(Y_FOOT, versionString);
}

void Menu::activate()
{
	// HOME sprites are used only on HOME; free them when entering menu to save RAM
	homeSpritesFree();

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
		"СВОБОДНО"
	};

	tft->loadFont(AA_FONT_TIME18);
	tft->fillScreen(TFT_BLACK);

	tft->setTextColor(TFT_CYAN, TFT_BLACK);
	tft->drawString("ВЫБОР МЕНЮ", 10, 10);

	// [O]/[X] on top line depending on SW5 (like HOME)
	drawSaveMarkerTop(tft, 10);

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

	// HOME:вход в меню по SW1
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

	// show marker also here (optional but consistent "all pages")
	drawSaveMarkerTop(tft, 10);
}
