#ifndef MENU_H
#define MENU_H

#include <TFT_eSPI.h>
extern const char* menu[5][5];
enum MenuState { MENU_IDLE, MENU_PAGE_SELECT, MENU_ITEM_SELECT };


class Menu {
public:
    Menu(TFT_eSPI* disp);
    void activate();
    void deactivate();
    void nextPage();
    void prevPage();
    void nextItem();
    void prevItem();
    void select(); // фиксация выбора
    void draw();
    void setup();
    void drawStartPage();

    bool isActive() const;
    MenuState getState() const;

private:
    TFT_eSPI* tft;
    int page;
    int item;
    MenuState state;
    void drawPages();
    void drawItems();
};

#endif