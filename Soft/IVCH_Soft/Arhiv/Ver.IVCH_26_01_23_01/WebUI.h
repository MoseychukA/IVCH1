#pragma once
#include <Arduino.h>
#include <Ethernet_Generic.hpp> // <-- ВАЖНО:только декларации,без реализации

class WebUI
{
public:
	explicit WebUI(uint16_t port = 80);

	void begin();
	void tick();

	// public (как вы выбрали вариант 1)
	static void htmlEscPrint(EthernetClient& c, const char* s);

private:
	EthernetServer _srv;

	void handleClient(EthernetClient& c);
	bool readRequestLine(EthernetClient& c, char* out, size_t outN, uint32_t timeoutMs);
	void drainHeaders(EthernetClient& c, uint32_t timeoutMs);

	static bool startsWith(const char* s, const char* pref);
	static void urlDecodeInPlace(char* s);
	static const char* findParam(char* query, const char* key);

	static bool parseBool01(const char* s, bool& out);
	static bool parseU8(const char* s, uint8_t& out, uint8_t minV, uint8_t maxV);
	static bool parseIp4(const char* s, uint8_t out[4]);

	void sendHeader(EthernetClient& c, int code, const __FlashStringHelper* ct);
	void sendRedirect(EthernetClient& c, const char* location);

	void pageHome(EthernetClient& c);
	void pageInternet1(EthernetClient& c);
	void pageInternet2(EthernetClient& c);
	void pageGsm(EthernetClient& c);
	void pageNotFound(EthernetClient& c);

	void actionInternet1Save(EthernetClient& c, char* query);
	void actionInternet2Save(EthernetClient& c, char* query);
	void actionGsmSave(EthernetClient& c, char* query);
};