
#include "pak.h"

static const uint32_t XOR_KEY2 = 0x22683297;

static uint8_t *decode_toodc(uint8_t *p, int count) {
	uint32_t key = XOR_KEY2;
	uint32_t acc = 0;
	for (int i = 0; i < count; ++i) {
		uint8_t *q = p + i * 4;
		const uint32_t data = READ_LE_UINT32(q) ^ key;
		uint32_t r = (q[2] + q[1] + q[0]) ^ q[3];
		r += acc;
		key += r;
		acc += 0x4D;
		WRITE_LE_UINT32(q, data);
	}
	return p + 4;
}

const char *Pak::FILENAME = "Pak01.pak";

Pak::Pak(const char *dataPath)
	: _entries(0), _entriesCount(0) {
	_f.open(FILENAME, dataPath);
}

Pak::~Pak() {
	free(_entries);
}

void Pak::readEntries() {
	uint8_t header[12];

	memset(header, 0, sizeof(header));
	_f.read(header, sizeof(header));
	if (_f.ioErr() || memcmp(header, "PACK", 4) != 0) {
		return;
	}
	const uint32_t entriesOffset = READ_LE_UINT32(header + 4);
	_f.seek(entriesOffset);
	const uint32_t entriesSize = READ_LE_UINT32(header + 8);
	_entriesCount = entriesSize / 0x40;
	debug(DBG_PAK, "Pak::readEntries() entries count %d", _entriesCount);
	_entries = (PakEntry *)malloc(_entriesCount * sizeof(PakEntry));
	if (!_entries) {
		_entriesCount = 0;
		return;
	}
	for (int i = 0; i < _entriesCount; ++i) {
		uint8_t buf[0x40];
		_f.read(buf, sizeof(buf));
		if (_f.ioErr()) {
			break;
		}
		const char *name = (const char *)buf;
		if (strncmp(name, "dlx/", 4) != 0 || strchr(name + 4, '/')) {
			continue;
		}
		PakEntry *e = &_entries[i];
		strcpy(e->name, name + 4);
		e->offset = READ_LE_UINT32(buf + 0x38);
		e->size = READ_LE_UINT32(buf + 0x3C);
		debug(DBG_PAK, "Pak::readEntries() buf '%s' size %d", e->name, e->size);
	}
}

const PakEntry *Pak::find(const char *name) {
	for (int i = 0; i < _entriesCount; ++i) {
		const PakEntry *e = &_entries[i];
		if (strcasecmp(e->name, name) == 0) {
			return e;
		}
	}
	return 0;
}

void Pak::loadData(const PakEntry *e, uint8_t *buf, uint32_t *size) {
	debug(DBG_PAK, "Pak::loadData() %d bytes from 0x%x", e->size, e->offset);
	_f.seek(e->offset);
	if (_f.ioErr()) {
		*size = 0;
		return;
	}
	_f.read(buf, e->size);
	if (e->size > 5 && memcmp(buf, "TooDC", 5) == 0) {
		const int dataSize = e->size - 6;
		debug(DBG_PAK, "Pak::loadData() encoded TooDC data, size %d", dataSize);
		if ((dataSize & 3) != 0) {
			// descrambler operates on uint32_t
			warning("Unexpected size %d for encoded TooDC data '%s'", dataSize, e->name);
		}
		*size = dataSize - 4;
		decode_toodc(buf + 6, (dataSize + 3) / 4);
		memmove(buf, buf + 10, dataSize - 4);
	} else {
		*size = e->size;
	}
}
