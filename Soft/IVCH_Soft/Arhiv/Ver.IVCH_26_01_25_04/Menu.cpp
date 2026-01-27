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
extern uint8_t gGpsSnrAvg;
extern uint8_t gGpsSnrMax;
extern uint32_t gGpsLastRmcMs;
extern uint32_t gGpsLastGgaMs;
extern uint32_t gGpsLastGsvMs;

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

static const int Y_IP1 = 32;
static const int Y_IP2 = 52;

static const int Y_DATE = 72;
static const int Y_NET = 94;
// SIG removed from HOME (moved to GSM settings page)
static const int Y_SIG = 116;

static const int Y_VER = 140;

// -------------------- HOME Sprite (dynamic fields only) --------------------
// Small reusable sprite for value area (x=80,w=240). Height increased to avoid clipping AA_FONT_TIME18.
static TFT_eSprite* gHomeSpr240 = nullptr; // 240x28
static TFT_eSPI* gHomeSprOwner = nullptr;

static void homeSpritesFree()
{
	if (gHomeSpr240) {
		// unloadFont() is required for smooth fonts; project already uses loadFont().
		gHomeSpr240->unloadFont();
		gHomeSpr240->deleteSprite();
		delete gHomeSpr240;
		gHomeSpr240 = nullptr;
	}
	gHomeSprOwner = nullptr;
}

static void homeSpritesEnsure(TFT_eSPI* tft)
{
	if (!tft) return;

	// If TFT instance changed,recreate sprites bound to it
	if (gHomeSprOwner && gHomeSprOwner != tft) {
		homeSpritesFree();
	}
	if (!gHomeSprOwner) gHomeSprOwner = tft;

	if (!gHomeSpr240) {
		gHomeSpr240 = new (std::nothrow) TFT_eSprite(tft);
		if (gHomeSpr240) {
			gHomeSpr240->setColorDepth(16);
			if (gHomeSpr240->createSprite(240, 28) == nullptr) {
				delete gHomeSpr240;
				gHomeSpr240 = nullptr;
			}
			else {
				gHomeSpr240->loadFont(AA_FONT_TIME18);
				gHomeSpr240->setTextColor(TFT_WHITE, TFT_BLACK);
			}
		}
	}
}

static inline void homePushText(TFT_eSprite* spr, int x, int y, uint16_t fg, const char* txt)
{
	if (!spr) return;
	spr->fillSprite(TFT_BLACK);
	spr->setTextColor(fg, TFT_BLACK);
	spr->drawString(txt, 0, 0);
	spr->pushSprite(x, y);
}

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

// -------------------- NEW:menu inactivity + save lock --------------------
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
static void formatGpsLine1(char* out, size_t n)
{
	snprintf(out, n, "Sat:%u/%u Fix:%s HDOP:%.2f",
		(unsigned)gGpsSatsUsed,
		(unsigned)gGpsSatsView,
		gGpsFix ? "Y" : "N",
		(double)gGpsHdop_x100 / 100.0);
}

static void formatGpsLine2(char* out, size_t n)
{
	uint32_t now = millis();
	uint32_t ageR = (gGpsLastRmcMs == 0) ? 99999UL : (now - gGpsLastRmcMs);
	uint32_t ageG = (gGpsLastGgaMs == 0) ? 99999UL : (now - gGpsLastGgaMs);
	uint32_t ageV = (gGpsLastGsvMs == 0) ? 99999UL : (now - gGpsLastGsvMs);

	auto toTenth = [](uint32_t ms)->uint32_t { return (ms + 50) / 100; };
	uint32_t r = toTenth(ageR);
	uint32_t g = toTenth(ageG);
	uint32_t v = toTenth(ageV);

	snprintf(out, n, "Sig:%u/%u R:%u.%us G:%u.%us V:%u.%us",
		(unsigned)gGpsSnrAvg,
		(unsigned)gGpsSnrMax,
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

		// ---- LIST ----
		if (_mode == MODE_LIST) {
			if (buttonID == BTN_SW2) { // UP
				_sel = (uint8_t)((_sel + (PAGE1_LIST_COUNT - 1)) % PAGE1_LIST_COUNT);
				drawDynamic(false);
				return;
			}
			if (buttonID == BTN_SW3) { // DOWN
				_sel = (uint8_t)((_sel + 1) % PAGE1_LIST_COUNT);
				drawDynamic(false);
				return;
			}
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
			if (buttonID == BTN_SW4) { // HOME
				_m->backToStart();
				return;
			}
			return;
		}

		// ---- SUBMENU ----
		if (buttonID == BTN_SW1)
		{ // NEXT PARAM
			_param = (_param + 1) % paramCount();
			drawDynamic(false);
			return;
		}

		if (buttonID == BTN_SW2) { // "-"
			changeParam(-1);
			drawDynamic(false);
			return;
		}

		if (buttonID == BTN_SW3) { // "+"
			changeParam(+1);
			drawDynamic(false);
			return;
		}

		if (buttonID == BTN_SW4) { // SAVE + BACK (но может быть запрещено)
			if (menuSaveAllowed()) {
				commitSave();
			}
			// если запрещено — просто выходим назад без записи
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

	SyncSourcesStore::Data _tmp;

	int8_t _tzTargetTmp = 0;
	int8_t _tzNtpTmp = 0;

	uint8_t _lastSel = 255;
	uint8_t _lastParam = 255;
	uint32_t _lastHash = 0;

	uint32_t _lastGpsInfoMs = 0;
	char _lastGpsLine1[64] = { 0 };
	char _lastGpsLine2[96] = { 0 };

	uint8_t paramCount() const {
		if (_mode == MODE_GPS) return 4;
		if (_mode == MODE_NET) return 5;
		if (_mode == MODE_NET2) return 4;
		if (_mode == MODE_GSM) return 5;
		return 0;
	}

	static void clampTz(int8_t& tz) {
		if (tz < -12) tz = -12;
		if (tz > 14) tz = 14;
	}

	static void formatTz(char* out, size_t n, int8_t tzHours) {
		if (tzHours == 0) snprintf(out, n, "UTC");
		else snprintf(out, n, "UTC%+d", (int)tzHours);
	}

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

		if (_mode == MODE_GPS) {
			mix(_tmp.gpsEnable); mix(_tmp.gpsPeriodIdx);
			mix((uint8_t)_tzTargetTmp); mix((uint8_t)_tzNtpTmp);
		}
		else if (_mode == MODE_NET) {
			mix(_tmp.netEnable); mix(_tmp.netProviderIdx); mix(_tmp.netPeriodIdx);
			mix((uint8_t)_tzTargetTmp); mix((uint8_t)_tzNtpTmp);
		}
		else if (_mode == MODE_NET2) {
			mix(_tmp.net2Enable); mix(_tmp.net2PeriodIdx);
			mix((uint8_t)_tzTargetTmp); mix((uint8_t)_tzNtpTmp);
		}
		else if (_mode == MODE_GSM) {
			mix(_tmp.gsmEnable); mix(_tmp.gsmProviderIdx); mix(_tmp.gsmPeriodIdx);
			mix((uint8_t)_tzTargetTmp); mix((uint8_t)_tzNtpTmp);
		}
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
		// enable-переключатели:каждое нажатие инвертирует
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

		char line1[64];
		char line2[96];
		formatGpsLine1(line1, sizeof(line1));
		formatGpsLine2(line2, sizeof(line2));

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
	static const int Y_HINT1 = 132, Y_HINT2 = 148;

	enum Param :uint8_t {
		P_DHCP = 0,
		P_NTP = 1,
		P_PERIOD = 2,
		PARAM_COUNT = 3
	};

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
	}

	void draw() override
	{
		if (!_staticDrawn) { drawStatic(); _staticDrawn = true; }
		drawDynamic(true);
	}

	void onButtonPressed(int buttonID) override
	{
		if (buttonID == BTN_SW4) { // SAVE+HOME (но может быть запрещено)
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
	}

	void drawStatic()
	{
		auto d = tft(); if (!d) return;
		d->loadFont(AA_FONT_TIME18);
		d->fillScreen(TFT_BLACK);
		d->setTextColor(TFT_YELLOW, TFT_BLACK);
		d->drawString("INTERNET 1", X, Y_TITLE);

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
	}

	void draw() override {
		if (!_staticDrawn) { drawStatic(); _staticDrawn = true; }
		drawDynamic(true);
	}

	void onButtonPressed(int buttonID) override {
		if (buttonID == BTN_SW4) { // SAVE+HOME (но может быть запрещено)
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
	}

	void drawStatic()
	{
		auto d = tft(); if (!d) return;
		d->loadFont(AA_FONT_TIME18);
		d->fillScreen(TFT_BLACK);
		d->setTextColor(TFT_YELLOW, TFT_BLACK);
		d->drawString("INTERNET 2", X, Y_TITLE);

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

	void applyDelta(int delta) {
		if (_param == P_DHCP) { _cfg.dhcp = _cfg.dhcp ? 0 : 1; return; }
		if (_param == P_NTP) { _cfg.ntpIdx = (uint8_t)((_cfg.ntpIdx + (delta > 0 ? 1 : 4)) % 5); return; }
		if (_param == P_PERIOD) { _cfg.periodIdx = (uint8_t)((_cfg.periodIdx + (delta > 0 ? 1 : 5)) % 6); return; }
	}
};

// -------------------- Page4:GSM SETTINGS --------------------
class Page4 :public PageBase {
public:
	using PageBase::PageBase;

	void setup() override {}

	void onActivate() override
	{
		_param = 0;
		_tmp = syncData;
		_tzTargetTmp = cfg.tzTargetHours;
		_tzNtpTmp = cfg.tzNtpHours;

		_staticDrawn = false;
		_lastParam = 255;
		_lastHash = 0;
		_lastSig[0] = 0;
		_lastSigMs = 0;

		draw();
	}

	void update() override
	{
		// update SIG line periodically
		uint32_t now = millis();
		if (now - _lastSigMs < 500) return;
		_lastSigMs = now;
		drawSig(false);
	}

	void draw() override
	{
		if (!_staticDrawn) {
			drawStatic();
			_staticDrawn = true;
			_lastParam = 255;
			_lastHash = 0;
			_lastSig[0] = 0;
		}
		drawDynamic(true);
	}

	void onButtonPressed(int buttonID) override
	{
		if (buttonID == BTN_SW4) { // SAVE+HOME (но может be запрещено)
			if (menuSaveAllowed()) {
				commitSave();
			}
			_m->backToStart();
			return;
		}

		if (buttonID == BTN_SW1) { // NEXT
			_param = (uint8_t)((_param + 1) % PARAM_COUNT);
			drawDynamic(false);
			return;
		}

		if (buttonID == BTN_SW2 || buttonID == BTN_SW3) {
			int delta = (buttonID == BTN_SW3) ? +1 : -1;
			changeParam(delta);
			drawDynamic(false);
			return;
		}
	}

	void onButtonReleased(int) override {}

private:
	// layout
	static const int X = 10;
	static const int Y_TITLE = 0;
	static const int Y0 = 22;
	static const int DY = 20;
	static const int W = 320;
	static const int H = 18;

	static const int Y_SIG_LINE = 122;
	static const int Y_HINT1 = 142;
	static const int Y_HINT2 = 156;

	enum :uint8_t {
		P_ENABLE = 0,
		P_PROVIDER = 1,
		P_PERIOD = 2,
		P_TZ_TARGET = 3,
		P_TZ_NTP = 4,
		PARAM_COUNT = 5
	};

	bool _staticDrawn = false;
	uint8_t _param = 0;

	SyncSourcesStore::Data _tmp{};
	int8_t _tzTargetTmp = 0;
	int8_t _tzNtpTmp = 0;

	uint8_t _lastParam = 255;
	uint32_t _lastHash = 0;

	uint32_t _lastSigMs = 0;
	char _lastSig[48] = { 0 };

	static void clampTz(int8_t& tz) {
		if (tz < -12) tz = -12;
		if (tz > 14) tz = 14;
	}

	static void formatTz(char* out, size_t n, int8_t tzHours) {
		if (tzHours == 0) snprintf(out, n, "UTC");
		else snprintf(out, n, "UTC%+d", (int)tzHours);
	}

	uint32_t calcHash() const
	{
		uint32_t h = 2166136261u;
		auto mix = [&](uint32_t v) { h ^= v; h *= 16777619u; };

		mix(_tmp.gsmEnable);
		mix(_tmp.gsmProviderIdx);
		mix(_tmp.gsmPeriodIdx);
		mix((uint8_t)_tzTargetTmp);
		mix((uint8_t)_tzNtpTmp);
		mix(_param);

		return h;
	}

	void drawStatic()
	{
		auto d = tft(); if (!d) return;
		d->loadFont(AA_FONT_TIME18);

		d->fillScreen(TFT_BLACK);
		d->setTextColor(TFT_YELLOW, TFT_BLACK);
		d->drawString("НАСТРОЙКИ GSM", X, Y_TITLE);

		// clear rows area
		for (int i = 0; i < 6; i++) d->fillRect(0, Y0 + i * DY, W, H, TFT_BLACK);

		// hints
		d->setTextColor(TFT_WHITE, TFT_BLACK);
		d->fillRect(0, Y_HINT1, 320, 28, TFT_BLACK);
		d->drawString("SW1:NEXT SW2:- SW3:+", X, Y_HINT1);
		d->drawString("SW4:SAVE+HOME", X, Y_HINT2);
	}

	void buildLine(uint8_t i, char* out, size_t n) const
	{
		char tzBuf[16];
		formatTz(tzBuf, sizeof(tzBuf), _tzTargetTmp);

		if (i == P_ENABLE) snprintf(out, n, "1) GSM:%s", _tmp.gsmEnable ? "ON" : "OFF");
		else if (i == P_PROVIDER) snprintf(out, n, "2) OP:%s", kGsmProviders[_tmp.gsmProviderIdx % 3]);
		else if (i == P_PERIOD) snprintf(out, n, "3) ПЕРИОД:%s", kPeriodsName[_tmp.gsmPeriodIdx % 6]);
		else if (i == P_TZ_TARGET) snprintf(out, n, "4) Часовой пояс:%s", tzBuf);
		else snprintf(out, n, "5) TZ NTP:%d", (int)_tzNtpTmp);
	}

	void drawDynamic(bool force)
	{
		auto d = tft(); if (!d) return;
		d->loadFont(AA_FONT_TIME18);

		uint32_t h = calcHash();
		if (!force && _lastParam == _param && _lastHash == h) {
			drawSig(false);
			return;
		}
		_lastParam = _param;
		_lastHash = h;

		for (uint8_t i = 0; i < PARAM_COUNT; i++) {
			d->fillRect(0, Y0 + i * DY, W, H, TFT_BLACK);
			d->setTextColor((i == _param) ? TFT_CYAN : TFT_WHITE, TFT_BLACK);
			d->drawString((i == _param) ? ">" : " ", 0, Y0 + i * DY);

			char line[96];
			buildLine(i, line, sizeof(line));
			d->drawString(line, X, Y0 + i * DY);
		}

		drawSig(true);
	}

	void changeParam(int delta)
	{
		if (_param == P_ENABLE) { _tmp.gsmEnable = _tmp.gsmEnable ? 0 : 1; return; }
		if (_param == P_PROVIDER) { _tmp.gsmProviderIdx = (uint8_t)((_tmp.gsmProviderIdx + (delta > 0 ? 1 : 2)) % 3); return; }
		if (_param == P_PERIOD) { _tmp.gsmPeriodIdx = (uint8_t)((_tmp.gsmPeriodIdx + (delta > 0 ? 1 : 5)) % 6); return; }
		if (_param == P_TZ_TARGET) { _tzTargetTmp += (delta > 0 ? 1 : -1); clampTz(_tzTargetTmp); return; }
		_tzNtpTmp += (delta > 0 ? 1 : -1); clampTz(_tzNtpTmp);
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


	void drawSig(bool force)
	{
		auto d = tft(); if (!d) return;
		d->loadFont(AA_FONT_TIME18);

		char buf[48];

		if (gCsqRssi == 99 || gRssiDbm == 0) snprintf(buf, sizeof(buf), "SIG :-- [0/5]");
		else snprintf(buf, sizeof(buf), "SIG :%ddBm [%u/5]", (int)gRssiDbm, (unsigned)gSignalBars);

		if (!force && strcmp(_lastSig, buf) == 0) return;

		strncpy(_lastSig, buf, sizeof(_lastSig) - 1);
		_lastSig[sizeof(_lastSig) - 1] = 0;

		d->fillRect(0, Y_SIG_LINE, 320, 20, TFT_BLACK);
		d->setTextColor(TFT_WHITE, TFT_BLACK);
		d->drawString(_lastSig, X, Y_SIG_LINE);
	}


	//void drawSig(bool force)
	//{
	//	auto d = tft(); if (!d) return;
	//	d->loadFont(AA_FONT_TIME18);

	//	char buf[48];
	//	// These globals are used in existing HOME code (from your previous file):
	//	// gSignalBars,gCsqRssi,gRssiDbm
	//	extern uint8_t gSignalBars;
	//	extern uint8_t gCsqRssi;
	//	extern int16_t gRssiDbm;

	//	if (gCsqRssi == 99 || gRssiDbm == 0) snprintf(buf, sizeof(buf), "SIG :-- [0/5]");
	//	else snprintf(buf, sizeof(buf), "SIG :%ddBm [%u/5]", (int)gRssiDbm, (unsigned)gSignalBars);

	//	if (!force && strcmp(_lastSig, buf) == 0) return;

	//	strncpy(_lastSig, buf, sizeof(_lastSig) - 1);
	//	_lastSig[sizeof(_lastSig) - 1] = 0;

	//	d->fillRect(0, Y_SIG_LINE, 320, 20, TFT_BLACK);
	//	d->setTextColor(TFT_WHITE, TFT_BLACK);
	//	d->drawString(_lastSig, X, Y_SIG_LINE);
	//}
};

// -------------------- Page5 stub --------------------
class Page5 :public PageBase {
public:using PageBase::PageBase;
	  void setup() override {}
	  void onActivate() override { draw(); }
	  void update() override {}
	  void draw() override
	  {
		  auto d = tft(); if (!d) return;
		  d->loadFont(AA_FONT_TIME18);
		  d->fillScreen(TFT_BLACK);
		  d->setTextColor(TFT_YELLOW, TFT_BLACK);
		  d->drawString("PAGE 5", 10, 10);
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
	pages[3] = &p4; // GSM SETTINGS
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

	// NEW:автовозврат на HOME если в меню и нет нажатий 10 минут
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

void Menu::drawHomeStatic()
{
	tft->loadFont(AA_FONT_TIME18);
	tft->fillScreen(TFT_BLACK);

	tft->setTextColor(TFT_CYAN, TFT_BLACK);
	tft->drawString("СОСТОЯНИЕ СИСТЕМЫ", 10, Y_HOME);

	tft->setTextColor(TFT_WHITE, TFT_BLACK);
	tft->drawString("IP1:", 10, Y_IP1);
	tft->drawString("IP2:", 10, Y_IP2);
	tft->drawString("DATE:", 10, Y_DATE);
	tft->drawString("GSM :", 10, Y_NET);
	// SIG removed from HOME

	tft->setTextColor(TFT_YELLOW, TFT_BLACK);
	tft->drawString(versionString, 10, Y_VER);

	_lastIp[0] = 0;
	_lastIp2[0] = 0;
	_lastDate[0] = 0;
	_lastNet = !gNetRegistered;
	_lastBars = 255;
}

void Menu::drawHomeDynamic()
{
	// Sprite is used only on HOME dynamic fields
	homeSpritesEnsure(tft);
	tft->loadFont(AA_FONT_TIME18);

	const char* ip1Show = isZeroIpStr(gIp1Str) ? "NOT CONNECTED" : gIp1Str;
	if (strcmp(_lastIp, ip1Show) != 0) {
		strncpy(_lastIp, ip1Show, sizeof(_lastIp) - 1);
		_lastIp[sizeof(_lastIp) - 1] = 0;
		homePushText(gHomeSpr240, 80, Y_IP1, TFT_WHITE, _lastIp);
	}

	const char* ip2Show = isZeroIpStr(gIp2Str) ? "NOT CONNECTED" : gIp2Str;
	if (strcmp(_lastIp2, ip2Show) != 0) {
		strncpy(_lastIp2, ip2Show, sizeof(_lastIp2) - 1);
		_lastIp2[sizeof(_lastIp2) - 1] = 0;
		homePushText(gHomeSpr240, 80, Y_IP2, TFT_WHITE, _lastIp2);
	}

	if (strcmp(_lastDate, gDateStr) != 0) {
		strncpy(_lastDate, gDateStr, sizeof(_lastDate) - 1);
		_lastDate[sizeof(_lastDate) - 1] = 0;
		homePushText(gHomeSpr240, 80, Y_DATE, TFT_WHITE, _lastDate);
	}

	if (_lastNet != gNetRegistered) {
		_lastNet = gNetRegistered;
		homePushText(gHomeSpr240, 80, Y_NET, gNetRegistered ? TFT_GREEN : TFT_RED,
			gNetRegistered ? "CONNECTED" : "NO");
	}

	// SIG removed from HOME (moved to GSM settings page)
}

void Menu::activate()
{
	// HOME sprites are used only on HOME; free them when entering menu to save RAM
	homeSpritesFree();

	page = 0;
	item = 0;
	state = MENU_PAGE_SELECT;
	menuMarkUserActivity(); // NEW
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
		menuMarkUserActivity(); // NEW
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
	// NEW:фиксируем активность только от SW1..SW4
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
	// NEW:считаем release тоже активностью
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
}

