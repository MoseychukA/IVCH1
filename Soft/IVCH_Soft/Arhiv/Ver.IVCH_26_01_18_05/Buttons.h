#ifndef BUTTONS_H
#define BUTTONS_H

#include <stdint.h>

enum ButtonEvent { BTN_NONE, BTN_SHORT, BTN_LONG };

class Button {
public:
    Button(uint8_t pin, unsigned long longPressMs = 700);
    void update();
    ButtonEvent getEvent();

private:
    uint8_t pin;
    unsigned long lastPressed;
    bool lastState;
    bool pressedFlag;
    unsigned long pressedTime;
    unsigned long longPressDuration;
    ButtonEvent event;
};

#endif
