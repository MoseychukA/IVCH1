#include "Buttons.h"
#include <Arduino.h>

Button::Button(uint8_t apin, unsigned long longPressMs, unsigned long debounceMs)
{
	pin = apin;
	pinMode(pin, INPUT_PULLUP);

	longPressDuration = longPressMs;
	debounceDuration = debounceMs;

	lastRawLevel = true;
	stableLevel = true;
	lastStableLevel = true;
	lastRawChangeMs = 0;

	pressedFlag = false;
	longSent = false;
	pressedTime = 0;
	lastReleaseDuration = 0;

	event = BTN_NONE;
	edge = EDGE_NONE;
}

bool Button::isPressed() const
{
	// stable (debounced) состояние
	return stableLevel == false; // INPUT_PULLUP:LOW=нажато
}

unsigned long Button::pressDurationMs() const
{
	if (isPressed()) {
		return millis() - pressedTime;
	}
	return lastReleaseDuration;
}

void Button::update()
{
	event = BTN_NONE;
	edge = EDGE_NONE;

	// 1) читаем raw уровень
	bool raw = digitalRead(pin); // true=HIGH (отпущено),false=LOW (нажато)
	unsigned long now = millis();

	// 2) отслеживаем изменение raw (для debounce)
	if (raw != lastRawLevel) {
		lastRawLevel = raw;
		lastRawChangeMs = now;
	}

	// 3) если raw стабилен debounceDuration — принимаем как stable
	if ((now - lastRawChangeMs) >= debounceDuration) {
		stableLevel = raw;
	}

	// 4) фронты по stable уровню
	if (stableLevel != lastStableLevel) {

		if (!stableLevel && lastStableLevel) {
			// отпущено -> нажато
			edge = EDGE_PRESSED;
			pressedFlag = true;
			longSent = false;
			pressedTime = now;
		}

		if (stableLevel && !lastStableLevel) {
			// нажато -> отпущено
			edge = EDGE_RELEASED;

			if (pressedFlag) {
				unsigned long d = now - pressedTime;
				lastReleaseDuration = d;

				// short:только если long ещё не отправляли
				if (!longSent && d < longPressDuration) {
					event = BTN_SHORT;
				}

				pressedFlag = false;
				longSent = false;
			}
			else {
				// если по какой-то причине отпустили без pressedFlag — всё равно фиксируем
				lastReleaseDuration = 0;
			}
		}

		lastStableLevel = stableLevel;
	}

	// 5) long press (по stable удержанию)
	if (!stableLevel && pressedFlag && !longSent) {
		if ((now - pressedTime) >= longPressDuration) {
			event = BTN_LONG;
			longSent = true;
		}
	}
}

ButtonEvent Button::getEvent() { return event; }
ButtonEdge Button::getEdge() { return edge; }