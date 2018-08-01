
/*
 * Another World engine rewrite
 * Copyright (C) 2004-2005 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include "resource.h"
#include "file.h"
#include "pak.h"
#include "resource_nth.h"
#include "resource_win31.h"
#include "resource_3do.h"
#include "resource_mac.h"
#include "unpack.h"
#include "util.h"
#include "video.h"


Resource::Resource(Video *vid, const char *dataDir) 
	: _vid(vid), _dataDir(dataDir), _currentPart(0), _nextPart(0), _dataType(DT_DOS), _nth(0), _win31(0), _3do(0), _mac(0) {
	_bankPrefix = "bank";
	memset(_memList, 0, sizeof(_memList));
	_numMemList = 0;
	if (!_dataDir) {
		_dataDir = ".";
	}
}

Resource::~Resource() {
	delete _nth;
	delete _win31;
	delete _3do;
	delete _mac;
}

bool Resource::readBank(const MemEntry *me, uint8_t *dstBuf) {
	bool ret = false;
	char name[10];
	snprintf(name, sizeof(name), "%s%02x", _bankPrefix, me->bankNum);
	File f;
	if (f.open(name, _dataDir)) {
		f.seek(me->bankPos);
		const size_t count = f.read(dstBuf, me->packedSize);
		ret = (count == me->packedSize);
		if (ret && me->packedSize != me->unpackedSize) {
			ret = delphine_unpack(dstBuf, me->unpackedSize, dstBuf, me->packedSize);
		}
	}
	return ret;
}

static bool check15th(File &f, const char *dataDir) {
	char path[MAXPATHLEN];
	snprintf(path, sizeof(path), "%s/Data", dataDir);
	return f.open(Pak::FILENAME, path);
}

static bool check20th(File &f, const char *dataDir) {
	char path[MAXPATHLEN];
	snprintf(path, sizeof(path), "%s/game/DAT", dataDir);
	return f.open("FILE017.DAT", path);
}

static bool check3DO(File &f, const char *dataDir) {
	const char *ext = strrchr(dataDir, '.');
	if (ext && strcasecmp(ext, ".iso") == 0) {
		return f.open(dataDir);
	}
	char path[MAXPATHLEN];
	snprintf(path, sizeof(path), "%s/GameData", dataDir);
	return f.open("File340", path);
}

void Resource::detectVersion() {
	File f;
	if (check15th(f, _dataDir)) {
		_dataType = DT_15TH_EDITION;
		debug(DBG_INFO, "Using 15th anniversary edition data files");
	} else if (check20th(f, _dataDir)) {
		_dataType = DT_20TH_EDITION;
		debug(DBG_INFO, "Using 20th anniversary edition data files");
	} else if (f.open("memlist.bin", _dataDir)) {
		_dataType = DT_DOS;
		debug(DBG_INFO, "Using DOS data files");
	}  else if (f.open("bank01", _dataDir)) {
		_dataType = DT_AMIGA;
		debug(DBG_INFO, "Using Amiga data files");
	} else if (f.open(ResourceWin31::FILENAME, _dataDir)) {
		_dataType = DT_WIN31;
		debug(DBG_INFO, "Using Win31 data files");
	} else if (check3DO(f, _dataDir)) {
		_dataType = DT_3DO;
		debug(DBG_INFO, "Using 3DO data files");
	} else if (f.open(ResourceMac::FILE017, _dataDir)) {
		_dataType = DT_MAC;
		debug(DBG_INFO, "Using Macintosh data files");
	} else {
		error("No data files found in '%s'", _dataDir);
	}
}

static const char *kGameTitleEU = "Another World";
static const char *kGameTitleUS = "Out Of This World";
static const char *kGameTitle15thEdition = "Another World 15th anniversary edition";
static const char *kGameTitle20thEdition = "Another World 20th anniversary edition";

const char *Resource::getGameTitle(Language lang) const {
	switch (_dataType) {
	case DT_15TH_EDITION:
		return kGameTitle15thEdition;
	case DT_20TH_EDITION:
		return kGameTitle20thEdition;
	case DT_3DO:
		return kGameTitleUS;
	case DT_DOS:
		if (lang == LANG_US) {
			return kGameTitleUS;
		}
		/* fall-through */
	default:
		break;
	}
	return kGameTitleEU;
}

void Resource::readEntries() {
	if (_dataType == DT_15TH_EDITION) {
		_numMemList = ENTRIES_COUNT;
		_nth = ResourceNth::create(15, _dataDir);
		if (_nth && _nth->init()) {
			return;
		}
	} else if (_dataType == DT_20TH_EDITION) {
		_numMemList = ENTRIES_COUNT;
		_nth = ResourceNth::create(20, _dataDir);
		if (_nth && _nth->init()) {
			return;
		}
	} else if (_dataType == DT_AMIGA) {
		static const uint32_t bank01Sizes[] = { 244674, 244868, 0 };
		static const AmigaMemEntry *entries[] = { _memListAmigaFR, _memListAmigaEN, 0 };
		File f;
		if (f.open("bank01", _dataDir)) {
			for (int i = 0; bank01Sizes[i] != 0; ++i) {
				if (f.size() == bank01Sizes[i]) {
					readEntriesAmiga(entries[i], 146);
					return;
				}
			}
		}
	} else if (_dataType == DT_DOS) {
		File f;
		if (f.open("demo01", _dataDir)) {
			_bankPrefix = "demo";
		}
		if (f.open("memlist.bin", _dataDir)) {
			MemEntry *me = _memList;
			while (1) {
				assert(_numMemList < ARRAYSIZE(_memList));
				me->status = f.readByte();
				me->type = f.readByte();
				me->bufPtr = 0; f.readUint32BE();
				me->rankNum = f.readByte();
				me->bankNum = f.readByte();
				me->bankPos = f.readUint32BE();
				me->packedSize = f.readUint32BE();
				me->unpackedSize = f.readUint32BE();
				if (me->status == 0xFF) {
					return;
				}
				++_numMemList;
				++me;
			}
		}
	} else if (_dataType == DT_WIN31) {
		_numMemList = ENTRIES_COUNT;
		_win31 = new ResourceWin31(_dataDir);
		if (_win31->readEntries()) {
			return;
		}
	} else if (_dataType == DT_3DO) {
		_numMemList = ENTRIES_COUNT;
		_3do = new Resource3do(_dataDir);
		if (_3do->readEntries()) {
			return;
		}
	} else if (_dataType == DT_MAC) {
		_numMemList = ENTRIES_COUNT;
		_mac = new ResourceMac(_dataDir);
		return;
	}
	error("No data files found in '%s'", _dataDir);
}

void Resource::readEntriesAmiga(const AmigaMemEntry *entries, int count) {
	_numMemList = count;
	for (int i = 0; i < count; ++i) {
		_memList[i].type = entries[i].type;
		_memList[i].bankNum = entries[i].bank;
		_memList[i].bankPos = entries[i].offset;
		_memList[i].packedSize = entries[i].packedSize;
		_memList[i].unpackedSize = entries[i].unpackedSize;
	}
	_memList[count].status = 0xFF;
}

void Resource::dumpEntries() {
	static const bool kDump = false;
	if (kDump && (_dataType == DT_DOS || _dataType == DT_AMIGA)) {
		for (int i = 0; i < _numMemList; ++i) {
			if (_memList[i].unpackedSize == 0) {
				continue;
			}
			if (_memList[i].bankNum == 5 && _dataType == DT_AMIGA) {
				continue;
			}
			uint8_t *p = (uint8_t *)malloc(_memList[i].unpackedSize);
			if (p) {
				if (readBank(&_memList[i], p)) {
					char name[16];
					snprintf(name, sizeof(name), "data_%02x_%d", i, _memList[i].type);
					dumpFile(name, p, _memList[i].unpackedSize);
				}
				free(p);
			}
		}
	}
}

void Resource::load() {
	while (1) {
		MemEntry *me = 0;

		// get resource with max rankNum
		uint8_t maxNum = 0;
		for (int i = 0; i < _numMemList; ++i) {
			MemEntry *it = &_memList[i];
			if (it->status == STATUS_TOLOAD && maxNum <= it->rankNum) {
				maxNum = it->rankNum;
				me = it;
			}
		}
		if (me == 0) {
			break; // no entry found
		}

		const int resourceNum = me - _memList;

		uint8_t *memPtr = 0;
		if (me->type == 2) {
			memPtr = _vidCurPtr;
		} else {
			memPtr = _scriptCurPtr;
			if (me->unpackedSize > uint32_t(_vidBakPtr - _scriptCurPtr)) {
				warning("Resource::load() not enough memory");
				me->status = STATUS_NULL;
				continue;
			}
		}
		if (me->bankNum == 0) {
			warning("Resource::load() ec=0x%X (me->bankNum == 0)", 0xF00);
			me->status = STATUS_NULL;
		} else {
			debug(DBG_BANK, "Resource::load() bufPos=0x%X size=%d type=%d pos=0x%X bankNum=%d", memPtr - _memPtrStart, me->packedSize, me->type, me->bankPos, me->bankNum);
			if (readBank(me, memPtr)) {
				if (me->type == 2) {
					_vid->copyBitmapPtr(_vidCurPtr, me->unpackedSize);
					me->status = STATUS_NULL;
				} else {
					me->bufPtr = memPtr;
					me->status = STATUS_LOADED;
					_scriptCurPtr += me->unpackedSize;
				}
			} else {
				if (_dataType == DT_DOS && me->bankNum == 12 && me->type == RT_UNK) {
					// DOS demo version does not have the bank for this resource
					// this should be safe to ignore as the resource does not appear to be used by the game code
					me->status = STATUS_NULL;
					continue;
				}
				error("Unable to read resource %d from bank %d", resourceNum, me->bankNum);
			}
		}
	}
}

void Resource::invalidateRes() {
	for (int i = 0; i < _numMemList; ++i) {
		MemEntry *me = &_memList[i];
		if (me->type <= 2 || me->type > 6) {
			me->status = STATUS_NULL;
		}
	}
	_scriptCurPtr = _scriptBakPtr;
	_vid->_currentPal = 0xFF;
}

void Resource::invalidateAll() {
	for (int i = 0; i < _numMemList; ++i) {
		_memList[i].status = STATUS_NULL;
	}
	_scriptCurPtr = _memPtrStart;
}

static const uint8_t *getSoundsList3DO(int num) {
	static const uint8_t intro7[] = {
		0x33, 0xFF
	};
	static const uint8_t water7[] = {
		0x08, 0x10, 0x2D, 0x30, 0x31, 0x32, 0x35, 0x39, 0x3A, 0x3C,
		0x3D, 0x3E, 0x4A, 0x4B, 0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x52,
		0x54, 0xFF
	};
	static const uint8_t pri1[] = {
		0x52, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D,
		0x5E, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
		0x69, 0x70, 0x71, 0x72, 0x73, 0xFF
	};
	static const uint8_t cite1[] = {
		0x02, 0x03, 0x52, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B,
		0x5C, 0x5D, 0x5E, 0x5F, 0x60, 0x63, 0x66, 0x6A, 0x6B, 0x6C,
		0x6D, 0x6E, 0x6F, 0x70, 0x72, 0x74, 0x75, 0x77, 0x78, 0x79,
		0x7A, 0x7B, 0x7C, 0x88, 0xFF
	};
	static const uint8_t arene2[] = {
		0x52, 0x57, 0x58, 0x59, 0x5B, 0x84, 0x8B, 0x8C, 0x8E, 0xFF
	};
	static const uint8_t luxe2[] = {
		0x30, 0x52, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C,
		0x5D, 0x5E, 0x5F, 0x60, 0x66, 0x67, 0x6B, 0x6C, 0x70, 0x74,
		0x75, 0x79, 0x7A, 0x8D, 0xFF
	};
	static const uint8_t final3[] = {
		0x08, 0x0E, 0x0F, 0x57, 0xFF
	};
	switch (num) {
	case 2001: return intro7;
	case 2002: return water7;
	case 2003: return pri1;
	case 2004: return cite1;
	case 2005: return arene2;
	case 2006: return luxe2;
	case 2007: return final3;
	}
	return 0;
}

static const int _memListAudio[] = {
	8, 16, 97, 102, -1
};

static const int _memListBmp[] = {
	145, 144, 73, 72, 70, 69, 68, 67, -1
};

void Resource::update(uint16_t num) {
	if (_dataType == DT_15TH_EDITION || _dataType == DT_20TH_EDITION || _dataType == DT_WIN31 || _dataType == DT_MAC) {
		if (num > 16000) {
			_nextPart = num;
		} else if (num >= 3000) {
			loadBmp(num);
		} else {
			for (int i = 0; _memListBmp[i] != -1; ++i) {
				if (num == _memListBmp[i]) {
					loadBmp(num);
					break;
				}
			}
		}
		return;
	}
	if (_dataType == DT_3DO) {
		if (num > 16000) {
			_nextPart = num;
		} else if (num >= 2000) { // preload sounds
			const uint8_t *soundsList = getSoundsList3DO(num);
			for (int i = 0; soundsList[i] != 255; ++i) {
				loadDat(soundsList[i]);
			}
		} else if (num >= 200) {
			loadBmp(num);
		}
		return;
	}
	if (num > _numMemList) {
		_nextPart = num;
	} else {
		if (0) { // from DOS disassembly
			for (int i = 0; _memListAudio[i] != -1; ++i) {
				if (num == _memListAudio[i]) {
					return;
				}
			}
		}
		MemEntry *me = &_memList[num];
		if (me->status == STATUS_NULL) {
			me->status = STATUS_TOLOAD;
			load();
		}
	}
}

void Resource::loadBmp(int num) {
	uint32_t size = 0;
	uint8_t *p = 0;
	if (_nth) {
		p = _nth->loadBmp(num);
	} else if (_win31) {
		p = _win31->loadFile(num, 0, &size);
	} else if (_3do) {
		p = _3do->loadFile(num, 0, &size);
	} else if (_mac) {
		p = _mac->loadFile(num, 0, &size);
	}
	if (p) {
		_vid->copyBitmapPtr(p, size);
		free(p);
	}
}

uint8_t *Resource::loadDat(int num) {
	assert(num < _numMemList);
	if (_memList[num].status == STATUS_LOADED) {
		return _memList[num].bufPtr;
	}
	uint32_t size = 0;
	uint8_t *p = 0;
	if (_nth) {
		p = _nth->loadDat(num, _scriptCurPtr, &size);
	} else if (_win31) {
		p = _win31->loadFile(num, _scriptCurPtr, &size);
	} else if (_3do) {
		p = _3do->loadFile(num, _scriptCurPtr, &size);
	} else if (_mac) {
		p = _mac->loadFile(num, _scriptCurPtr, &size);
	}
	if (p) {
		_scriptCurPtr += size;
		_memList[num].bufPtr = p;
		_memList[num].status = STATUS_LOADED;
	}
	return p;
}

void Resource::loadFont() {
	if (_nth) {
		uint8_t *p = _nth->load("font.bmp");
		if (p) {
			_vid->setFont(p);
			free(p);
		}
	}
}

void Resource::loadHeads() {
	if (_nth) {
		uint8_t *p = _nth->load("heads.bmp");
		if (p) {
			_vid->setHeads(p);
			free(p);
		}
	}
}

uint8_t *Resource::loadWav(int num) {
	if (_memList[num].status == STATUS_LOADED) {
		return _memList[num].bufPtr;
	}
	uint32_t size = 0;
	uint8_t *p = 0;
	if (_nth) {
		p = _nth->loadWav(num, _scriptCurPtr, &size);
	} else if (_win31) {
		p = _win31->loadFile(num, _scriptCurPtr, &size);
	}
	if (p && size != 0) {
		_scriptCurPtr += size;
		_memList[num].bufPtr = p;
		_memList[num].status = STATUS_LOADED;
	}
	return p;
}

const char *Resource::getString(int num) {
	if (_nth) {
		return _nth->getString(LANG_US, num);
	} else if (_win31) {
		return _win31->getString(num);
	}
	return 0;
}

const char *Resource::getMusicPath(int num, char *buf, int bufSize, uint32_t *offset) {
	const char *name = 0;
	if (_nth) {
		name = _nth->getMusicName(num);
	} else if (_win31) {
		name = _win31->getMusicName(num);
	} else if (_3do) {
		assert(offset);
		name = _3do->getMusicName(num, offset);
		if (*offset != 0) {
			return _dataDir; // playing music from .ISO
		}
	}
	if (name) {
		snprintf(buf, bufSize, "%s/%s", _dataDir, name);
		return buf;
	}
	return 0;
}

static void preloadDat(int part, int type, int num) {
	static const char *names[] = {
		"intro", "eau", "pri", "cite", "arene", "luxe", "final", 0
	};
	static const char *exts[] = {
		"pal", "mac", "mat", 0
	};
	if (part > 0 && part < 8) {
		char name[16];
		if (type == 3) {
			assert(num == 0x11);
			strcpy(name, "bank2.mat");
		} else {
			snprintf(name, sizeof(name), "%s2011.%s", names[part - 1], exts[type]);
		}
		debug(DBG_RESOURCE, "Loading '%s'", name);
	}
}

static const uint8_t _memListParts[][4] = {
	{ 0x14, 0x15, 0x16, 0x00 }, // protection screens
	{ 0x17, 0x18, 0x19, 0x00 }, // introduction
	{ 0x1A, 0x1B, 0x1C, 0x11 },
	{ 0x1D, 0x1E, 0x1F, 0x11 },
	{ 0x20, 0x21, 0x22, 0x11 },
	{ 0x23, 0x24, 0x25, 0x00 },
	{ 0x26, 0x27, 0x28, 0x11 },
	{ 0x29, 0x2A, 0x2B, 0x11 },
	{ 0x7D, 0x7E, 0x7F, 0x00 },
	{ 0x7D, 0x7E, 0x7F, 0x00 }  // password screen
};

void Resource::setupPart(int ptrId) {
	if (_dataType == DT_15TH_EDITION || _dataType == DT_20TH_EDITION || _dataType == DT_WIN31 || _dataType == DT_3DO || _dataType == DT_MAC) {
		if (ptrId >= 16001 && ptrId <= 16009) {
			invalidateAll();
			uint8_t **segments[4] = { &_segVideoPal, &_segCode, &_segVideo1, &_segVideo2 };
			for (int i = 0; i < 4; ++i) {
				const int num = _memListParts[ptrId - 16000][i];
				if (num != 0) {
					preloadDat(ptrId - 16000, i, num);
					*segments[i] = loadDat(num);
				}
			}
			_currentPart = ptrId;
		}
		_scriptBakPtr = _scriptCurPtr;
		return;
	}
	if (ptrId != _currentPart) {
		uint8_t ipal = 0;
		uint8_t icod = 0;
		uint8_t ivd1 = 0;
		uint8_t ivd2 = 0;
		if (ptrId >= 16000 && ptrId <= 16009) {
			uint16_t part = ptrId - 16000;
			ipal = _memListParts[part][0];
			icod = _memListParts[part][1];
			ivd1 = _memListParts[part][2];
			ivd2 = _memListParts[part][3];
		} else {
			error("Resource::setupPart() ec=0x%X invalid part", 0xF07);
		}
		invalidateAll();
		_memList[ipal].status = STATUS_TOLOAD;
		_memList[icod].status = STATUS_TOLOAD;
		_memList[ivd1].status = STATUS_TOLOAD;
		if (ivd2 != 0) {
			_memList[ivd2].status = STATUS_TOLOAD;
		}
		load();
		_segVideoPal = _memList[ipal].bufPtr;
		_segCode = _memList[icod].bufPtr;
		_segVideo1 = _memList[ivd1].bufPtr;
		if (ivd2 != 0) {
			_segVideo2 = _memList[ivd2].bufPtr;
		}
		_currentPart = ptrId;
	}
	_scriptBakPtr = _scriptCurPtr;
}

void Resource::allocMemBlock() {
	_memPtrStart = (uint8_t *)malloc(MEM_BLOCK_SIZE);
	_scriptBakPtr = _scriptCurPtr = _memPtrStart;
	_vidBakPtr = _vidCurPtr = _memPtrStart + MEM_BLOCK_SIZE - 0x800 * 16;
	_useSegVideo2 = false;
}

void Resource::freeMemBlock() {
	free(_memPtrStart);
	_memPtrStart = 0;
}
