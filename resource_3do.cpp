
#include "resource_3do.h"

static int decodeLzss(const uint8_t *src, int len, uint8_t *dst) {
	uint32_t rd = 0, wr = 0;
	int code = 0x100 | src[rd++];
	while (rd < len) {
		const bool bit = (code & 1);
		code >>= 1;
		if (bit) {
			dst[wr++] = src[rd++];
		} else {
			const int code1 = src[rd] | ((src[rd + 1] & 0xF) << 8); // offset
			const int code2 = (src[rd + 1] >> 4) + 3; // len
			rd += 2;
			const uint16_t offset = 0xF000 | code1;
			for (int i = 0; i < code2; ++i) {
				dst[wr] = dst[wr + (int16_t)offset];
				++wr;
			}
		}
		if (code == 1) {
			code = 0x100 | src[rd++];
		}
	}
	return wr;
}

OperaIso::OperaIso()
	: _entriesCount(0) {
	memset(_entries, 0, sizeof(_entries));
}

void OperaIso::readEntries(int block) {
	if (block < 0) {
		_entriesCount = 0;

		uint8_t buf[128];
		_f.read(buf, sizeof(buf));
		if (buf[0] == 1 && memcmp(buf + 40, "CD-ROM", 6) == 0) {
			const int offset = READ_BE_UINT32(buf + 100);
			readEntries(offset);
		}
	}  else {
		uint32_t attr = 0;
		do {
			_f.seek(block * BLOCKSIZE + 20);
			do {
				uint8_t buf[72];
				_f.read(buf, sizeof(buf));
				attr = READ_BE_UINT32(buf);
				const char *name = (const char *)buf + 32;
				const uint32_t count = READ_BE_UINT32(buf + 64);
				const uint32_t offset = READ_BE_UINT32(buf + 68);
				_f.seek(count * 4, SEEK_CUR);
				switch (attr & 255) {
				case 2: {
						assert(_entriesCount < 300);
						OperaEntry *e = &_entries[_entriesCount++];
						strcpy(e->name, name);
						e->offset = offset * BLOCKSIZE;
						e->size = READ_BE_UINT32(buf + 16);
					}
					break;
				case 7:
					if (strcmp(name, "GameData") == 0) {
						readEntries(offset);
					}
					break;
				}
			} while (attr < 256);
			++block;
		} while ((attr >> 24) == 0x40);
	}
}

const OperaEntry *OperaIso::findEntry(const char *name) const {
	for (int i = 0; i < _entriesCount; ++i) {
		if (strcmp(_entries[i].name, name) == 0) {
			return &_entries[i];
		}
	}
	return 0;
}

uint8_t *OperaIso::readFile(const char *name, uint8_t *dst, uint32_t *size) {
	const OperaEntry *e = findEntry(name);
	if (e) {
		*size = e->size;
		if (!dst) {
			dst = (uint8_t *)malloc(e->size);
			if (!dst) {
				warning("Unable to allocate %d bytes", e->size);
				return 0;
			}
		}
		_f.seek(e->offset);
		_f.read(dst, e->size);
		return dst;
	}
	warning("Unable to load file '%s'", name);
	return 0;
}

Resource3do::Resource3do(const char *dataPath)
	: _dataPath(dataPath), _useIso(false) {
}

Resource3do::~Resource3do() {
}

void Resource3do::readEntries() {
	if (_iso._f.open("ootw.iso", _dataPath)) {
		_iso.readEntries();
		_useIso = (_iso._entriesCount > 0);
	}
}

uint8_t *Resource3do::loadFile(int num, uint8_t *dst, uint32_t *size) {
	uint8_t *in = dst;
	if (_useIso) {
		char name[32];
		snprintf(name, sizeof(name), "File%d", num);
		dst = _iso.readFile(name, dst, size);
	} else {
		char path[512];
		snprintf(path, sizeof(path), "%s/GameData/File%d", _dataPath, num);
		File f;
		if (f.open(path)) {
			const int sz = f.size();
			if (!dst) {
				dst = (uint8_t *)malloc(sz);
				if (!dst) {
					warning("Unable to allocate %d bytes", sz);
					return 0;
				}
			}
			*size = sz;
			f.read(dst, sz);
		} else {
			warning("Failed to load '%s'", path);
			return 0;
		}
	}
	if (dst && memcmp(dst, "\x00\xf4\x01\x00", 4) == 0) {
		static const int SZ = 64000 * 2;
		uint8_t *tmp = (uint8_t *)calloc(1, SZ);
		if (!tmp) {
			warning("Unable to allocate %d bytes", SZ);
			if (in != dst) free(dst);
			return 0;
		}
		const int decodedSize = decodeLzss(dst + 4, *size - 4, tmp);
		if (in != dst) free(dst);
		if (decodedSize != SZ) {
			warning("Unexpected LZSS decoded size %d", decodedSize);
			return 0;
		}
		*size = decodedSize;
		return tmp;
	}
	return dst;
}

