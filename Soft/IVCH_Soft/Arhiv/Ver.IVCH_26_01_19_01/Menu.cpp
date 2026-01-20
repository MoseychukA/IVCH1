#include "Menu.h"
#include <TFT_eSPI.h>

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



// Экспортируются из Menu.h extern const char* menu[5][5];
const char* menu[5][5] = {
    // примеры пунктов
    {"P1-1","P1-2","P1-3","P1-4","P1-5"},
    {"P2-1","P2-2","P2-3","P2-4","P2-5"},
    {"P3-1","P3-2","P3-3","P3-4","P3-5"},
    {"P4-1","P4-2","P4-3","P4-4","P4-5"},
    {"P5-1","P5-2","P5-3","P5-4","P5-5"}
};

Menu::Menu(TFT_eSPI* disp) {
    tft = disp;
    page = 0;
    item = 0;
    state = MENU_IDLE;
}

void Menu::setup(const String& ver) 
{
    tft->init();
    tft->setRotation(3);
    tft->fillScreen(TFT_NAVY);
    tft->setTextColor(TFT_WHITE, TFT_BLACK);  // Set the font colour AND the background colour
    versionString = ver;

    uint16_t tbw1;
    uint16_t x_tft, y_tft;

    tft->loadFont(AA_FONT_CALI);     // Must load the font first
 
    tft->setCursor(120, 20);
    tft->println("IVCH");
    tft->setCursor(100, 50);
    tft->println("DECIMA");

    tft->loadFont(AA_FONT_TIME24I);     // Must load the font first
 
    tft->setTextColor(TFT_YELLOW, TFT_BLACK);  // Set the font colour AND the background colour
    tft->setCursor(84, 90);
    tft->println("ВКЛЮЧЕНИЕ");

    tft->loadFont(AA_FONT_TIME18);     // Must load the font first
    tft->setTextColor(TFT_WHITE, TFT_BLACK);  // Set the font colour AND the background colour

    tft->setCursor(5,130);
    tft->println("(C) 2026");

    tft->setCursor(5, 150);
    tft->println("www.decima.ru");

    tbw1 = tft->textWidth(versionString);
    x_tft = (tft->width() - tbw1) - 4;
    tft->setCursor(x_tft, 150);
    tft->print(versionString);
    tft->loadFont(AA_FONT_TIME24I);     // Must load the font first

  //  delay(2000);
}

void Menu::drawStartPage() 
{
    tft->fillScreen(TFT_BLACK);
    tft->setTextColor(TFT_GREEN, TFT_BLACK);
    tft->setTextSize(2);
    tft->drawString("Home page", 50, 60);
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
    state = MENU_IDLE;
    drawStartPage();
}

bool Menu::isActive() const {
    return state != MENU_IDLE;
}

MenuState Menu::getState() const {
    return state;
}

// --- Новая логика листания:
void Menu::prevPage() {
    if (state == MENU_PAGE_SELECT) {
        page = (page + 4) % 5;
        drawPages();
    }
}
void Menu::nextPage() {
    if (state == MENU_PAGE_SELECT) {
        page = (page + 1) % 5;
        drawPages();
    }
}
void Menu::prevItem() {
    if (state == MENU_ITEM_SELECT) {
        item = (item + 4) % 5;
        drawItems();
    }
}
void Menu::nextItem() {
    if (state == MENU_ITEM_SELECT) {
        item = (item + 1) % 5;
        drawItems();
    }
}

// --- Кнопка "фиксировать страницу" (SW1)
void Menu::fixPage() {
    if (state == MENU_PAGE_SELECT) {
        item = 0;
        state = MENU_ITEM_SELECT;
        drawItems();
    }
}

// --- Кнопка "фиксировать пункт" (SW4)
void Menu::fixItem() 
{
    if (state == MENU_ITEM_SELECT) 
    {
        state = MENU_TEST;
        runTest();
    }
}

void Menu::backToStart() 
{
    deactivate();
}

void Menu::draw() 
{
    if (state == MENU_PAGE_SELECT) drawPages();
    else if (state == MENU_ITEM_SELECT) drawItems();
    else if (state == MENU_TEST) drawTestPage();
}

// обновите select() если пользовались им раньше:
void Menu::select() {}

// --- Вывод страниц меню
void Menu::drawPages() 
{
    tft->fillScreen(TFT_BLACK);

    // 1. В верхнем левом углу строка "Str1"..."Str5"
    char str[8];
    sprintf(str, "Str%d", page + 1);
    tft->setTextColor(TFT_CYAN, TFT_BLACK);
    tft->setTextSize(2);
    tft->drawString(str, 5, 5); // координаты верхний левый угол

    // 2. Ниже выводятся пункты меню текущей страницы, по одному в строку
    for (int i = 0; i < 5; i++) 
    {
        if (i == item && state == MENU_ITEM_SELECT)
            tft->setTextColor(TFT_GREEN, TFT_BLUE);
        else
            tft->setTextColor(TFT_WHITE, TFT_BLACK);
        tft->drawString(menu[page][i], 20, 35 + i * 30);
    }
}

// --- Вывод пунктов выбранной страницы
void Menu::drawItems() 
{
    tft->fillScreen(TFT_BLACK);
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    tft->setTextSize(2);
    char head[20];
    sprintf(head, "Page %d:", page + 1);
    tft->drawString(head, 30, 10);
    for (int i = 0; i < 5; i++) {
        if (i == item)
            tft->setTextColor(TFT_GREEN, TFT_BLUE);
        else
            tft->setTextColor(TFT_WHITE, TFT_BLACK);
        tft->drawString(menu[page][i], 40, 40 + i * 30);
    }
}

// --- Демонстрация вызова тестовой программы
void Menu::runTest() 
{
    // Здесь вызывается тестовая функция для выбранного пункта
    drawTestPage();
    // Например: testFunction(page, item);
}
void Menu::drawTestPage() 
{
    tft->fillScreen(TFT_BLACK);
    tft->setTextColor(TFT_RED, TFT_BLACK);
    tft->setTextSize(2);
    char str[32];
    sprintf(str, "ТЕСТ %s", menu[page][item]);
    tft->drawString(str, 20, 60);
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    tft->drawString("SW1 - exit", 20, 120);
}
