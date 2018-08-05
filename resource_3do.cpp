
#include <sys/stat.h>
#include <unistd.h>
#include "resource_3do.h"
#include "util.h"

static const int ISO_BLOCK_SIZE = 2048;

struct OperaIsoEntry {
	char name[16];
	uint32_t offset;
	uint32_t size;
};

static int compareOperaIsoEntry(const void *a, const void *b) {
	const OperaIsoEntry *entry1 = (const OperaIsoEntry *)a;
	const OperaIsoEntry *entry2 = (const OperaIsoEntry *)b;
	return strcmp(entry1->name, entry2->name);
}

static int compareOperaIsoEntryName(const void *name, const void *p) {
	const OperaIsoEntry *entry = (const OperaIsoEntry *)p;
	return strcmp((const char *)name, entry->name);
}

struct OperaIso {
	File _f;
	OperaIsoEntry *_entries;
	int _entriesCount;

	OperaIso(const char *filePath)
		: _entries(0), _entriesCount(0) {
		_f.open(filePath);
	}
	~OperaIso() {
		free(_entries);
	}
	void readToc() {
		uint8_t buf[128];
		const int count = _f.read(buf, sizeof(buf));
		if (count != sizeof(buf)) {
			warning("Failed to read %d bytes", count);
			return;
		}
		if (buf[0] != 1 || memcmp(buf + 40, "CD-ROM", 6) != 0) {
			warning("Unexpected Opera ISO signature");
			return;
		}
		const int block = READ_BE_UINT32(buf + 100);
		readTocEntry(block);
		qsort(_entries, _entriesCount, sizeof(OperaIsoEntry), compareOperaIsoEntry);
	}
	void readTocEntry(int block) {
		uint32_t attr = 0;
		do {
			_f.seek(block * ISO_BLOCK_SIZE + 20, SEEK_SET);
			do {
				uint8_t buf[72];
				_f.read(buf, sizeof(buf));
				attr = READ_BE_UINT32(buf);
				const char *name = (const char *)buf + 32;
				const uint32_t count = READ_BE_UINT32(buf + 64);
				const uint32_t offset = READ_BE_UINT32(buf + 68);
				_f.seek(count * 4, SEEK_CUR);
				switch (attr & 255) {
				case 2:
					_entries = (OperaIsoEntry *)realloc(_entries, (_entriesCount + 1) * sizeof(OperaIsoEntry));
					if (_entries) {
						OperaIsoEntry *e = &_entries[_entriesCount];
						strncpy(e->name, name, sizeof(e->name) - 1);
						e->name[sizeof(e->name) - 1] = 0;
						e->offset = offset * ISO_BLOCK_SIZE;
						e->size = READ_BE_UINT32(buf + 16);
						++_entriesCount;
					}
					break;
				case 7:
					if (strcmp(name, "GameData") == 0) {
						readTocEntry(offset);
					}
					break;
				}
			} while (attr != 0 && attr < 256);
			++block;
		} while ((attr >> 24) == 0x40);
	}
	const OperaIsoEntry *find(const char *name) const {
		return (const OperaIsoEntry *)bsearch(name, _entries, _entriesCount, sizeof(OperaIsoEntry), compareOperaIsoEntryName);
	}
};

static int decodeLzss(const uint8_t *src, uint32_t len, uint8_t *dst) {
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

Resource3do::Resource3do(const char *dataPath)
	: _dataPath(dataPath) {
	struct stat st;
	if (stat(dataPath, &st) == 0 && S_ISREG(st.st_mode)) {
		_iso = new OperaIso(dataPath);
	} else {
		_iso = 0;
	}
}

Resource3do::~Resource3do() {
	delete _iso;
}

bool Resource3do::readEntries() {
	if (_iso) {
		_iso->readToc();
		return _iso->_entriesCount != 0;
	}
	return true;
}

uint8_t *Resource3do::loadFile(int num, uint8_t *dst, uint32_t *size) {
	uint8_t *in = dst;
	if (_iso) {
		char name[16];
		snprintf(name, sizeof(name), "File%d", num);
		const OperaIsoEntry *e = _iso->find(name);
		if (e) {
			if (!dst) {
				dst = (uint8_t *)malloc(e->size);
				if (!dst) {
					warning("Unable to allocate %d bytes", e->size);
					return 0;
				}
			}
			*size = e->size;
			_iso->_f.seek(e->offset);
			_iso->_f.read(dst, e->size);
		} else {
			warning("Failed to load '%s'", name);
			return 0;
		}
	} else {
		char path[MAXPATHLEN];
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

const char *Resource3do::getMusicName(int num, uint32_t *offset) {
	*offset = 0;
	if (_iso) {
		char name[16];
		snprintf(name, sizeof(name), "song%d", num);
		const OperaIsoEntry *e = _iso->find(name);
		if (e) {
			*offset = e->offset;
		}
		return 0;
	}
	snprintf(_musicPath, sizeof(_musicPath), "GameData/song%d", num);
	return _musicPath;
}
