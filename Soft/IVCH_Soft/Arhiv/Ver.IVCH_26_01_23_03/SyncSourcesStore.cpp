#include "SyncSourcesStore.h"

SyncSourcesStore::SyncSourcesStore(TwoWire& w, uint8_t addr)
	:_w(&w), _addr(addr)
{}

bool SyncSourcesStore::begin()
{
	_w->beginTransmission(_addr);
	return (_w->endTransmission() == 0);
}

void SyncSourcesStore::defaults(Data& d)
{
	d.gpsEnable = 0; d.gpsPeriodIdx = 0;

	d.netEnable = 0; d.netProviderIdx = 0; d.netPeriodIdx = 0;

	d.gsmEnable = 1; d.gsmProviderIdx = 0; d.gsmPeriodIdx = 0;

	// NEW defaults:
	d.net2Enable = 0; // по умолчанию выключен (включите в меню)
	d.net2PeriodIdx = 3; // 1 час
}

// --- helpers to work with any base (new/old) ---
static inline uint16_t ss_addr_magic(uint16_t base) { return base + 0; } // u16
static inline uint16_t ss_addr_ver(uint16_t base) { return base + 2; } // u8
static inline uint16_t ss_addr_data(uint16_t base) { return base + 4; } // bytes

bool SyncSourcesStore::load(Data& d)
{
	// strict load ONLY from new BASE
	defaults(d);

	uint16_t mg = 0;
	uint8_t ver = 0;

	if (!readU16(ss_addr_magic(BASE), mg) || mg != MAGIC) return false;
	if (!readU8(ss_addr_ver(BASE), ver) || ver != VERSION) return false;

	return readBytes(ss_addr_data(BASE), (uint8_t*)&d, sizeof(d));
}

bool SyncSourcesStore::loadLoose(Data& d)
{
	defaults(d);

	auto tryLoadFromBase = [&](uint16_t base, Data& out)->int {
		// return:0 = no data/invalid,1 = loaded ver=VERSION,2 = loaded ver=1,3 = loaded other ver (unsupported)
		uint16_t mg = 0;
		if (!readU16(ss_addr_magic(base), mg)) return 0;
		if (mg != MAGIC) return 0;

		uint8_t ver = 0;
		if (!readU8(ss_addr_ver(base), ver)) return 0;

		if (ver == VERSION)
		{
			if (!readBytes(ss_addr_data(base), (uint8_t*)&out, sizeof(out))) return 0;
			return 1;
		}

		if (ver == 1)
		{
			uint8_t b[OLD_SIZE_V1];
			if (!readBytes(ss_addr_data(base), b, sizeof(b))) return 0;

			out = Data{}; // обнулим всё
			out.gpsEnable = b[0];
			out.gpsPeriodIdx = b[1];

			out.netEnable = b[2];
			out.netProviderIdx = b[3];
			out.netPeriodIdx = b[4];

			out.gsmEnable = b[5];
			out.gsmProviderIdx = b[6];
			out.gsmPeriodIdx = b[7];

			// новые поля оставляем дефолтами (как договорились)
			out.net2Enable = 0;
			out.net2PeriodIdx = 3;

			return 2;
		}

		return 3; // magic есть,но версия неизвестна
	};

	// 1) Сначала пробуем новую область
	Data tmp;
	int r = tryLoadFromBase(BASE, tmp);
	if (r == 1)
	{
		d = tmp;
		return true;
	}
	else if (r == 2)
	{
		// обнаружили V1 уже в новой области -> обновим запись до V2
		d = tmp;
		(void)save(d); // best-effort
		return true;
	}
	else if (r == 3)
	{
		// неизвестная версия в новой области -> defaults
		return false;
	}

	// 2) Новой области нет -> пробуем старую (для миграции)
	r = tryLoadFromBase(OLD_BASE, tmp);
	if (r == 1 || r == 2)
	{
		// миграция:сохраняем в новую область
		d = tmp;
		(void)save(d);
		return true;
	}

	// 3) Ничего валидного — defaults
	return false;
}

bool SyncSourcesStore::save(const Data& d)
{
	// пишем в НОВУЮ область BASE
	if (!writeU16(ss_addr_magic(BASE), MAGIC)) return false;
	if (!writeU8(ss_addr_ver(BASE), VERSION)) return false;
	return writeBytes(ss_addr_data(BASE), (const uint8_t*)&d, sizeof(d));
}

// ---- low level (libmaple compatible) ----
bool SyncSourcesStore::readBytes(uint16_t memAddr, uint8_t* out, size_t len)
{
	_w->beginTransmission(_addr);
	_w->write((uint8_t)(memAddr >> 8));
	_w->write((uint8_t)(memAddr & 0xFF));

	// STM32duino поддерживает endTransmission(false) = repeated-start
	if (_w->endTransmission(false) != 0) return false;

	size_t got = _w->requestFrom((int)_addr, (int)len);
	if (got != len) return false;

	for (size_t i = 0; i < len; i++)
	{
		int v = _w->read();
		if (v < 0) return false;
		out[i] = (uint8_t)v;
	}
	return true;
}

bool SyncSourcesStore::writeBytes(uint16_t memAddr, const uint8_t* data, size_t len)
{
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

bool SyncSourcesStore::writePage(uint16_t memAddr, const uint8_t* data, size_t len)
{
	_w->beginTransmission(_addr);
	_w->write((uint8_t)(memAddr >> 8));
	_w->write((uint8_t)(memAddr & 0xFF));
	_w->write((uint8_t*)data, (int)len); // libmaple требует non-const
	return (_w->endTransmission() == 0);
}

bool SyncSourcesStore::waitReady(uint32_t timeoutMs)
{
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

bool SyncSourcesStore::readU16(uint16_t memAddr, uint16_t& v)
{
	uint8_t b[2];
	if (!readBytes(memAddr, b, 2)) return false;
	v = (uint16_t)b[0] | ((uint16_t)b[1] << 8);
	return true;
}

bool SyncSourcesStore::writeU16(uint16_t memAddr, uint16_t v)
{
	uint8_t b[2] = { (uint8_t)(v & 0xFF),(uint8_t)((v >> 8) & 0xFF) };
	return writeBytes(memAddr, b, 2);
}