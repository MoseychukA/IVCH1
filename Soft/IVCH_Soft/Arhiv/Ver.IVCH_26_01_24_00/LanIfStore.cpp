#include "LanIfStore.h"
#include <string.h> // memset,memcmp

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

	const uint16_t base = baseFor(id);

	uint32_t mg = 0;
	uint8_t ver = 0;

	const bool okMagic = readU32(base + OFF_MAGIC, mg) && (mg == magicFor(id));
	const bool okVer = readU8(base + OFF_VER, ver) && (ver == VERSION);

	// Жёстко:если заголовок не совпал — не читаем поля (не подхватываем мусор)
	if (!(okMagic && okVer)) {
		(void)strict; // параметр оставлен для совместимости,поведение одинаково
		return false;
	}

	// читаем поля (если что-то не прочиталось — оставляем то,что уже в cfg по умолчанию)
	(void)readU8(base + OFF_DHCP, cfg.dhcp);

	(void)readBytes(base + OFF_IP, cfg.ip, 4);
	(void)readBytes(base + OFF_MASK, cfg.mask, 4);
	(void)readBytes(base + OFF_GW, cfg.gw, 4);
	(void)readBytes(base + OFF_DNS, cfg.dns, 4);

	(void)readU8(base + OFF_NTPIDX, cfg.ntpIdx);
	(void)readU8(base + OFF_PERIOD, cfg.periodIdx);

	// нормализация
	cfg.dhcp = cfg.dhcp ? 1 : 0;
	cfg.ntpIdx %= 5;
	cfg.periodIdx %= 6;

	return true;
}

bool LanIfStore::save(IfId id, const IfConfig& in, bool writeMagic) {
	const uint16_t base = baseFor(id);

	IfConfig cfg = in;
	cfg.dhcp = cfg.dhcp ? 1 : 0;
	cfg.ntpIdx %= 5;
	cfg.periodIdx %= 6;

	// Пишем безопасным порядком:
	// 1) данные
	// 2) version
	// 3) magic в самом конце (commit)
	if (!writeU8(base + OFF_DHCP, cfg.dhcp)) return false;
	if (!writeBytes(base + OFF_IP, cfg.ip, 4)) return false;
	if (!writeBytes(base + OFF_MASK, cfg.mask, 4)) return false;
	if (!writeBytes(base + OFF_GW, cfg.gw, 4)) return false;
	if (!writeBytes(base + OFF_DNS, cfg.dns, 4)) return false;
	if (!writeU8(base + OFF_NTPIDX, cfg.ntpIdx)) return false;
	if (!writeU8(base + OFF_PERIOD, cfg.periodIdx)) return false;

	if (writeMagic) {
		if (!writeU8(base + OFF_VER, VERSION)) return false;
		if (!writeU32(base + OFF_MAGIC, magicFor(id))) return false;

		// Верификация:читаем назад строго и сравниваем
		IfConfig rd;
		if (!load(id, rd, true)) return false;

		if (rd.dhcp != cfg.dhcp) return false;
		if (memcmp(rd.ip, cfg.ip, 4) != 0) return false;
		if (memcmp(rd.mask, cfg.mask, 4) != 0) return false;
		if (memcmp(rd.gw, cfg.gw, 4) != 0) return false;
		if (memcmp(rd.dns, cfg.dns, 4) != 0) return false;
		if (rd.ntpIdx != cfg.ntpIdx) return false;
		if (rd.periodIdx != cfg.periodIdx) return false;
	}

	return true;
}

// ---- low level I2C (libmaple compatible) ----
bool LanIfStore::readBytes(uint16_t memAddr, uint8_t* out, size_t len) {
	_w->beginTransmission(_addr);
	_w->write((uint8_t)(memAddr >> 8));
	_w->write((uint8_t)(memAddr & 0xFF));

	// repeated-start
	if (_w->endTransmission(false) != 0) return false;

	size_t got = _w->requestFrom((int)_addr, (int)len);
	if (got != len) return false;

	for (size_t i = 0; i < len; i++) {
		int v = _w->read();
		if (v < 0) return false;
		out[i] = (uint8_t)v;
	}
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
	v = (uint32_t)b[0]
		| ((uint32_t)b[1] << 8)
		| ((uint32_t)b[2] << 16)
		| ((uint32_t)b[3] << 24);
	return true;
}

bool LanIfStore::writeU32(uint16_t memAddr, uint32_t v) {
	uint8_t b[4] = { (uint8_t)v,(uint8_t)(v >> 8),(uint8_t)(v >> 16),(uint8_t)(v >> 24) };
	return writeBytes(memAddr, b, 4);
}

bool LanIfStore::readU8(uint16_t memAddr, uint8_t& v) { return readBytes(memAddr, &v, 1); }
bool LanIfStore::writeU8(uint16_t memAddr, uint8_t v) { return writeBytes(memAddr, &v, 1); }