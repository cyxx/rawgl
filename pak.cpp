
#include "pak.h"

extern uint8_t *decode_toodc(uint8_t *p, int count);

const char *Pak::FILENAME = "Pak01.pak";

Pak::Pak(const char *dataPath)
	: _entries(0), _entriesCount(0) {
	_f.open(FILENAME, dataPath);
}

Pak::~Pak() {
	free(_entries);
}

void Pak::readEntries() {
	uint8_t header[8];

	memset(header, 0, sizeof(header));
	_f.read(header, sizeof(header));
	if (_f.ioErr() || memcmp(header, "PACK", 4) != 0) {
		return;
	}
	const uint32_t entriesOffset = READ_LE_UINT32(header + 4);
	_f.seek(entriesOffset);

	uint8_t entry[0x40];
	int count = 0;
	while (1) {
		_f.read(entry, sizeof(entry));
		if (_f.ioErr()) {
			break;
		}
		if (strncmp((const char *)entry, "dlx/", 4) != 0 || strchr((const char *)entry + 4, '/')) {
			continue;
		}
		++count;
		if (count > _entriesCount) {
			_entriesCount = count * 2;
			_entries = (PakEntry *)realloc(_entries, sizeof(PakEntry) * _entriesCount);
		}
		if (_entries) {
			PakEntry *e = &_entries[count - 1];
			strcpy(e->name, (const char *)entry + 4);
			e->offset = READ_LE_UINT32(entry + 0x38);
			e->size = READ_LE_UINT32(entry + 0x3C);
			debug(DBG_PAK, "Pak::readEntries() entry '%s' size %d", e->name, e->size);
		}
	}
	_entriesCount = count;
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
