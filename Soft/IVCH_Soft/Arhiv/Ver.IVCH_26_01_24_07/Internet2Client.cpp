#include "Internet2Client.h"

Internet2Client::Internet2Client(TwoWire& w, uint8_t addr)
	:_w(&w), _addr(addr) {}

bool Internet2Client::begin() {
	_w->beginTransmission(_addr);
	return (_w->endTransmission() == 0);
}

bool Internet2Client::writeReg(uint8_t reg, const uint8_t* data, size_t len) {
	_w->beginTransmission(_addr);
	_w->write(reg);
	for (size_t i = 0; i < len; i++) _w->write(data[i]);
	return (_w->endTransmission() == 0);
}

bool Internet2Client::readReg(uint8_t reg, uint8_t* data, size_t len) {
	_w->beginTransmission(_addr);
	_w->write(reg);

	// STM32duino поддерживает endTransmission(false)
	if (_w->endTransmission(false) != 0) return false;

	size_t got = _w->requestFrom((int)_addr, (int)len);
	if (got != len) return false;

	for (size_t i = 0; i < len; i++) data[i] = (uint8_t)_w->read();
	return true;
}

bool Internet2Client::setTimeUnixUtc(uint32_t unixUtc, uint16_t ms) {
	uint8_t b[6];
	b[0] = (uint8_t)(unixUtc);
	b[1] = (uint8_t)(unixUtc >> 8);
	b[2] = (uint8_t)(unixUtc >> 16);
	b[3] = (uint8_t)(unixUtc >> 24);
	b[4] = (uint8_t)(ms);
	b[5] = (uint8_t)(ms >> 8);
	return writeReg(0x10, b, sizeof(b));
}

bool Internet2Client::applyNetCfg(bool dhcp,
	IPAddress ip,
	IPAddress mask,
	IPAddress gw,
	IPAddress dns,
	IPAddress ntpUpstream,
	uint32_t periodMs) {

	uint8_t b[1 + 4 * 5 + 4];
	b[0] = dhcp ? 1 : 0;

	int k = 1;
	auto putIp = [&](IPAddress a) {
		b[k++] = a[0]; b[k++] = a[1]; b[k++] = a[2]; b[k++] = a[3];
	};

	putIp(ip);
	putIp(mask);
	putIp(gw);
	putIp(dns);
	putIp(ntpUpstream);

	b[k++] = (uint8_t)(periodMs);
	b[k++] = (uint8_t)(periodMs >> 8);
	b[k++] = (uint8_t)(periodMs >> 16);
	b[k++] = (uint8_t)(periodMs >> 24);

	return writeReg(0x20, b, sizeof(b));
}

bool Internet2Client::requestSyncNow() {
	uint8_t cmd = 1;
	return writeReg(0x30, &cmd, 1);
}

bool Internet2Client::requestNetReinit() {
	uint8_t cmd = 2; // NEW cmd
	return writeReg(0x30, &cmd, 1);
}

bool Internet2Client::readStatus(Status& out) {
	uint8_t b[16];
	if (!readReg(0x80, b, sizeof(b))) return false;

	out.protoVer = b[0];

	out.raw1 = b[1];
	out.flags = (uint8_t)(b[1] & 0x0F);
	out.lastErr = (uint8_t)((b[1] >> 4) & 0x0F);

	out.ip = IPAddress(b[2], b[3], b[4], b[5]);

	out.lastOffsetSec = (int32_t)(
		(uint32_t)b[6] |
		((uint32_t)b[7] << 8) |
		((uint32_t)b[8] << 16) |
		((uint32_t)b[9] << 24));

	out.lastNtpUtc =
		(uint32_t)b[10] |
		((uint32_t)b[11] << 8) |
		((uint32_t)b[12] << 16) |
		((uint32_t)b[13] << 24);

	out.lastSyncOk = (b[14] != 0);

	// NEW (old slaves used to send 0 here)
	out.netReinitCount = b[15];

	return true;
}