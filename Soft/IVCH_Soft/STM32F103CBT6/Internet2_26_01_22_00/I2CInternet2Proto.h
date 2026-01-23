#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "W5500NtpServerClient.h"

// I2C slave protocol @0x42
// Reg write:first byte is reg
// 0x10 SET_TIME_UTC:u32 unixUtc LE + u16 ms LE
// 0x20 NETCFG:u8 dhcp + ip[4]+mask[4]+gw[4]+dns[4]+upstream[4]+u32 periodMs
// 0x30 CMD:u8 cmd (1=sync now)
// Read:0x80 STATUS (16 bytes)
class I2CInternet2Proto {
public:
	static I2CInternet2Proto* instance;

	explicit I2CInternet2Proto(W5500NtpServerClient& n) :_n(n) {}

	static void onReceiveThunk(int n) { if (instance) instance->onReceive(n); }
	static void onRequestThunk() { if (instance) instance->onRequest(); }

	void resetReg() { _reg = 0x80; }

	void onReceive(int n);
	void onRequest();

private:
	W5500NtpServerClient& _n;
	uint8_t _reg = 0x80;

	void handleSetTime();
	void handleNetCfg();
	void handleCmd();

	void writeStatus();
};