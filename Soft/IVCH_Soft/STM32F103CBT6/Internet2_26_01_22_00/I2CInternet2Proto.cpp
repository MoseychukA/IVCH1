#include "I2CInternet2Proto.h"

I2CInternet2Proto* I2CInternet2Proto::instance = nullptr;

static uint32_t readU32LE(TwoWire& w) {
	uint32_t v = 0;
	v |= (uint32_t)(uint8_t)w.read();
	v |= (uint32_t)(uint8_t)w.read() << 8;
	v |= (uint32_t)(uint8_t)w.read() << 16;
	v |= (uint32_t)(uint8_t)w.read() << 24;
	return v;
}
static uint16_t readU16LE(TwoWire& w) {
	uint16_t v = 0;
	v |= (uint16_t)(uint8_t)w.read();
	v |= (uint16_t)(uint8_t)w.read() << 8;
	return v;
}

void I2CInternet2Proto::onReceive(int n) {
	if (n <= 0) return;

	_reg = (uint8_t)Wire.read(); // first byte = reg
	n--;

	switch (_reg) {
	case 0x10:handleSetTime(); break;
	case 0x20:handleNetCfg(); break;
	case 0x30:handleCmd(); break;
	default:
		while (Wire.available()) Wire.read();
		break;
	}
}

void I2CInternet2Proto::handleSetTime() {
	if (Wire.available() < 4) { while (Wire.available()) Wire.read(); return; }
	uint32_t unixUtc = readU32LE(Wire);
	uint16_t ms = 0;
	if (Wire.available() >= 2) ms = readU16LE(Wire);
	_n.setMasterTimeUtc(unixUtc, ms);
	while (Wire.available()) Wire.read();
}

void I2CInternet2Proto::handleNetCfg() {
	if (Wire.available() < (1 + 4 * 5 + 4)) { while (Wire.available()) Wire.read(); return; }

	W5500NtpServerClient::NetCfg c;
	c.dhcp = (uint8_t)Wire.read();

	for (int i = 0; i < 4; i++) c.ip[i] = (uint8_t)Wire.read();
	for (int i = 0; i < 4; i++) c.mask[i] = (uint8_t)Wire.read();
	for (int i = 0; i < 4; i++) c.gw[i] = (uint8_t)Wire.read();
	for (int i = 0; i < 4; i++) c.dns[i] = (uint8_t)Wire.read();
	for (int i = 0; i < 4; i++) c.upstream[i] = (uint8_t)Wire.read();

	c.periodMs = readU32LE(Wire);

	_n.applyNetCfg(c);
	while (Wire.available()) Wire.read();
}

void I2CInternet2Proto::handleCmd() {
	if (Wire.available() < 1) { while (Wire.available()) Wire.read(); return; }
	uint8_t cmd = (uint8_t)Wire.read();
	if (cmd == 1) _n.requestSyncNow();
	while (Wire.available()) Wire.read();
}

void I2CInternet2Proto::onRequest() {
	// Master does:write(reg) then requestFrom
	if (_reg == 0x80) writeStatus();
	else writeStatus();
}

void I2CInternet2Proto::writeStatus() {
	auto st = _n.status();

	uint8_t buf[16] = { 0 };
	buf[0] = 1; // protoVer

	buf[1] = 0;
	if (st.linkUp) buf[1] |= (1 << 0);
	if (st.dhcp) buf[1] |= (1 << 1);
	if (st.haveMasterTime) buf[1] |= (1 << 2);
	if (st.syncInProgress) buf[1] |= (1 << 3);

	buf[2] = st.ip[0]; buf[3] = st.ip[1]; buf[4] = st.ip[2]; buf[5] = st.ip[3];

	int32_t off = st.lastOffsetSec;
	buf[6] = (uint8_t)off; buf[7] = (uint8_t)(off >> 8); buf[8] = (uint8_t)(off >> 16); buf[9] = (uint8_t)(off >> 24);

	uint32_t ntp = st.lastNtpUtc;
	buf[10] = (uint8_t)ntp; buf[11] = (uint8_t)(ntp >> 8); buf[12] = (uint8_t)(ntp >> 16); buf[13] = (uint8_t)(ntp >> 24);

	buf[14] = st.lastSyncOk ? 1 : 0;
	buf[15] = 0;

	Wire.write(buf, sizeof(buf));
}