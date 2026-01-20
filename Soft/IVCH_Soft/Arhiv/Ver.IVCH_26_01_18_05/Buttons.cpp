#include "Buttons.h"
#include <Arduino.h>

Button::Button(uint8_t apin, unsigned long longPressMs) 
{
    pin = apin;
    pinMode(pin, INPUT_PULLUP);
    lastState = true;
    pressedFlag = false;
    longPressDuration = longPressMs;
    event = BTN_NONE;
}

void Button::update() 
{
    bool state = digitalRead(pin);
    event = BTN_NONE;
    if (!state && lastState) 
    { // �������
        pressedFlag = true;
        pressedTime = millis();
    }
    if (state && !lastState && pressedFlag) 
    { // ����������
        unsigned long d = millis() - pressedTime;
        if (d > 15 && d < longPressDuration) 
        {
            event = BTN_SHORT;
        }
        pressedFlag = false;
    }
    if (!state && pressedFlag) 
    {
        if ((millis() - pressedTime) >= longPressDuration) 
        {
            event = BTN_LONG;
            pressedFlag = false;
        }
    }
    lastState = state;
}

ButtonEvent Button::getEvent() 
{
    return event;
}
