
#include <string.h>
#include "resource_win31.h"

static const uint8_t _shuffleTable[256] = {
	0xB2, 0x91, 0x49, 0xEE, 0x8C, 0xBC, 0x16, 0x0D, 0x07, 0x87, 0xCD, 0xB6, 0x4C, 0x44, 0x22, 0xB3,
	0xAE, 0x96, 0xDF, 0x18, 0x7B, 0x28, 0x17, 0x9A, 0x74, 0x3C, 0x2E, 0x59, 0x69, 0x56, 0x38, 0x82,
	0x7F, 0x25, 0x41, 0xC6, 0xE8, 0x8A, 0x86, 0x7A, 0xB5, 0x8B, 0xA7, 0xB1, 0x2C, 0x53, 0xF0, 0x3B,
	0x20, 0xCB, 0x6F, 0x9E, 0xD9, 0x05, 0x54, 0x08, 0x4F, 0xFE, 0x32, 0x31, 0xF9, 0x50, 0xBD, 0x37,
	0x45, 0xDA, 0x46, 0x33, 0x01, 0xC5, 0x27, 0xEC, 0xE5, 0x14, 0x98, 0x70, 0xB0, 0xF8, 0x93, 0xC9,
	0xAC, 0xEB, 0xE4, 0xE1, 0xE6, 0xF7, 0xAF, 0x76, 0x0E, 0x63, 0x80, 0x83, 0x1E, 0x57, 0x47, 0x9F,
	0xC2, 0x42, 0xA5, 0xFF, 0x5B, 0xBF, 0x12, 0xFA, 0x61, 0x5E, 0x5D, 0xC8, 0x21, 0xA8, 0xB9, 0x5A,
	0x9D, 0x30, 0xD5, 0x09, 0xB7, 0x0B, 0x2F, 0xED, 0x6E, 0xA2, 0x5F, 0x6C, 0xA0, 0x95, 0x00, 0x55,
	0x75, 0x7D, 0x89, 0x97, 0x6A, 0xFB, 0x1A, 0x58, 0xDE, 0x8D, 0x4E, 0xE3, 0x4B, 0x3D, 0x15, 0x67,
	0x11, 0x5C, 0x1C, 0x71, 0x73, 0x1B, 0xD3, 0x13, 0xE7, 0x77, 0x4D, 0xD6, 0x9C, 0x1D, 0x1F, 0xEF,
	0xBB, 0x66, 0x99, 0xF6, 0x3F, 0x02, 0x7E, 0xCF, 0x2B, 0x35, 0x88, 0xBA, 0xA4, 0x40, 0x19, 0x23,
	0xC1, 0xD4, 0xD7, 0x43, 0x52, 0x34, 0xE9, 0xDC, 0x60, 0x24, 0x94, 0x6B, 0x81, 0x03, 0xC0, 0x39,
	0xBE, 0x90, 0x65, 0xFD, 0xE0, 0x2D, 0x7C, 0xEA, 0x04, 0xA6, 0xDB, 0xF3, 0xCE, 0xB4, 0xA9, 0xAA,
	0xAD, 0x64, 0xF2, 0x72, 0xD2, 0x84, 0x8E, 0xD1, 0x26, 0xA3, 0xCA, 0x4A, 0x48, 0x06, 0x0F, 0x36,
	0x85, 0xD0, 0x51, 0x6D, 0xC4, 0x3E, 0x92, 0xF1, 0xC7, 0x62, 0x79, 0xA1, 0x9B, 0x68, 0xF5, 0xE2,
	0xAB, 0x0C, 0xCC, 0x78, 0xFC, 0x2A, 0xD8, 0x3A, 0xDD, 0x8F, 0x10, 0x29, 0xF4, 0x0A, 0xB8, 0xC3
};

static uint16_t decode(uint8_t *p, int size, uint16_t key) {
	for (int i = 0; i < size; ++i) {
		uint8_t dl = (key >> 8) & 255;
		uint8_t dh = (key & 255);
		++dl;
		const uint8_t acc = _shuffleTable[dl];
		dh ^= acc;
		p[i] ^= acc;
		key = (dh << 8) | dl;
	}
	return key;
}

struct Bitstream {
	File *_f;
	int _size;
	uint16_t _bits;
	int _len;
	uint16_t _value;

	bool eos() const {
		return _size <= 0;
	}

	bool refill() {
		while (_len <= 8) {
			if (eos()) {
				return false;
			}
			const uint8_t b = _f->readByte();
			--_size;
			_value |= b << (8 - _len);
			_len += 8;
		}
		return true;
	}
	uint8_t readByte() {
		assert(_len >= 8);
		const uint8_t b = _value >> 8;
		_value <<= 8;
		_len -= 8;
		return b;
	}
	bool readBit() {
		assert(_len > 0);
		const bool carry = (_value & 0x8000) != 0;
		_value <<= 1;
		--_len;
		return carry;
	}
};

struct Buffer {
	uint16_t _buf[0x894];

	Buffer() {
		memset(_buf, 0, sizeof(_buf));
	}

	int fixAddr(int addr) {
		const uint32_t in = addr;
		assert((addr & 1) == 0);
		addr /= 2;
		assert(addr >= 0x4A30 && addr < 0x52C4);
		return addr - 0x4A30;
	}
	int getWord(uint16_t addr) {
		return _buf[fixAddr(addr)];
	}
	void setWord(uint16_t addr, int value) {
		_buf[fixAddr(addr)] = value;
	}
};

struct OotwmmDll {

	Bitstream _bs;
	Buffer _mem;
	uint8_t _window[0x1000];
	uint8_t _windowCodeOffsets[0x100];

	void buildDecodeTable1();
	void buildDecodeTable2();
	int readWindowOffset();
	void updateCode0x8000();
	void updateCodesTable(int num);
	bool decompressEntry(File &f, const Win31BankEntry *e, uint8_t *dst);
};

static const uint8_t _windowCodeLengths[0x10] = { 1, 1, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 5, 5, 5, 6 };

void OotwmmDll::buildDecodeTable1() {
	static const uint8_t byte_9E2[] = { 1, 3, 8, 0xC, 0x18, 0x10 };
	int count = 0;
	int val = 0;
	int mask = 0x20;
	uint8_t *p = _windowCodeOffsets;
	do {
		assert(count < 6);
		for (int i = 0; i < byte_9E2[count]; ++i) {
			memset(p, val, mask);
			p += mask;
			++val;
		}
		++count;
		mask >>= 1;
	} while (mask != 0);
}

void OotwmmDll::buildDecodeTable2() {
	for (int i = 0; i < 314; ++i) {
		_mem.setWord(i * 2 - 0x6BA0, 1);
		_mem.setWord(i * 2 - 0x61D2, i);
		_mem.setWord(i * 2 - 0x5F5E, 627 + i);
	}
	int _di = 0;
	for (int i = 628; i < 1254; i += 2) {
		_mem.setWord(i - 0x6BA0, _mem.getWord(_di - 0x6BA0) + _mem.getWord(_di - 0x6B9E));
		_mem.setWord(i - 0x5F5E, _di >> 1);
		_mem.setWord(_di - 0x66B8, i >> 1);
		_mem.setWord(_di - 0x66B6, i >> 1);
		_di += 4;
	}
	_mem.setWord(0x9946, 0xFFFF);
	_mem.setWord(0x9E2C, 0);
}

int OotwmmDll::readWindowOffset() {
	_bs.refill();
	int val = _bs.readByte();
	const int offset = _windowCodeOffsets[val] << 6;
	const int count = _windowCodeLengths[val >> 4];
	assert(count <= 6);
	for (int i = 0; i < count; ++i) {
		_bs.refill();
		val <<= 1;
		if (_bs.readBit()) {
			val |= 1;
		}
	}
	return offset | (val & 0x3F);
}

void OotwmmDll::updateCode0x8000() {
	int _si = 0xA0A2;
	int ndx = 0;
	for (int i = 0; i < 627; ++i) {
		int code = _mem.getWord(_si); _si += 2;
		if (code >= 627) {
			int code2 = _mem.getWord(_si - 0xC44);
			_mem.setWord(ndx - 0x6BA0, (code2 + 1) >> 1);
			_mem.setWord(ndx - 0x5F5E, code);
			ndx += 2;
		}
	}
	for (int i = 0; i < 313; ++i) {
		int _di = i * 4;
		int ndx = 628 + i * 2;
		int _si = ndx;
		int code = _mem.getWord(_di - 0x6B9E) + _mem.getWord(_di - 0x6BA0);
		_mem.setWord(ndx - 0x6BA0, code);
		do {
			ndx -= 2;
		} while (code < _mem.getWord(ndx - 0x6BA0));
		ndx += 2;
		_si -= ndx;
		if (_si > 0) {
			do {
				_mem.setWord(ndx + _si - 0x6BA0, _mem.getWord(ndx + _si - 0x6BA2));
				_mem.setWord(ndx + _si - 0x5F5E, _mem.getWord(ndx + _si - 0x5F60));
				_si -= 2;
			} while (_si != 0);
		}
		_mem.setWord(ndx - 0x6BA0, code);
		_mem.setWord(ndx - 0x5F5E, _di >> 1);
	}
	for (int i = 0; i < 627; ++i) {
		int _di = _mem.getWord(i * 2 - 0x5F5E);
		_di = 0x9948 + _di * 2;
		assert(_di >= 0x9948);
		_mem.setWord(_di, i);
		_di += 2;
		if (_di < 0x9E30) {
			_mem.setWord(_di, i);
		}
	}
}

void OotwmmDll::updateCodesTable(int num) {
	const int val = _mem.getWord(0x9944);
	if (val == 0x8000) {
		updateCode0x8000();
	}
	int ndx = _mem.getWord(num * 2 - 0x61D2);
	do {
		ndx *= 2;
		_mem.setWord(ndx - 0x6BA0, _mem.getWord(ndx - 0x6BA0) + 1);
		int code = _mem.getWord(ndx - 0x6BA0);
		int _si = ndx + 2;
		if (code > _mem.getWord(_si - 0x6BA0)) {
			do {
				_si += 2;
			} while (code > _mem.getWord(_si - 0x6BA0));
			_si -= 2;
			_mem.setWord(ndx - 0x6BA0, _mem.getWord(_si - 0x6BA0));
			_mem.setWord(_si - 0x6BA0, code);
			int _di = _mem.getWord(ndx - 0x5F5E) * 2;
			_mem.setWord(_di - 0x66B8, _si >> 1);
			if (_di < 1254) {
				_mem.setWord(_di - 0x66B6, _si >> 1);
			}
			const int code2 = _di >> 1;
			_di = _mem.getWord(_si - 0x5F5E) * 2;
			_mem.setWord(_si - 0x5F5E, code2);
			_mem.setWord(_di - 0x66B8, ndx >> 1);
			if (_di < 1254) {
				_mem.setWord(_di - 0x66B6, ndx >> 1);
			}
			_mem.setWord(ndx - 0x5F5E, _di >> 1);
			ndx = _si;
		}
		ndx = _mem.getWord(ndx - 0x66B8);
	} while (ndx != 0);
}

bool OotwmmDll::decompressEntry(File &f, const Win31BankEntry *e, uint8_t *out) {
	f.seek(e->offset);
	memset(&_bs, 0, sizeof(_bs));
	_bs._f = &f;
	_bs._size = e->packedSize;

	memset(_window, 0x20, sizeof(_window));
	int windowSize = 4078;
	int decompressedSize = e->size;

	buildDecodeTable1();
	buildDecodeTable2();

	int size = 0;
	while (size < decompressedSize) {
		int code = _mem.getWord(0xA586);
		while (code < 627) {
			_bs.refill();
			if (_bs.readBit()) {
				++code;
			}
			code <<= 1;
			code = _mem.getWord(code - 0x5F5E);
		}
		code -= 627;
		updateCodesTable(code);
		int code2 = code;
		if ((code2 >> 8) == 0) {
			const uint8_t val = code2 & 255;
			_window[windowSize] = val;
			windowSize = (windowSize + 1) & 0xFFF;
			out[size++] = val;
		} else {
			int count = code2 - 253;
			const int offset = readWindowOffset();
			code = (windowSize - offset - 1) & 0xFFF;
			do {
				const uint8_t val = _window[code];
				code = (code + 1) & 0xFFF;
				_window[windowSize] = val;
				windowSize = (windowSize + 1) & 0xFFF;
				out[size++] = val;
			} while (--count != 0 && size < decompressedSize);
                }
	}
	return size == decompressedSize;
}

const char *ResourceWin31::FILENAME = "BANK";

ResourceWin31::ResourceWin31(const char *dataPath)
	:  _dataPath(dataPath), _entries(0), _entriesCount(0) {
	_f.open(FILENAME, dataPath);
	_textBuf = 0;
	memset(_stringsTable, 0, sizeof(_stringsTable));
	_dll = new OotwmmDll;
}

ResourceWin31::~ResourceWin31() {
	free(_entries);
	free(_textBuf);
	delete _dll;
}

bool ResourceWin31::readEntries() {
	uint8_t buf[32];
	const int count = _f.read(buf, sizeof(buf));
	if (count == 32 && memcmp(buf, "NL\00\00", 4) == 0) {
		_entriesCount = READ_LE_UINT16(buf + 4);
		debug(DBG_RESOURCE, "Read %d entries in win31 '%s'", _entriesCount, FILENAME);
		_entries = (Win31BankEntry *)calloc(_entriesCount, sizeof(Win31BankEntry));
		if (_entries) {
			uint16_t key = READ_LE_UINT16(buf + 0x14);
			for (int i = 0; i < _entriesCount; ++i) {
				_f.read(buf, sizeof(buf));
				key = decode(buf, sizeof(buf), key);
				Win31BankEntry *e = &_entries[i];
				memcpy(e->name, buf, 16);
				const uint16_t flags = READ_LE_UINT16(buf + 16);
				e->type = buf[19];
				e->size = READ_LE_UINT32(buf + 20);
				e->offset = READ_LE_UINT32(buf + 24);
				e->packedSize = READ_LE_UINT32(buf + 28);
				debug(DBG_RESOURCE, "Res #%03d '%s' type %d size %d (%d) offset 0x%x", i, e->name, e->type, e->size, e->packedSize, e->offset);
				assert(e->size == 0 || flags == 0x80);
			}
			readStrings();
		}
	}
	return _entries != 0;
}

uint8_t *ResourceWin31::loadEntry(int num, uint8_t *dst, uint32_t *size) {
	if (num > 0 && num < _entriesCount) {
		Win31BankEntry *e = &_entries[num];
		*size = e->size;
		if (!dst) {
			dst = (uint8_t *)malloc(e->size);
			if (!dst) {
				warning("Unable to allocate %d bytes", e->size);
				return 0;
			}
		}
		// check for unpacked data
		char name[32];
		snprintf(name, sizeof(name), "%03d_%s", num, e->name);
		File f;
		if (f.open(name, _dataPath) && f.size() == e->size) {
			f.read(dst, e->size);
			return dst;
		}
		if (_dll->decompressEntry(_f, e, dst)) {
			return dst;
		}
	}
	warning("Unable to load resource #%d", num);
	return 0;
}

void ResourceWin31::readStrings() {
	int count = 0;
	uint32_t len, offset = 0;
	_textBuf = loadEntry(148, 0, &len);
	while (1) {
		const uint32_t sep = READ_LE_UINT32(_textBuf + offset); offset += 4;
		const uint16_t num = sep >> 16;
		if (num == 0xFFFF) {
			break;
		}
		if (num < ARRAYSIZE(_stringsTable)) {
			_stringsTable[num] = (const char *)_textBuf + offset;
		}
		while (offset < len && _textBuf[offset++] != 0);
		// strings are not always '\0' terminated
		if (_textBuf[offset + 1] != 0) {
			--offset;
		}
	}
}

const char *ResourceWin31::getString(int num) const {
	return _stringsTable[num];
}

const char *ResourceWin31::getMusicName(int num) const {
	switch (num) {
	case 7:
		return "y.mid";
	case 138:
		return "X.mid";
	}
	return 0;
}
