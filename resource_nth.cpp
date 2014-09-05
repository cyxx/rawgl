
#include <sys/stat.h>
#include <zlib.h>
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

static uint8_t *inflateGzip(const char *filepath) {
	File f;
	if (!f.open(filepath)) {
		warning("Unable to open '%s'", filepath);
		return 0;
	}
	const uint16_t sig = f.readUint16LE();
	if (sig != 0x8B1F) {
		warning("Unexpected file signature 0x%x for '%s'", sig, filepath);
		return 0;
	}
	f.seek(-4, SEEK_END);
	const uint32_t dataSize = f.readUint32LE();
	uint8_t *out = (uint8_t *)malloc(dataSize);
	if (!out) {
		warning("Failed to allocate %d bytes",  dataSize);
		return 0;
	}
	f.seek(0);
	z_stream str;
	memset(&str, 0, sizeof(str));
	int err = inflateInit2(&str, MAX_WBITS + 16);
	if (err == Z_OK) {
		uint8_t buf[1 << MAX_WBITS];
		str.next_in = buf;
		str.avail_in = 0;
		str.next_out = out;
		str.avail_out = dataSize;
		while (err == Z_OK && str.avail_out != 0) {
			if (str.avail_in == 0 && !f.ioErr()) {
				str.next_in = buf;
				str.avail_in = f.read(buf, sizeof(buf));
			}
			err = inflate(&str, Z_NO_FLUSH);
		}
		inflateEnd(&str);
		if (err == Z_STREAM_END) {
			return out;
		}
	}
	free(out);
	return 0;
}

struct Resource20th: ResourceNth {
	const char *_dataPath;

	Resource20th(const char *dataPath)
		: _dataPath(dataPath) {
	}

	virtual bool init() {
		static const char *dirs[] = { "BGZ", "DAT", "WGZ", 0 };
		for (int i = 0; dirs[i]; ++i) {
			char path[512];
			snprintf(path, sizeof(path), "%s/game/%s", _dataPath, dirs[i]);
			struct stat s;
			if (stat(path, &s) != 0 || !S_ISDIR(s.st_mode)) {
				warning("'%s' is not a directory", path);
				return false;
			}
		}
		return true;
	}

	virtual uint8_t *load(const char *name) {
		if (strcmp(name, "font.bmp") == 0) {
			char path[512];
			snprintf(path, sizeof(path), "%s/game/BGZ/Font.bgz", _dataPath);
			return inflateGzip(path);
		}
		return 0;
	}

	virtual uint8_t *loadBmp(int num) {
		char path[512];
		if (num >= 3000) {
			snprintf(path, sizeof(path), "%s/game/BGZ/data1280x800/1280x800_e%04d.bgz", _dataPath, num);
		} else {
			snprintf(path, sizeof(path), "%s/game/BGZ/file%03d.bgz", _dataPath, num);
		}
		return inflateGzip(path);
	}

	virtual uint8_t *loadDat(int num, uint8_t *dst, uint32_t *size) {
		char path[512];
		snprintf(path, sizeof(path), "%s/game/DAT/FILE%03d.DAT", _dataPath, num);
		File f;
		if (f.open(path)) {
			const uint32_t dataSize = f.size();
			const uint32_t count = f.read(dst, dataSize);
			if (count != dataSize) {
				warning("Failed to read %d bytes (expected %d)", dataSize, count);
			}
			*size = dataSize;
			return dst;
		} else {
			warning("Unable to open '%s'", path);
		}
		return 0;
	}

	virtual uint8_t *loadWav(int num, uint8_t *dst, uint32_t *size) {
		char path[512];
		snprintf(path, sizeof(path), "%s/game/WGZ/file%03d.wgz", _dataPath, num);
		struct stat s;
		if (stat(path, &s) != 0) {
			snprintf(path, sizeof(path), "%s/game/WGZ/file%03dB.wgz", _dataPath, num);
		}
		return inflateGzip(path);
	}
};

ResourceNth *ResourceNth::create(int edition, const char *dataPath) {
	switch (edition) {
	case 15:
		return new Resource15th(dataPath);
	case 20:
		return new Resource20th(dataPath);
	}
	return 0;
}

