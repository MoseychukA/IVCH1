// Требования:
// - HOME показывает IP1 и IP2 (gIp1Str/gIp2Str)
// - "ВЫБОР МЕНЮ" показывает только пункты (без IP)
// - Page1 "ИСТОЧНИКИ СИНХРОНИЗАЦИИ":GPS / INTERNET1 / INTERNET2 / GSM + Часовой пояс/TZ NTP
// - Page2 INTERNET1:настройка + STATUS:OK/NO IP=...
// - Page3 INTERNET2:настройка + STATUS:OK/NO IP=... + SAVE вызывает applyInternet2FromStore()

#include "Menu.h"
#include <TFT_eSPI.h>
#include <string.h>
#include "NetHelpers.h"

#include "TimeFeed.h" // gDateStr,gTimeStr,gTimeUpdated
#include "NetFeed.h" // gNetRegistered,gRssiDbm,gSignalBars,gCsqRssi,gIp1Str,gIp2Str,...
#include "AT24C128Settings.h"
#include "SyncSourcesStore.h"
#include "TimeSyncPlanner.h"
#include "LanIfStore.h"

#include "NtpLanService_Generic.h" // INTERNET1 status
#include "Internet2Client.h" // INTERNET2 status

// --- externs from .ino ---
extern AT24C128Settings ee;
extern AT24C128Settings::Config cfg;

extern SyncSourcesStore syncStore;
extern SyncSourcesStore::Data syncData;

extern TimeSyncPlanner planner;

extern LanIfStore lanStore; // INTERNET store in EEPROM
extern void applyInternet1FromStore(); // применить INTERNET1 (в .ino)
extern void applyInternet2FromStore(); // применить INTERNET2 (I2C) (в .ino)

extern NtpLanService_Generic ntpLan; // INTERNET1
extern Internet2Client internet2; // INTERNET2 (I2C)

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
static const int Y_SIG = 116;

static const int Y_VER = 140;

// --- NTP upstream list (только IP,индекс 0..4) ---
static const char* kNtpIpStr[5] = {
 "162.159.200.123",
 "162.159.200.1",
 "129.6.15.28",
 "132.163.96.1",
 "216.239.35.0"
};

// --- Period names (idx 0..5) ---
static const char* kPeriodsName[6] = { "1 мин","10 мин","30 мин","1 час","6 часов","12 часов" };

// --- GSM provider names (только 3) ---
static const char* kGsmProviders[3] = { "МТС","МЕГАФОН","БИЛАЙН" };

// Таблица строк для Page1->MODE_LIST (page=0,row=item)
const char* menu[5][5] = {
	// FIX:раньше 5-й пункт был пустой строкой,из-за этого казалось что "нет переключения" (курсор уходил в пустоту).
	// Теперь используем только первые 4 пункта,5-й оставляем как резерв/не рисуем.
	{"GPS","ИНТЕРНЕТ1","ИНТЕРНЕТ2","GSM"," "},
	{"СВОБОДНО","СВОБОДНО","СВОБОДНО","СВОБОДНО","СВОБОДНО"},
	{"СВОБОДНО","СВОБОДНО","СВОБОДНО","СВОБОДНО","СВОБОДНО"},
	{"СВОБОДНО","СВОБОДНО","СВОБОДНО","СВОБОДНО","СВОБОДНО"},
	{"СВОБОДНО","СВОБОДНО","СВОБОДНО","СВОБОДНО","СВОБОДНО"}
};

static constexpr uint8_t PAGE1_LIST_COUNT = 4; // GPS/INTERNET1/INTERNET2/GSM

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

		if (buttonID == BTN_SW4) { // SAVE + BACK
			commitSave();
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

	static void clampTz(int8_t& tz) {
		if (tz < -12) tz = -12;
		if (tz > 14) tz = 14;
	}

	static void formatTz(char* out, size_t n, int8_t tzHours) {
		if (tzHours == 0) snprintf(out, n, "UTC");
		else snprintf(out, n, "UTC%+d", (int)tzHours);
	}

	bool anyEnabledExceptCurrent(Mode m) const {
		const bool gps = (_tmp.gpsEnable != 0);
		const bool net = (_tmp.netEnable != 0);
		const bool net2 = (_tmp.net2Enable != 0);
		const bool gsm = (_tmp.gsmEnable != 0);

		if (m == MODE_GPS) return (net || net2 || gsm);
		if (m == MODE_NET) return (gps || net2 || gsm);
		if (m == MODE_NET2) return (gps || net || gsm);
		if (m == MODE_GSM) return (gps || net || net2);
		return false;
	}

	void setExclusive(Mode m) {
		_tmp.gpsEnable = 0;
		_tmp.netEnable = 0;
		_tmp.net2Enable = 0;
		_tmp.gsmEnable = 0;

		if (m == MODE_GPS) _tmp.gpsEnable = 1;
		else if (m == MODE_NET) _tmp.netEnable = 1;
		else if (m == MODE_NET2) _tmp.net2Enable = 1;
		else if (m == MODE_GSM) _tmp.gsmEnable = 1;
	}

	uint8_t paramCount() const {
		if (_mode == MODE_GPS) return 4;
		if (_mode == MODE_NET) return 5;
		if (_mode == MODE_NET2) return 4;
		if (_mode == MODE_GSM) return 5;
		return 0;
	}

	void drawStatic() {
		TFT_eSPI* d = tft(); if (!d) return;

		d->fillScreen(TFT_BLACK);
		d->setTextSize(2);
		d->setTextColor(TFT_YELLOW, TFT_BLACK);

		if (_mode == MODE_LIST) d->drawString("ИСТОЧНИКИ СИНХРОНИЗАЦИИ", X, Y_TITLE);
		else if (_mode == MODE_GPS) d->drawString("GPS", X, Y_TITLE);
		else if (_mode == MODE_NET) d->drawString("ИНТЕРНЕТ1", X, Y_TITLE);
		else if (_mode == MODE_NET2) d->drawString("ИНТЕРНЕТ2", X, Y_TITLE);
		else if (_mode == MODE_GSM) d->drawString("GSM", X, Y_TITLE);

		for (int i = 0; i < 6; i++) d->fillRect(0, Y0 + i * DY, LINE_W, LINE_H, TFT_BLACK);

		d->setTextSize(1);
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

		d->setTextSize(2);
	}

	void drawDynamic(bool force) {
		TFT_eSPI* d = tft(); if (!d) return;

		if (_mode == MODE_LIST) {
			if (!force && _lastSel == _sel) return;
			_lastSel = _sel;

			// FIX:рисуем только 4 пункта,без пустого 5-го
			for (int i = 0; i < PAGE1_LIST_COUNT; i++) {
				d->fillRect(0, Y0 + i * DY, LINE_W, LINE_H, TFT_BLACK);
				d->setTextColor((i == _sel) ? TFT_GREEN : TFT_WHITE, TFT_BLACK);
				d->drawString(_m->itemLabel(0, i), X, Y0 + i * DY);
			}
			// очистим возможный “хвост” пятой строки (если раньше рисовали 5)
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
			else if (i == 1) snprintf(out, n, "2) PER:%s", kPeriodsName[_tmp.gpsPeriodIdx % 6]);
			else if (i == 2) snprintf(out, n, "3) Часовой пояс:%s", tzBuf);
			else snprintf(out, n, "4) TZ NTP:%d", (int)_tzNtpTmp);
			return;
		}

		if (_mode == MODE_NET) {
			if (i == 0) snprintf(out, n, "1) NTP:%s", _tmp.netEnable ? "ON" : "OFF");
			else if (i == 1) snprintf(out, n, "2) NTP IP:%s", kNtpIpStr[_tmp.netProviderIdx % 5]);
			else if (i == 2) snprintf(out, n, "3) PER:%s", kPeriodsName[_tmp.netPeriodIdx % 6]);
			else if (i == 3) snprintf(out, n, "4) Часовой пояс:%s", tzBuf);
			else snprintf(out, n, "5) TZ NTP:%d", (int)_tzNtpTmp);
			return;
		}

		if (_mode == MODE_NET2) {
			if (i == 0) snprintf(out, n, "1) NET2:%s", _tmp.net2Enable ? "ON" : "OFF");
			else if (i == 1) snprintf(out, n, "2) PER:%s", kPeriodsName[_tmp.net2PeriodIdx % 6]);
			else if (i == 2) snprintf(out, n, "3) Часовой пояс:%s", tzBuf);
			else snprintf(out, n, "4) TZ NTP:%d", (int)_tzNtpTmp);
			return;
		}

		// MODE_GSM
		if (i == 0) snprintf(out, n, "1) GSM:%s", _tmp.gsmEnable ? "ON" : "OFF");
		else if (i == 1) snprintf(out, n, "2) OP:%s", kGsmProviders[_tmp.gsmProviderIdx % 3]);
		else if (i == 2) snprintf(out, n, "3) PER:%s", kPeriodsName[_tmp.gsmPeriodIdx % 6]);
		else if (i == 3) snprintf(out, n, "4) Часовой пояс:%s", tzBuf);
		else snprintf(out, n, "5) TZ NTP:%d", (int)_tzNtpTmp);
	}

	void changeParam(int delta) {
		// текущая логика:выбор одного источника (эксклюзивно)
		if (_mode == MODE_GPS) {
			if (_param == 0) {
				if (_tmp.gpsEnable) {
					if (!anyEnabledExceptCurrent(MODE_GPS)) {}
					else _tmp.gpsEnable = 0;
				}
				else setExclusive(MODE_GPS);
			}
			else if (_param == 1) _tmp.gpsPeriodIdx = (uint8_t)((_tmp.gpsPeriodIdx + (delta > 0 ? 1 : 5)) % 6);
			else if (_param == 2) { _tzTargetTmp += (delta > 0 ? 1 : -1); clampTz(_tzTargetTmp); }
			else { _tzNtpTmp += (delta > 0 ? 1 : -1); clampTz(_tzNtpTmp); }
			return;
		}

		if (_mode == MODE_NET) {
			if (_param == 0) {
				if (_tmp.netEnable) {
					if (!anyEnabledExceptCurrent(MODE_NET)) {}
					else _tmp.netEnable = 0;
				}
				else setExclusive(MODE_NET);
			}
			else if (_param == 1) _tmp.netProviderIdx = (uint8_t)((_tmp.netProviderIdx + (delta > 0 ? 1 : 4)) % 5);
			else if (_param == 2) _tmp.netPeriodIdx = (uint8_t)((_tmp.netPeriodIdx + (delta > 0 ? 1 : 5)) % 6);
			else if (_param == 3) { _tzTargetTmp += (delta > 0 ? 1 : -1); clampTz(_tzTargetTmp); }
			else { _tzNtpTmp += (delta > 0 ? 1 : -1); clampTz(_tzNtpTmp); }
			return;
		}

		if (_mode == MODE_NET2) {
			if (_param == 0) {
				if (_tmp.net2Enable) {
					if (!anyEnabledExceptCurrent(MODE_NET2)) {}
					else _tmp.net2Enable = 0;
				}
				else setExclusive(MODE_NET2);
			}
			else if (_param == 1) _tmp.net2PeriodIdx = (uint8_t)((_tmp.net2PeriodIdx + (delta > 0 ? 1 : 5)) % 6);
			else if (_param == 2) { _tzTargetTmp += (delta > 0 ? 1 : -1); clampTz(_tzTargetTmp); }
			else { _tzNtpTmp += (delta > 0 ? 1 : -1); clampTz(_tzNtpTmp); }
			return;
		}

		// MODE_GSM
		if (_param == 0) {
			if (_tmp.gsmEnable) {
				if (!anyEnabledExceptCurrent(MODE_GSM)) {}
				else _tmp.gsmEnable = 0;
			}
			else setExclusive(MODE_GSM);
		}
		else if (_param == 1) {
			// FIX:раньше при "-" было +4 и %3 => фактически всегда +1.
			_tmp.gsmProviderIdx = (uint8_t)((_tmp.gsmProviderIdx + (delta > 0 ? 1 : 2)) % 3);
		}
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

		if (clearLine || changed1) {
			d->fillRect(0, y1, LINE_W, LINE_H, TFT_BLACK);
			d->setTextColor(TFT_GREEN, TFT_BLACK);
			d->drawString(line1, X, y1);
			strncpy(_lastGpsLine1, line1, sizeof(_lastGpsLine1) - 1);
			_lastGpsLine1[sizeof(_lastGpsLine1) - 1] = 0;
		}

		if (clearLine || changed2) {
			d->fillRect(0, y2 - 2, LINE_W, LINE_H + 2, TFT_BLACK);
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

	void drawIpLineRO(TFT_eSPI* d, const char* name, uint8_t v[4], int row) {
		d->setTextColor(TFT_WHITE, TFT_BLACK);
		d->drawString(" ", 0, Y0 + row * DY);

		char s[40];
		snprintf(s, sizeof(s), "%s %u.%u.%u.%u", name, v[0], v[1], v[2], v[3]);
		d->drawString(s, X, Y0 + row * DY);
	}

	void drawIpLine(TFT_eSPI* d, const char* name, uint8_t v[4], uint8_t p, int row, uint8_t curParam) {
		d->setTextColor(curParam == p ? TFT_CYAN : TFT_WHITE, TFT_BLACK);
		d->drawString(curParam == p ? ">" : " ", 0, Y0 + row * DY);

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
		if (buttonID == BTN_SW4) { // SAVE+HOME
			lanStore.save(LanIfStore::IF1, _cfg, true);
			applyInternet1FromStore();
			_m->backToStart();
			return;
		}

		if (buttonID == BTN_SW1) { // NEXT (только DHCP/NTP/PER)
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

		d->setTextSize(1);
		d->fillRect(140, 0, 170, 16, TFT_BLACK);

		bool ok = (strstr(_statLine, "STATUS :OK") != nullptr);
		d->setTextColor(ok ? TFT_GREEN : TFT_RED, TFT_BLACK);
		d->drawString(_statLine, 140, Y_TITLE);

		d->setTextSize(2);
	}

	void drawStatic()
	{
		auto d = tft(); if (!d) return;
		d->fillScreen(TFT_BLACK);
		d->setTextSize(2);
		d->setTextColor(TFT_YELLOW, TFT_BLACK);
		d->drawString("INTERNET 1", X, Y_TITLE);

		drawStatusLine();
		d->setTextSize(2);
	}

	void drawDynamic(bool)
	{
		auto d = tft(); if (!d) return;

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
		if (buttonID == BTN_SW4) { // SAVE+HOME
			lanStore.save(LanIfStore::IF2, _cfg, true);
			applyInternet2FromStore();
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

		d->setTextSize(1);
		d->fillRect(140, 0, 170, 16, TFT_BLACK);

		bool ok = (strstr(_statLine, "STATUS :OK") != nullptr);
		d->setTextColor(ok ? TFT_GREEN : TFT_RED, TFT_BLACK);
		d->drawString(_statLine, 140, Y_TITLE);

		d->setTextSize(2);
	}

	void drawStatic()
	{
		auto d = tft(); if (!d) return;
		d->fillScreen(TFT_BLACK);
		d->setTextSize(2);
		d->setTextColor(TFT_YELLOW, TFT_BLACK);
		d->drawString("INTERNET 2", X, Y_TITLE);

		drawStatusLine();
		d->setTextSize(2);
	}

	void drawDynamic(bool)
	{
		auto d = tft(); if (!d) return;

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
public:using PageBase::PageBase;
	  void setup() override {}
	  void onActivate() override { draw(); }
	  void update() override {}
	  void draw() override {
		  auto d = tft(); if (!d) return;
		  d->fillScreen(TFT_BLACK);
		  d->setTextSize(2);
		  d->setTextColor(TFT_YELLOW, TFT_BLACK);
		  d->drawString("PAGE 4", 10, 10);
	  }
	  void onButtonPressed(int) override {}
	  void onButtonReleased(int) override {}
};

class Page5 :public PageBase {
public:using PageBase::PageBase;
	  void setup() override {}
	  void onActivate() override { draw(); }
	  void update() override {}
	  void draw() override {
		  auto d = tft(); if (!d) return;
		  d->fillScreen(TFT_BLACK);
		  d->setTextSize(2);
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
	tft->fillScreen(TFT_BLACK);
	tft->setTextSize(2);

	tft->setTextColor(TFT_CYAN, TFT_BLACK);
	tft->drawString("HOME", 10, Y_HOME);

	tft->setTextColor(TFT_WHITE, TFT_BLACK);
	tft->drawString("IP1:", 10, Y_IP1);
	tft->drawString("IP2:", 10, Y_IP2);
	tft->drawString("DATE:", 10, Y_DATE);
	tft->drawString("NET GSM :", 10, Y_NET);
	tft->drawString("SIG :", 10, Y_SIG);

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
	// IP1 (0.0.0.0 -> NOT CONNECTED)
	const char* ip1Show = isZeroIpStr(gIp1Str) ? "NOT CONNECTED" : gIp1Str;
	if (strcmp(_lastIp, ip1Show) != 0) {
		strncpy(_lastIp, ip1Show, sizeof(_lastIp) - 1);
		_lastIp[sizeof(_lastIp) - 1] = 0;
		tft->fillRect(80, Y_IP1, 240, 20, TFT_BLACK);
		tft->setTextColor(TFT_WHITE, TFT_BLACK);
		tft->drawString(_lastIp, 80, Y_IP1);
	}

	// IP2 (0.0.0.0 -> NOT CONNECTED)
	const char* ip2Show = isZeroIpStr(gIp2Str) ? "NOT CONNECTED" : gIp2Str;
	if (strcmp(_lastIp2, ip2Show) != 0) {
		strncpy(_lastIp2, ip2Show, sizeof(_lastIp2) - 1);
		_lastIp2[sizeof(_lastIp2) - 1] = 0;
		tft->fillRect(80, Y_IP2, 240, 20, TFT_BLACK);
		tft->setTextColor(TFT_WHITE, TFT_BLACK);
		tft->drawString(_lastIp2, 80, Y_IP2);
	}

	// Date
	if (strcmp(_lastDate, gDateStr) != 0) {
		strncpy(_lastDate, gDateStr, sizeof(_lastDate) - 1);
		_lastDate[sizeof(_lastDate) - 1] = 0;
		tft->fillRect(80, Y_DATE, 240, 20, TFT_BLACK);
		tft->setTextColor(TFT_WHITE, TFT_BLACK);
		tft->drawString(_lastDate, 80, Y_DATE);
	}

	// Net (SIM800 registration)
	if (_lastNet != gNetRegistered) {
		_lastNet = gNetRegistered;
		tft->fillRect(100, Y_NET, 240, 20, TFT_BLACK);
		tft->setTextColor(gNetRegistered ? TFT_GREEN : TFT_RED, TFT_BLACK);
		tft->drawString(gNetRegistered ? "CONNECTED" : "NO", 100, Y_NET);
	}

	// Signal
	if (_lastBars != gSignalBars) {
		_lastBars = gSignalBars;
		tft->fillRect(80, Y_SIG, 240, 20, TFT_BLACK);
		tft->setTextColor(TFT_WHITE, TFT_BLACK);
		char buf[32];
		if (gCsqRssi == 99 || gRssiDbm == 0) snprintf(buf, sizeof(buf), "-- [0/5]");
		else snprintf(buf, sizeof(buf), "%ddBm [%u/5]", (int)gRssiDbm, (unsigned)gSignalBars);
		tft->drawString(buf, 80, Y_SIG);
	}
}

void Menu::activate()
{
	page = 0;
	item = 0;
	state = MENU_PAGE_SELECT;
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
		if (pages[page]) pages[page]->onActivate();
		else drawActivePage();
	}
}

void Menu::fixItem()
{
	// legacy:в нашем UI item не используется как отдельный уровень
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
		"СВОБОДНО",
		"СВОБОДНО"
	};

	tft->fillScreen(TFT_BLACK);
	tft->setTextSize(2);
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
	if (state == MENU_ITEM_SELECT && pages[page]) pages[page]->onButtonReleased(buttonID);
}

const char* Menu::itemLabel(int p, int i) const
{
	if (p < 0 || p > 4 || i < 0 || i > 4) return "";
	return menu[p][i];
}

void Menu::drawTestPage()
{
	tft->fillScreen(TFT_BLACK);
	tft->setTextColor(TFT_RED, TFT_BLACK);
	tft->setTextSize(2);
	tft->drawString("TEST (legacy)", 20, 60);
	tft->setTextColor(TFT_WHITE, TFT_BLACK);
	tft->drawString("SW4 - exit", 20, 120);
}