#include "Menu.h"
#include <TFT_eSPI.h>

#include "TimeFeed.h" // gDateStr,gTimeStr,gTimeUpdated
#include "NetFeed.h" // gNetRegistered,gRssiDbm,gSignalBars,gCsqRssi...
#include "AT24C128Settings.h" // ee,cfg
#include "SyncSourcesStore.h" // syncStore,syncData
#include "TimeSyncPlanner.h" // planner

// ВАЖНО:в .ino эти объекты должны быть ГЛОБАЛЬНЫМИ (не static)
extern AT24C128Settings ee;
extern AT24C128Settings::Config cfg;

extern SyncSourcesStore syncStore;
extern SyncSourcesStore::Data syncData;
extern TimeSyncPlanner planner;

// GPS пока нет:в .ino заведите эти переменные (0/0/false),чтобы экран не падал на линковке
extern uint8_t gGpsSatsUsed;
extern uint8_t gGpsSatsView;
extern bool gGpsFix;

// Шрифты (как у вас)
#include "zTimesNRItalic18.h"
#include "zTimesNRItalic24.h"
#include "zTimesNRItalic28.h"
#include "zTimesNRItalic36.h"
#include "zCalibri36.h"
#include "zTimesNR28.h"
#include "zTimesNR14.h"
#include "zTimesNR18.h"
#include "zTimesNR24.h"

#define AA_FONT_CALI zCalibri36
#define AA_FONT_TIME18I zTimesNRItalic18
#define AA_FONT_TIME24I zTimesNRItalic24
#define AA_FONT_TIME28I zTimesNRItalic28
#define AA_FONT_TIME36I zTimesNRItalic36
#define AA_FONT_TIME28 zTimesNR28
#define AA_FONT_TIME18 zTimesNR18
#define AA_FONT_TIME24 zTimesNR24

// HOME layout (компактнее)
static const int Y_DATE = 40;
static const int Y_TIME = 63;
static const int Y_NET = 86;
static const int Y_SIG = 109;
static const int Y_VER = 150;

// Списки для источников синхронизации
static const char* kNtpServers[5] = {
 "pool.ntp.org",
 "time.google.com",
 "time.cloudflare.com",
 "time.windows.com",
 "ru.pool.ntp.org"
};

static const char* kGsmProviders[5] = { "МТС","МЕГАФОН","БИЛАЙН","YOTA","ТЕЛЕ 2" };
static const char* kPeriodsName[6] = { "1 мин","10 мин","30 мин","1 час","6 часов","12 часов" };

// Таблица меню (Str1 = источники синхронизации; Str2 = time zone)
const char* menu[5][5] = {
 {"GPS","ИНТЕРНЕТ","GSM","СВОБОДНО","СВОБОДНО"},
 {"TIME ZONE"," "," "," "," "},
 {"СВОБОДНО","СВОБОДНО","СВОБОДНО","СВОБОДНО","СВОБОДНО"},
 {"СВОБОДНО","СВОБОДНО","СВОБОДНО","СВОБОДНО","СВОБОДНО"},
 {"СВОБОДНО","СВОБОДНО","СВОБОДНО","СВОБОДНО","СВОБОДНО"}
};

// -------------------- Base for pages --------------------
class PageBase :public IMenuPage {
public:
	explicit PageBase(Menu* m) :_m(m) {}
protected:
	Menu* _m = nullptr;
	TFT_eSPI* tft() const { return _m ? _m->display() : nullptr; }
	int page() const { return _m ? _m->currentPageIndex() : 0; }
	int item() const { return _m ? _m->currentItemIndex() : 0; }
};

// -------------------- Page1:ИСТОЧНИКИ СИНХРОНИЗАЦИИ --------------------
class Page1 :public PageBase {
public:
	using PageBase::PageBase;

	void setup() override {}

	void onActivate() override {
		_mode = MODE_LIST;
		_sel = 0;
		_param = 0;

		_tmp = syncData; // работаем с копией (без записи)
		_staticDrawn = false;

		_lastSel = 255;
		_lastParam = 255;
		_lastLineHash = 0;

		draw();
	}

	void update() override {
		// GPS info можно обновлять хоть каждую итерацию (строка одна)
		if (_mode == MODE_GPS) drawGpsInfo(false);
	}

	void draw() override {
		if (!_staticDrawn) {
			drawStatic();
			_staticDrawn = true;
			// сброс кешей,чтобы первая динамика точно нарисовалась
			_lastSel = 255;
			_lastParam = 255;
			_lastLineHash = 0;
		}
		drawDynamic(true);
	}

	void onButtonPressed(int buttonID) override {

		// ---------- LIST MODE ----------
		if (_mode == MODE_LIST) {

			if (buttonID == BTN_SW2) { // UP
				_sel = (_sel + 4) % 5;
				drawDynamic(false);
				return;
			}

			if (buttonID == BTN_SW3) { // DOWN
				_sel = (_sel + 1) % 5;
				drawDynamic(false);
				return;
			}

			if (buttonID == BTN_SW1) { // ENTER selected source
				if (_sel == 0) _mode = MODE_GPS;
				else if (_sel == 1) _mode = MODE_NET;
				else if (_sel == 2) _mode = MODE_GSM;
				else return; // "СВОБОДНО"

				_param = 0;
				_staticDrawn = false;
				draw();
				return;
			}

			if (buttonID == BTN_SW4) { // EXIT to HOME
				_m->backToStart();
				return;
			}

			return;
		}

		// ---------- SUBMENU (GPS/NET/GSM) ----------
		// SW1:next parameter
		if (buttonID == BTN_SW1) {
			_param = (_param + 1) % paramCount();
			drawDynamic(false);
			return;
		}

		// SW2:"-" ,SW3:"+"
		if (buttonID == BTN_SW2) {
			changeParam(-1);
			drawDynamic(false);
			return;
		}
		if (buttonID == BTN_SW3) {
			changeParam(+1);
			drawDynamic(false);
			return;
		}

		// SW4:SAVE and back to list
		if (buttonID == BTN_SW4) {
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
	enum Mode :uint8_t { MODE_LIST = 0, MODE_GPS = 1, MODE_NET = 2, MODE_GSM = 3 };

	// Экран 320x170:разметка
	static const int X = 10;
	static const int Y_TITLE = 0;
	static const int Y0 = 22;
	static const int DY = 22; // 5 строк *22 = 110,влезает
	static const int LINE_W = 320;
	static const int LINE_H = 20;

	static const int Y_HINT1 = 132; // подсказки внизу (не затираются)
	static const int Y_HINT2 = 148;

	Mode _mode = MODE_LIST;
	bool _staticDrawn = false;

	uint8_t _sel = 0; // выбор источника в списке (0..4)
	uint8_t _param = 0; // выбор пункта в подменю (циклом)

	SyncSourcesStore::Data _tmp;

	// кеши для частичной перерисовки
	uint8_t _lastSel = 255;
	uint8_t _lastParam = 255;
	uint32_t _lastLineHash = 0;

	// ----- helpers -----
	uint8_t paramCount() const {
		if (_mode == MODE_GPS) return 2; // enable,period
		if (_mode == MODE_NET) return 3; // enable,server,period
		if (_mode == MODE_GSM) return 3; // enable,operator,period
		return 0;
	}

	void drawStatic() {
		TFT_eSPI* d = tft(); if (!d) return;

		d->fillScreen(TFT_BLACK);
		d->setTextSize(2);

		d->setTextColor(TFT_YELLOW, TFT_BLACK);
		if (_mode == MODE_LIST) d->drawString("ИСТОЧНИКИ СИНХРОНИЗАЦИИ", X, Y_TITLE);
		else if (_mode == MODE_GPS) d->drawString("GPS", X, Y_TITLE);
		else if (_mode == MODE_NET) d->drawString("ИНТЕРНЕТ", X, Y_TITLE);
		else if (_mode == MODE_GSM) d->drawString("GSM", X, Y_TITLE);

		// очистка области строк
		for (int i = 0; i < 6; i++) d->fillRect(0, Y0 + i * DY, LINE_W, LINE_H, TFT_BLACK);

		// подсказки (мелким шрифтом)
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

		// вернуть размер текста для контента
		d->setTextSize(2);
	}

	void drawDynamic(bool force) {
		TFT_eSPI* d = tft(); if (!d) return;

		if (_mode == MODE_LIST) {
			if (!force && _lastSel == _sel) return;
			_lastSel = _sel;

			for (int i = 0; i < 5; i++) {
				d->fillRect(0, Y0 + i * DY, LINE_W, LINE_H, TFT_BLACK);
				d->setTextColor((i == _sel) ? TFT_GREEN : TFT_WHITE, TFT_BLACK);
				d->drawString(_m->itemLabel(0, i), X, Y0 + i * DY);
			}
			return;
		}

		// Подменю:перерисуем только если изменился _param или изменились значения
		uint32_t h = calcStateHash();
		if (!force && _lastParam == _param && _lastLineHash == h) {
			if (_mode == MODE_GPS) drawGpsInfo(false);
			return;
		}
		_lastParam = _param;
		_lastLineHash = h;

		// Рисуем параметры (2 или 3 строки)
		uint8_t n = paramCount();
		for (uint8_t i = 0; i < n; i++) {
			d->fillRect(0, Y0 + i * DY, LINE_W, LINE_H, TFT_BLACK);

			// маркер активного пункта
			d->setTextColor((i == _param) ? TFT_CYAN : TFT_WHITE, TFT_BLACK);
			if (i == _param) d->drawString(">", 0, Y0 + i * DY);

			char line[64];
			buildParamLine(i, line, sizeof(line));
			d->drawString(line, X, Y0 + i * DY);
		}

		if (_mode == MODE_GPS) drawGpsInfo(true);
	}

	uint32_t calcStateHash() const {
		// дешёвый хеш для определения изменения содержимого
		uint32_t h = 2166136261u;
		auto mix = [&](uint32_t v) { h ^= v; h *= 16777619u; };

		if (_mode == MODE_GPS) {
			mix(_tmp.gpsEnable); mix(_tmp.gpsPeriodIdx);
		}
		else if (_mode == MODE_NET) {
			mix(_tmp.netEnable); mix(_tmp.netProviderIdx); mix(_tmp.netPeriodIdx);
		}
		else if (_mode == MODE_GSM) {
			mix(_tmp.gsmEnable); mix(_tmp.gsmProviderIdx); mix(_tmp.gsmPeriodIdx);
		}
		mix(_param);
		return h;
	}

	void buildParamLine(uint8_t i, char* out, size_t n) const {
		if (_mode == MODE_GPS) {
			if (i == 0) snprintf(out, n, "1) ВКЛ:%s", _tmp.gpsEnable ? "ДА" : "НЕТ");
			else snprintf(out, n, "2) ПЕР:%s", kPeriodsName[_tmp.gpsPeriodIdx % 6]);
			return;
		}

		if (_mode == MODE_NET) {
			if (i == 0) snprintf(out, n, "1) ВКЛ:%s", _tmp.netEnable ? "ДА" : "НЕТ");
			else if (i == 1) snprintf(out, n, "2) NTP:%s", kNtpServers[_tmp.netProviderIdx % 5]);
			else snprintf(out, n, "3) ПЕР:%s", kPeriodsName[_tmp.netPeriodIdx % 6]);
			return;
		}

		// MODE_GSM
		if (i == 0) snprintf(out, n, "1) ВКЛ:%s", _tmp.gsmEnable ? "ДА" : "НЕТ");
		else if (i == 1) snprintf(out, n, "2) ОП:%s", kGsmProviders[_tmp.gsmProviderIdx % 5]);
		else snprintf(out, n, "3) ПЕР:%s", kPeriodsName[_tmp.gsmPeriodIdx % 6]);
	}

	void changeParam(int delta) {
		// delta:-1/+1
		if (_mode == MODE_GPS) {
			if (_param == 0) {
				// ВКЛ:переключение при любом знаке
				_tmp.gpsEnable = _tmp.gpsEnable ? 0 : 1;
			}
			else {
				// период
				_tmp.gpsPeriodIdx = (uint8_t)((_tmp.gpsPeriodIdx + (delta > 0 ? 1 : 5)) % 6);
			}
			return;
		}

		if (_mode == MODE_NET) {
			if (_param == 0) _tmp.netEnable = _tmp.netEnable ? 0 : 1;
			else if (_param == 1) _tmp.netProviderIdx = (uint8_t)((_tmp.netProviderIdx + (delta > 0 ? 1 : 4)) % 5);
			else _tmp.netPeriodIdx = (uint8_t)((_tmp.netPeriodIdx + (delta > 0 ? 1 : 5)) % 6);
			return;
		}

		// MODE_GSM
		if (_param == 0) _tmp.gsmEnable = _tmp.gsmEnable ? 0 : 1;
		else if (_param == 1) _tmp.gsmProviderIdx = (uint8_t)((_tmp.gsmProviderIdx + (delta > 0 ? 1 : 4)) % 5);
		else _tmp.gsmPeriodIdx = (uint8_t)((_tmp.gsmPeriodIdx + (delta > 0 ? 1 : 5)) % 6);
	}

	void commitSave() {
		// записываем только по SW4
		syncData = _tmp;
		syncStore.save(syncData);

		// INTERNET:выбранный NTP сервер -> cfg.server -> EEPROM cfg
		if (_mode == MODE_NET) {
			const char* s = kNtpServers[syncData.netProviderIdx % 5];
			strncpy(cfg.server, s, sizeof(cfg.server) - 1);
			cfg.server[sizeof(cfg.server) - 1] = 0;
			ee.writeSERVER(cfg.server);
		}

		planner.onSettingsChanged();
	}

	void drawGpsInfo(bool clearLine) {
		TFT_eSPI* d = tft(); if (!d) return;
		// строка под параметрами (Y0 + 3*DY = 88) — влезает
		int y = Y0 + 3 * DY;
		if (clearLine) d->fillRect(0, y, LINE_W, LINE_H, TFT_BLACK);

		d->setTextColor(TFT_GREEN, TFT_BLACK);
		char buf[64];
		snprintf(buf, sizeof(buf), "SAT:%u/%u FIX:%s",
			(unsigned)gGpsSatsUsed, (unsigned)gGpsSatsView, gGpsFix ? "YES" : "NO");
		d->drawString(buf, X, y);
	}
};

// -------------------- Page2:TIME ZONE (tmp + confirm save on SW4) --------------------
class Page2 :public PageBase {
public:
 using PageBase::PageBase;

 void setup() override {}

 void onActivate() override {
 _staticDrawn = false;

 _tmpTarget = cfg.tzTargetHours;
 _tmpNtp = cfg.tzNtpHours;

 _param = 0; // 0=TZ target,1=TZ NTP
 draw();
 }

 void update() override {}

 void draw() override {
 if (!_staticDrawn) { drawStatic(); _staticDrawn = true; }
 drawDynamic(true);
 }

 void onButtonPressed(int buttonID) override {
 // SW1:следующий пункт (параметр)
 if (buttonID == BTN_SW1) {
 _param = (_param + 1) % 2;
 drawDynamic(false);
 return;
 }

 // SW2:минус
 if (buttonID == BTN_SW2) {
 if (_param == 0) { _tmpTarget--; clampTz(_tmpTarget); }
 else { _tmpNtp--; clampTz(_tmpNtp); }
 drawDynamic(false);
 return;
 }

 // SW3:плюс
 if (buttonID == BTN_SW3) {
 if (_param == 0) { _tmpTarget++; clampTz(_tmpTarget); }
 else { _tmpNtp++; clampTz(_tmpNtp); }
 drawDynamic(false);
 return;
 }

 // SW4:сохранить и выйти на HOME
 if (buttonID == BTN_SW4) {
 cfg.tzTargetHours = _tmpTarget;
 cfg.tzNtpHours = _tmpNtp;

 ee.writeTzTargetHours(cfg.tzTargetHours);
 ee.writeTzNtpHours(cfg.tzNtpHours);

 planner.onSettingsChanged();

 _m->backToStart();
 return;
 }
 }

 void onButtonReleased(int) override {}

private:
 bool _staticDrawn = false;

 uint8_t _param = 0;
 int8_t _tmpTarget = 0;
 int8_t _tmpNtp = 0;

 // кеш для частичной перерисовки
 uint8_t _lastParam = 255;
 int8_t _lastTarget = 127;
 int8_t _lastNtp = 127;

 // компактная разметка
 static const int X = 10;
 static const int Y_TITLE = 10;
 static const int Y1 = 50;
 static const int Y2 = 75;
 static const int H = 20;

 void drawStatic() {
 auto d = tft(); if(!d) return;
 d->fillScreen(TFT_BLACK);
 d->setTextSize(2);

 d->setTextColor(TFT_YELLOW,TFT_BLACK);
 d->drawString("TIME ZONE",10,10);

 d->setTextColor(TFT_WHITE,TFT_BLACK);
 d->drawString("1) TZ target:",X,Y1);
 d->drawString("2) TZ NTP :",X,Y2);

 d->drawString("SW1:NEXT",10,140);
 d->drawString("SW2:- SW3:+",10,165);
 d->drawString("SW4:SAVE+EXIT",10,190);
 }

 void drawDynamic(bool force) {
 auto d = tft(); if(!d) return;

 if (force || _lastParam != _param) {
 _lastParam = _param;
 // маркеры
 d->fillRect(0,Y1,10,H,TFT_BLACK);
 d->fillRect(0,Y2,10,H,TFT_BLACK);
 d->setTextColor(TFT_CYAN,TFT_BLACK);
 d->drawString(">",0,(_param==0)?Y1:Y2);
 }

 if (force || _lastTarget != _tmpTarget) {
 _lastTarget = _tmpTarget;
 d->fillRect(210,Y1,100,H,TFT_BLACK);
 char b[8]; snprintf(b,sizeof(b),"%d",(int)_tmpTarget);
 d->setTextColor(TFT_WHITE,TFT_BLACK);
 d->drawString(b,210,Y1);
 }

 if (force || _lastNtp != _tmpNtp) {
 _lastNtp = _tmpNtp;
 d->fillRect(210,Y2,100,H,TFT_BLACK);
 char b[8]; snprintf(b,sizeof(b),"%d",(int)_tmpNtp);
 d->setTextColor(TFT_WHITE,TFT_BLACK);
 d->drawString(b,210,Y2);
 }
 }

 static void clampTz(int8_t& tz) {
 if (tz < -12) tz = -12;
 if (tz > 14) tz = 14;
 }
};

// -------------------- Page3..Page5 stubs --------------------
class Page3 :public PageBase {
public:using PageBase::PageBase;
	  void setup() override {}
	  void update() override {}
	  void draw() override { auto d = tft(); if (!d) return; d->fillScreen(TFT_BLACK); d->setTextSize(2); d->setTextColor(TFT_YELLOW, TFT_BLACK); d->drawString("PAGE 3", 10, 10); }
	  void onActivate() override { draw(); }
	  void onButtonPressed(int) override {}
	  void onButtonReleased(int) override {}
};

class Page4 :public PageBase {
public:using PageBase::PageBase;
	  void setup() override {}
	  void update() override {}
	  void draw() override { auto d = tft(); if (!d) return; d->fillScreen(TFT_BLACK); d->setTextSize(2); d->setTextColor(TFT_YELLOW, TFT_BLACK); d->drawString("PAGE 4", 10, 10); }
	  void onActivate() override { draw(); }
	  void onButtonPressed(int) override {}
	  void onButtonReleased(int) override {}
};

class Page5 :public PageBase {
public:using PageBase::PageBase;
	  void setup() override {}
	  void update() override {}
	  void draw() override { auto d = tft(); if (!d) return; d->fillScreen(TFT_BLACK); d->setTextSize(2); d->setTextColor(TFT_YELLOW, TFT_BLACK); d->drawString("PAGE 5", 10, 10); }
	  void onActivate() override { draw(); }
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
	pages[0] = &p1;
	pages[1] = &p2;
	pages[2] = &p3;
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

	// стартовый экран (как у вас)
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

	drawStartPage(); // HOME
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

		// HOME dynamic каждые 500мс
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
	tft->drawString("HOME", 10, 10);

	tft->setTextColor(TFT_WHITE, TFT_BLACK);
	tft->drawString("DATE:", 10, Y_DATE);
	tft->drawString("TIME:", 10, Y_TIME);
	tft->drawString("NET :", 10, Y_NET);
	tft->drawString("SIG :", 10, Y_SIG);

	tft->setTextColor(TFT_YELLOW, TFT_BLACK);
	tft->drawString(versionString, 10, Y_VER);

	_lastDate[0] = 0;
	_lastTime[0] = 0;
	_lastNet = !gNetRegistered;
	_lastBars = 255;
}

void Menu::drawHomeDynamic()
{
	// Date
	if (strcmp(_lastDate, gDateStr) != 0) {
		strncpy(_lastDate, gDateStr, sizeof(_lastDate) - 1);
		_lastDate[sizeof(_lastDate) - 1] = 0;
		tft->fillRect(80, Y_DATE, 240, 20, TFT_BLACK);
		tft->setTextColor(TFT_WHITE, TFT_BLACK);
		tft->drawString(_lastDate, 80, Y_DATE);
	}

	// Time
	if (strcmp(_lastTime, gTimeStr) != 0) {
		strncpy(_lastTime, gTimeStr, sizeof(_lastTime) - 1);
		_lastTime[sizeof(_lastTime) - 1] = 0;
		tft->fillRect(80, Y_TIME, 240, 20, TFT_BLACK);
		tft->setTextColor(TFT_WHITE, TFT_BLACK);
		tft->drawString(_lastTime, 80, Y_TIME);
	}

	// Net
	if (_lastNet != gNetRegistered) {
		_lastNet = gNetRegistered;
		tft->fillRect(80, Y_NET, 240, 20, TFT_BLACK);
		tft->setTextColor(gNetRegistered ? TFT_GREEN : TFT_RED, TFT_BLACK);
		tft->drawString(gNetRegistered ? "CONNECTED" : "NO", 80, Y_NET);
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
	if (state == MENU_ITEM_SELECT) {
		if (pages[page]) pages[page]->onButtonPressed(BTN_SW4);
	}
}

void Menu::backToStart() { deactivate(); }

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
	"TIME ZONE",
	"СВОБОДНО",
	"СВОБОДНО",
	"СВОБОДНО"
	};

	tft->fillScreen(TFT_BLACK);
	tft->setTextSize(2);
	tft->setTextColor(TFT_CYAN, TFT_BLACK);
	tft->drawString("ВЫБОР МЕНЮ", 10, 10);

	// компактный шаг 24
	for (int i = 0; i < 5; i++) 
	{
		bool sel = (i == page);
		tft->setTextColor(sel ? TFT_GREEN : TFT_WHITE, sel ? TFT_BLUE : TFT_BLACK);
		tft->fillRect(10, 45 + i * 24, 310, 22, TFT_BLACK);
		tft->drawString(topMenuNames[i], 10, 45 + i * 24);
	}
}

void Menu::drawActivePage()
{
	if (pages[page]) pages[page]->draw();
}

void Menu::onButtonPressed(int buttonID)
{
	// HOME:вход в меню по SW1
	if (state == MENU_IDLE)
	{
		if (buttonID == BTN_SW1) activate();
		return;
	}

	// MENU_PAGE_SELECT:навигация и выбор
	if (state == MENU_PAGE_SELECT)
	{
		if (buttonID == BTN_SW2) prevPage(); // ВВЕРХ
		else if (buttonID == BTN_SW3) nextPage(); // ВНИЗ
		else if (buttonID == BTN_SW1) fixPage(); // ВЫБОР/ВХОД
		else if (buttonID == BTN_SW4) backToStart(); // ВЫХОД на HOME (если нужно)
		return;
	}

	// MENU_ITEM_SELECT:внутри страницы
	if (state == MENU_ITEM_SELECT)
	{
		// Отдаём кнопки странице (Page1/Page2 сами решают,что делать)
		if (pages[page]) {
			pages[page]->onButtonPressed(buttonID);
			return;
		}

		// Если страницы нет — можно сделать базовую навигацию по item:
		if (buttonID == BTN_SW2) prevItem(); // ВВЕРХ
		else if (buttonID == BTN_SW3) nextItem(); // ВНИЗ
		else if (buttonID == BTN_SW1) fixItem(); // ВЫБОР
		else if (buttonID == BTN_SW4) backToStart(); // ВЫХОД
		return;
	}
}

void Menu::onButtonReleased(int buttonID)
{
	if (state == MENU_ITEM_SELECT && pages[page]) pages[page]->onButtonReleased(buttonID);
}

void Menu::select() {}
void Menu::runTest() {}

void Menu::drawTestPage()
{
	tft->fillScreen(TFT_BLACK);
	tft->setTextColor(TFT_RED, TFT_BLACK);
	tft->setTextSize(2);
	tft->drawString("TEST (legacy)", 20, 60);
	tft->setTextColor(TFT_WHITE, TFT_BLACK);
	tft->drawString("SW1 - exit", 20, 120);
}

