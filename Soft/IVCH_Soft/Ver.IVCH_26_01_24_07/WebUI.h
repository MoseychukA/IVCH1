#pragma once
#include <Arduino.h>
#include <Ethernet_Generic.hpp> // только декларации (без реализации)

class WebUI
{
public:
	explicit WebUI(uint16_t port = 80);

	void begin(); // поднимет server + создаст дефолтную auth запись в EEPROM при отсутствии
	void tick();

	static void htmlEscPrint(EthernetClient& c, const char* s);

private:
	EthernetServer _srv;

	// ------------ HTTP ------------
	void handleClient(EthernetClient& c);

	bool readLine(EthernetClient& c, char* out, size_t outN, uint32_t timeoutMs);

	// читает заголовки,вытаскивает BasicAuth(b64) и Content-Length
	bool readHeaders(EthernetClient& c,
		char* authB64, size_t authB64N,
		size_t& contentLength,
		uint32_t timeoutMs);

	// читает POST body (x-www-form-urlencoded) по Content-Length
	bool readBody(EthernetClient& c, char* out, size_t outN, size_t contentLength, uint32_t timeoutMs);

	bool checkAuth(const char* authB64) const;
	void sendUnauthorized(EthernetClient& c);

	static bool startsWithI(const char* s, const char* pref); // case-insensitive
	static bool startsWith(const char* s, const char* pref);

	static void urlDecodeInPlace(char* s);
	static const char* findParam(char* query, const char* key);
	static bool hasParam(char* query, const char* key); // <-- ДОБАВЛЕНО (для checkbox)

	static bool parseBool01(const char* s, bool& out);
	static bool parseU8(const char* s, uint8_t& out, uint8_t minV, uint8_t maxV);
	static bool parseI8(const char* s, int8_t& out, int8_t minV, int8_t maxV);
	static bool parseIp4(const char* s, uint8_t out[4]);

	void sendHeader(EthernetClient& c, int code, const __FlashStringHelper* ct);
	void sendRedirect(EthernetClient& c, const char* location);

	// ------------ Pages ------------
	void pageStatus(EthernetClient& c);
	void pageCfg(EthernetClient& c);
	void pageGsm(EthernetClient& c);
	void pageNotFound(EthernetClient& c);

	// ------------ JSON ------------
	void apiStatus(EthernetClient& c);
	void apiConfig(EthernetClient& c);

	// ------------ Actions ------------
	// params = pointer на буфер с "a=1&b=2" (из query или body)
	void actionCfgSave(EthernetClient& c, char* params);
	void actionGsmSave(EthernetClient& c, char* params);

	// ------------ EEPROM auth (0x50 / 0x0190..) ------------
	// ХРАНИМ как раньше:user/pass для HTTP BasicAuth
	struct AuthRec {
		char user[32];
		char pass[32];
	};

	bool authLoad(AuthRec& out) const;
	bool authSave(const AuthRec& in);
	void authLoadOrDefaults(AuthRec& out, bool& existed) const;

	// ------------ low-level EEPROM I2C helpers ------------
	static constexpr uint8_t EEPROM_ADDR = 0x50;
	static constexpr uint8_t EEPROM_PAGE = 64;

	bool eepromRead(uint16_t memAddr, uint8_t* out, size_t len) const;
	bool eepromWrite(uint16_t memAddr, const uint8_t* data, size_t len);
	bool eepromWritePage(uint16_t memAddr, const uint8_t* data, size_t len);
	bool eepromWaitReady(uint32_t timeoutMs = 50);

	static void trimSpaces(char* s);

	// ------------ Base64 ------------
	static bool base64Encode(char* out, size_t outN, const uint8_t* in, size_t inN);

	// helpers
	static void printIpOct(EthernetClient& c, const uint8_t ip[4]);
};