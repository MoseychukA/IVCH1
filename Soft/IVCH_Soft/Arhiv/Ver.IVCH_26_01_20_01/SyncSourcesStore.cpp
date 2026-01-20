#include "SyncSourcesStore.h"

SyncSourcesStore::SyncSourcesStore(TwoWire& w, uint8_t addr) :_w(&w), _addr(addr) {}

bool SyncSourcesStore::begin() {
	_w->beginTransmission(_addr);
	return (_w->endTransmission() == 0);
}

void SyncSourcesStore::defaults(Data& d) {
	d.gpsEnable = 0; d.gpsPeriodIdx = 0;
	d.netEnable = 0; d.netProviderIdx = 0; d.netPeriodIdx = 0;
	d.gsmEnable = 1; d.gsmProviderIdx = 0; d.gsmPeriodIdx = 0;
}

bool SyncSourcesStore::load(Data& d) {
	defaults(d);
	uint16_t mg = 0; uint8_t ver = 0;
	if (!readU16(ADDR_MAGIC, mg) || mg != MAGIC) return false;
	if (!readU8(ADDR_VER, ver) || ver != VERSION) return false;
	return readBytes(ADDR_DATA, (uint8_t*)&d, sizeof(d));
}

bool SyncSourcesStore::loadLoose(Data& d) {
	if (!load(d)) { defaults(d); return false; }
	return true;
}

bool SyncSourcesStore::save(const Data& d) {
	if (!writeU16(ADDR_MAGIC, MAGIC)) return false;
	if (!writeU8(ADDR_VER, VERSION)) return false;
	return writeBytes(ADDR_DATA, (const uint8_t*)&d, sizeof(d));
}

// ---- low level (libmaple) ----
bool SyncSourcesStore::readBytes(uint16_t memAddr, uint8_t* out, size_t len) {
	_w->beginTransmission(_addr);
	_w->write((uint8_t)(memAddr >> 8));
	_w->write((uint8_t)(memAddr & 0xFF));
	if (_w->endTransmission() != 0) return false;

	uint8_t got = _w->requestFrom((uint8_t)_addr, (int)len);
	if (got != (uint8_t)len) return false;

	for (size_t i = 0; i < len; i++) out[i] = (uint8_t)_w->read();
	return true;
}

bool SyncSourcesStore::writeBytes(uint16_t memAddr, const uint8_t* data, size_t len) {
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

bool SyncSourcesStore::writePage(uint16_t memAddr, const uint8_t* data, size_t len) {
	_w->beginTransmission(_addr);
	_w->write((uint8_t)(memAddr >> 8));
	_w->write((uint8_t)(memAddr & 0xFF));
	_w->write((uint8_t*)data, (int)len); // libmaple требует non-const
	return (_w->endTransmission() == 0);
}

bool SyncSourcesStore::waitReady(uint32_t timeoutMs) {
	uint32_t t0 = millis();
	while ((millis() - t0) < timeoutMs) {
		_w->beginTransmission(_addr);
		if (_w->endTransmission() == 0) return true;
		delay(1);
	}
	return false;
}

bool SyncSourcesStore::readU8(uint16_t memAddr, uint8_t& v) { return readBytes(memAddr, &v, 1); }
bool SyncSourcesStore::writeU8(uint16_t memAddr, uint8_t v) { return writeBytes(memAddr, &v, 1); }

bool SyncSourcesStore::readU16(uint16_t memAddr, uint16_t& v) {
	uint8_t b[2]; if (!readBytes(memAddr, b, 2)) return false;
	v = (uint16_t)b[0] | ((uint16_t)b[1] << 8);
	return true;
}
bool SyncSourcesStore::writeU16(uint16_t memAddr, uint16_t v) {
	uint8_t b[2] = { (uint8_t)(v & 0xFF),(uint8_t)((v >> 8) & 0xFF) };
	return writeBytes(memAddr, b, 2);
}