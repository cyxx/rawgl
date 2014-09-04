
#include "pak.h"
#include "resource_nth.h"

struct Resource15th: ResourceNth {
	Pak _pak;

	Resource15th(const char *dataPath)
		: _pak(dataPath) {
	}

	virtual bool init() {
		_pak.readEntries();
		return _pak._entriesCount != 0;
	}

	virtual uint8_t *load(const char *name) {
		uint8_t *buf = 0;
		const PakEntry *e = _pak.find(name);
		if (e) {
			buf = (uint8_t *)malloc(e->size);
			if (buf) {
				uint32_t size;
				_pak.loadData(e, buf, &size);
			}
		} else {
			warning("Unable to load '%s'", name);
		}
		return buf;
	}

	virtual uint8_t *loadBmp(int num) {
		char name[16];
		if (num >= 3000) {
			snprintf(name, sizeof(name), "e%04d.bmp", num);
		} else {
			snprintf(name, sizeof(name), "file%03d.bmp", num);
		}
		return load(name);
	}

	virtual uint8_t *loadDat(int num, uint8_t *dst, uint32_t *size) {
		char name[16];
		snprintf(name, sizeof(name), "file%03d.dat", num);
		const PakEntry *e = _pak.find(name);
		if (e) {
			_pak.loadData(e, dst, size);
			return dst;
		} else {
			warning("Unable to load '%s'", name);
		}
		return 0;
	}

	virtual uint8_t *loadWav(int num, uint8_t *dst, uint32_t *size) {
		char name[16];
		snprintf(name, sizeof(name), "file%03d.wav", num);
		const PakEntry *e = _pak.find(name);
		if (e) {
			_pak.loadData(e, dst, size);
			return dst;
		} else {
			warning("Unable to load '%s'", name);
		}
		return 0;
	}
};

ResourceNth *ResourceNth::create(int edition, const char *dataPath) {
	switch (edition) {
	case 15:
		return new Resource15th(dataPath);
	}
	return 0;
}

