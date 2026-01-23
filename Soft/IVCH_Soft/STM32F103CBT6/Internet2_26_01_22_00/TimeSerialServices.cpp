#include "TimeSerialServices.h"

void TimeSerialServices::tick() {
	poll(_u2, _b2);
	poll(_u3, _b3);
}

void TimeSerialServices::poll(HardwareSerial& s, String& b) {
	while (s.available()) {
		char c = (char)s.read();
		if (c == '\r') continue;
		if (c == '\n') {
			if (b.length()) { handle(s, b); b = ""; }
		}
		else {
			if (b.length() < 80) b += c;
		}
	}
}

void TimeSerialServices::handle(HardwareSerial& s, const String& line) {
	if (line == "TIME?" || line == "TIME") {
		_n.printNowTo(s);
		return;
	}
	if (line == "SYNC!") {
		_n.requestSyncNow();
		s.println("OK");
		return;
	}
	s.println("ERR");
}