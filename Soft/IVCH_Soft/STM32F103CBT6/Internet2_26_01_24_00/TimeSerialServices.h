#pragma once
#include <Arduino.h>
#include "W5500NtpServerClient.h"

class TimeSerialServices {
public:
	TimeSerialServices(HardwareSerial& u2, HardwareSerial& u3, W5500NtpServerClient& n)
		:_u2(u2), _u3(u3), _n(n) {}

	void tick();

private:
	HardwareSerial& _u2;
	HardwareSerial& _u3;
	W5500NtpServerClient& _n;

	String _b2, _b3;

	void poll(HardwareSerial& s, String& b);
	void handle(HardwareSerial& s, const String& line);
};