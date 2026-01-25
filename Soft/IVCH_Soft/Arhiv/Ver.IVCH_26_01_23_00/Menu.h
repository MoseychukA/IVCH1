#ifndef MENU_H
#define MENU_H

#include <Arduino.h>
#include <TFT_eSPI.h>

extern const char* menu[5][5];

enum MenuState { MENU_IDLE, MENU_PAGE_SELECT, MENU_ITEM_SELECT, MENU_TEST };

// ID кнопок (единые)
enum MenuButtonID {
	BTN_SW1 = 1,
	BTN_SW2 = 2,
	BTN_SW3 = 3,
	BTN_SW4 = 4
};

// Интерфейс страницы-программы
class IMenuPage {
public:
	virtual ~IMenuPage() {}
	virtual void setup() = 0;
	virtual void update() = 0;
	virtual void draw() = 0;
	virtual void onActivate() = 0;
	virtual void onButtonPressed(int buttonID) = 0;
	virtual void onButtonReleased(int buttonID) = 0;
};

class Menu {
public:
	Menu(TFT_eSPI* disp);

	void setup(const String& ver);

	// ВАЖНО:вызывать постоянно из loop()
	void update();

	// HOME (главная страница)
	void drawStartPage(); // переключиться на HOME
	void invalidateHome(); // принудительно обновить HOME-динамику

	// Управление меню
	void activate();
	void deactivate();
	void nextPage();
	void prevPage();
	void nextItem();
	void prevItem();
	void select(); // оставлено для совместимости (пусто)
	void draw(); // перерисовка текущего экрана

	void fixPage();
	void fixItem();
	void backToStart();

	void runTest(); // оставлено для совместимости (legacy)
	void drawTestPage(); // legacy

	bool isActive() const;
	MenuState getState() const;

	// события кнопок (pressed/released)
	void onButtonPressed(int buttonID);
	void onButtonReleased(int buttonID);

	// Доступ страницам
	TFT_eSPI* display() const { return tft; }
	int currentPageIndex() const { return page; } // 0..4
	int currentItemIndex() const { return item; } // 0..4

	// ВАЖНО:только объявление (реализация в Menu.cpp)
	const char* itemLabel(int p, int i) const;

private:
	TFT_eSPI* tft = nullptr;
	int page = 0;
	int item = 0;
	MenuState state = MENU_IDLE;
	String versionString;

	// страницы
	IMenuPage* pages[5] = { nullptr,nullptr,nullptr,nullptr,nullptr };

	// HOME scheduling/caching
	uint32_t _lastHomeTickMs = 0;
	bool _homeStaticDrawn = false;
	bool _homeDirty = true;

	// caches for HOME
	char _lastIp[16] = "0.0.0.0"; // <-- ДОБАВЛЕНО
	char _lastIp2[16] = "0.0.0.0";
	char _lastTime[9] = "00:00:00"; // можно оставить (в HOME TIME не рисуем,но пусть будет)
	char _lastDate[11] = "00.00.0000";
	bool _lastNet = false;
	uint8_t _lastBars = 255;

private:
	void attachPages();

	// HOME
	void drawHomeStatic();
	void drawHomeDynamic();

	// MENU_PAGE_SELECT
	void drawPageSelect();

	// MENU_ITEM_SELECT
	void drawActivePage();
};

#endif