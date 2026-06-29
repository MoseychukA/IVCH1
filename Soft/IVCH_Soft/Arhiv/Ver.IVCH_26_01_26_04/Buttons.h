#ifndef BUTTONS_H
#define BUTTONS_H

#include <stdint.h>

enum ButtonEvent { BTN_NONE, BTN_SHORT, BTN_LONG };
enum ButtonEdge { EDGE_NONE, EDGE_PRESSED, EDGE_RELEASED };

class Button {
public:
	// longPressMs:порог длинного нажати€
	// debounceMs:антидребезг по уровню (стабилизаци€)
	Button(uint8_t pin, unsigned long longPressMs = 700, unsigned long debounceMs = 25);

	// вызывать в loop()
	void update();

	// одноразовые событи€ "жестов"
	ButtonEvent getEvent();

	// одноразовые событи€ фронтов
	ButtonEdge getEdge();

	// текущее (стабильное) состо€ние
	bool isPressed() const;

	// длительность текущего удержани€ (если нажата) или последнего удержани€ (если отпущена)
	unsigned long pressDurationMs() const;

private:
	uint8_t pin;

	unsigned long longPressDuration;
	unsigned long debounceDuration;

	// raw level tracking
	bool lastRawLevel; // предыдущий считанный уровень (не отфильтрованный)
	unsigned long lastRawChangeMs;

	// debounced stable level
	bool stableLevel; // true=HIGH(отпущено),false=LOW(нажато)
	bool lastStableLevel;

	// press tracking
	bool pressedFlag;
	bool longSent;
	unsigned long pressedTime;
	unsigned long lastReleaseDuration;

	// outputs
	ButtonEvent event;
	ButtonEdge edge;
};

#endif