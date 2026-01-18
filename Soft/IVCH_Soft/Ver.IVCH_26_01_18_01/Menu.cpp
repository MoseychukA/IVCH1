#include "Menu.h"
#include <TFT_eSPI.h>



const char* menu[5][5] = {
    {"P1-1", "P1-2", "P1-3", "P1-4", "P1-5"},
    {"P2-1", "P2-2", "P2-3", "P2-4", "P2-5"},
    {"P3-1", "P3-2", "P3-3", "P3-4", "P3-5"},
    {"P4-1", "P4-2", "P4-3", "P4-4", "P4-5"},
    {"P5-1", "P5-2", "P5-3", "P5-4", "P5-5"}
};

Menu::Menu(TFT_eSPI* disp) {
    tft = disp;
    page = 0;
    item = 0;
    state = MENU_IDLE;
}

void Menu::setup()
{
    tft->init();
    tft->setRotation(3);
    tft->fillScreen(TFT_BLACK);


}

void Menu::drawStartPage() {
    tft->fillScreen(TFT_BLACK);
    tft->setTextFont(2); // или другой шрифт по вашему вкусу
    tft->setTextColor(TFT_GREEN, TFT_BLACK);
    tft->setTextSize(2);
    tft->drawString("Home page", 20, 60); // можно заменить на свою
}

void Menu::activate() {
    page = 0;
    item = 0;
    state = MENU_PAGE_SELECT;
    draw();
}

void Menu::deactivate() {
    state = MENU_IDLE;
    drawStartPage();
   // tft->fillScreen(TFT_BLACK);
}

bool Menu::isActive() const {
    return state != MENU_IDLE;
}

MenuState Menu::getState() const {
    return state;
}

void Menu::nextPage() {
    if (state == MENU_PAGE_SELECT) {
        page = (page + 1) % 5;
        drawPages();
    }
}

void Menu::prevPage() {
    if (state == MENU_PAGE_SELECT) {
        page = (page + 4) % 5;
        drawPages();
    }
}

void Menu::nextItem() {
    if (state == MENU_ITEM_SELECT) {
        item = (item + 1) % 5;
        drawItems();
    }
}

void Menu::prevItem() {
    if (state == MENU_ITEM_SELECT) {
        item = (item + 4) % 5;
        drawItems();
    }
}

void Menu::select() {
    if (state == MENU_PAGE_SELECT) {
        state = MENU_ITEM_SELECT;
        item = 0;
        drawItems();
    }
    else if (state == MENU_ITEM_SELECT) {
        // Здесь можно добавить логику действия по выбранному пункту
        deactivate();
    }
}

void Menu::draw() {
    if (state == MENU_PAGE_SELECT) {
        drawPages();
    }
    else if (state == MENU_ITEM_SELECT) {
        drawItems();
    }
}

void Menu::drawPages() {
    tft->fillScreen(TFT_BLACK);
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    tft->setTextSize(2);
    tft->drawString("Выбор страницы", 30, 10);
    for (int i = 0; i < 5; i++) {
        if (i == page)
            tft->setTextColor(TFT_YELLOW, TFT_BLUE);
        else
            tft->setTextColor(TFT_WHITE, TFT_BLACK);
        char buf[16];
        sprintf(buf, "Страница %d", i + 1);
        tft->drawString(buf, 40, 40 + i * 30);
    }
}

void Menu::drawItems() {
    tft->fillScreen(TFT_BLACK);
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    tft->setTextSize(2);
    char head[20];
    sprintf(head, "Страница %d:", page + 1);
    tft->drawString(head, 30, 10);
    for (int i = 0; i < 5; i++) {
        if (i == item)
            tft->setTextColor(TFT_YELLOW, TFT_BLUE);
        else
            tft->setTextColor(TFT_WHITE, TFT_BLACK);
        tft->drawString(menu[page][i], 40, 40 + i * 30);
    }
}