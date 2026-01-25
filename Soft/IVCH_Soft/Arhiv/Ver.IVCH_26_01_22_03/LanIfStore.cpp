#include "LanIfStore.h"

LanIfStore::LanIfStore(uint8_t i2cAddr, TwoWire& w) :_w(&w), _addr(i2cAddr) {}

bool LanIfStore::begin() {
	_w->beginTransmission(_addr);
	return (_w->endTransmission() == 0);
}

void LanIfStore::defaults(IfId id, IfConfig& cfg) {
	memset(&cfg, 0, sizeof(cfg));
	cfg.dhcp = 1;

	// дефолтные статические (на случай DHCP=0)
	if (id == IF1) {
		cfg.ip[0] = 192; cfg.ip[1] = 168; cfg.ip[2] = 75; cfg.ip[3] = 231;
	}
	else {
		cfg.ip[0] = 192; cfg.ip[1] = 168; cfg.ip[2] = 75; cfg.ip[3] = 232;
	}
	cfg.mask[0] = 255; cfg.mask[1] = 255; cfg.mask[2] = 255; cfg.mask[3] = 0;
	cfg.gw[0] = 192; cfg.gw[1] = 168; cfg.gw[2] = 75; cfg.gw[3] = 1;
	cfg.dns[0] = 8; cfg.dns[1] = 8; cfg.dns[2] = 8; cfg.dns[3] = 8;

	cfg.ntpIdx = 0;
	cfg.periodIdx = 3; // 1 час
}

bool LanIfStore::load(IfId id, IfConfig& cfg, bool strict) {
	defaults(id, cfg);

	uint16_t base = baseFor(id);

	uint32_t mg = 0;
	uint8_t ver = 0;
	bool okMagic = readU32(base + 0, mg) && (mg == magicFor(id));
	bool okVer = readU8(base + 4, ver) && (ver == VERSION);

	if (strict && !(okMagic && okVer)) return false;

	// layout:
	// 0..3 magic,4 ver,5 dhcp,6..9 ip,10..13 mask,14..17 gw,18..21 dns,22 ntpIdx,23 periodIdx
	(void)readU8(base + 5, cfg.dhcp);

	(void)readBytes(base + 6, cfg.ip, 4);
	(void)readBytes(base + 10, cfg.mask, 4);
	(void)readBytes(base + 14, cfg.gw, 4);
	(void)readBytes(base + 18, cfg.dns, 4);

	(void)readU8(base + 22, cfg.ntpIdx);
	(void)readU8(base + 23, cfg.periodIdx);

	cfg.dhcp = cfg.dhcp ? 1 : 0;
	cfg.ntpIdx %= 5;
	cfg.periodIdx %= 6;

	return true;
}

bool LanIfStore::save(IfId id, const IfConfig& in, bool writeMagic) {
	uint16_t base = baseFor(id);

	IfConfig cfg = in;
	cfg.dhcp = cfg.dhcp ? 1 : 0;
	cfg.ntpIdx %= 5;
	cfg.periodIdx %= 6;

	if (writeMagic) {
		if (!writeU32(base + 0, magicFor(id))) return false;
		if (!writeU8(base + 4, VERSION)) return false;
	}

	if (!writeU8(base + 5, cfg.dhcp)) return false;
	if (!writeBytes(base + 6, cfg.ip, 4)) return false;
	if (!writeBytes(base + 10, cfg.mask, 4)) return false;
	if (!writeBytes(base + 14, cfg.gw, 4)) return false;
	if (!writeBytes(base + 18, cfg.dns, 4)) return false;
	if (!writeU8(base + 22, cfg.ntpIdx)) return false;
	if (!writeU8(base + 23, cfg.periodIdx)) return false;

	return true;
}

// ---- low level I2C (libmaple compatible) ----
bool LanIfStore::readBytes(uint16_t memAddr, uint8_t* out, size_t len) {
	_w->beginTransmission(_addr);
	_w->write((uint8_t)(memAddr >> 8));
	_w->write((uint8_t)(memAddr & 0xFF));
	if (_w->endTransmission() != 0) return false;

	uint8_t got = _w->requestFrom((uint8_t)_addr, (int)len);
	if (got != (uint8_t)len) return false;

	for (size_t i = 0; i < len; i++) out[i] = (uint8_t)_w->read();
	return true;
}

bool LanIfStore::writeBytes(uint16_t memAddr, const uint8_t* data, size_t len) {
	size_t off = 0;
	while (off < len) {
		uint16_t a = memAddr + (uint16_t)off;
		uint8_t pageOff = (uint8_t)(a % PAGE_SIZE);
		size_t chunk = min((size_t)(PAGE_SIZE - pageOff), len - off);
		if (!writePage(a, data + off, chunk)) return false;
		if (!waitReady(50)) return false;
		off += chunk;
	}
	return true;
}

bool LanIfStore::writePage(uint16_t memAddr, const uint8_t* data, size_t len) {
	_w->beginTransmission(_addr);
	_w->write((uint8_t)(memAddr >> 8));
	_w->write((uint8_t)(memAddr & 0xFF));
	_w->write((uint8_t*)data, (int)len); // libmaple требует non-const
	return (_w->endTransmission() == 0);
}

bool LanIfStore::waitReady(uint32_t timeoutMs) {
	uint32_t t0 = millis();
	while ((millis() - t0) < timeoutMs) {
		_w->beginTransmission(_addr);
		if (_w->endTransmission() == 0) return true;
		delay(1);
	}
	return false;
}

bool LanIfStore::readU32(uint16_t memAddr, uint32_t& v) {
	uint8_t b[4];
	if (!readBytes(memAddr, b, 4)) return false;
	v = (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
	return true;
}
bool LanIfStore::writeU32(uint16_t memAddr, uint32_t v) {
	uint8_t b[4] = { (uint8_t)v,(uint8_t)(v >> 8),(uint8_t)(v >> 16),(uint8_t)(v >> 24) };
	return writeBytes(memAddr, b, 4);
}
bool LanIfStore::readU8(uint16_t memAddr, uint8_t& v) { return readBytes(memAddr, &v, 1); }
bool LanIfStore::writeU8(uint16_t memAddr, uint8_t v) { return writeBytes(memAddr, &v, 1); }