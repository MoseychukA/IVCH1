#ifndef MENU_H
#define MENU_H

#include <TFT_eSPI.h>
extern const char* menu[5][5];
enum MenuState { MENU_IDLE, MENU_PAGE_SELECT, MENU_ITEM_SELECT, MENU_TEST };


class Menu {
public:
    Menu(TFT_eSPI* disp);
    void activate();
    void deactivate();
    void nextPage();
    void prevPage();
    void nextItem();
    void prevItem();
    void select(); // 
    void draw();
    void setup(const String& ver);
    void drawStartPage();
    void fixPage();
    void fixItem();
    void backToStart();
    void runTest();
    void drawTestPage();

    bool isActive() const;
    MenuState getState() const;

private:
    TFT_eSPI* tft;
    int page;
    int item;
    MenuState state;
    void drawPages();
    void drawItems();
    String versionString;
};

#endif
