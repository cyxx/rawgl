
/*
 * Another World engine rewrite
 * Copyright (C) 2004-2005 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include "resource.h"
#include "file.h"
#include "unpack.h"
#include "video.h"


Resource::Resource(Video *vid, const char *dataDir) 
	: _vid(vid), _dataDir(dataDir), _curPtrsId(0), _newPtrsId(0), _is15th(false), _pak(dataDir), _isAmiga(false) {
}

void Resource::readBank(const MemEntry *me, uint8_t *dstBuf) {
	uint16_t n = me - _memList;
	debug(DBG_BANK, "Resource::readBank(%d)", n);
#ifdef USE_UNPACKED_DATA
	char bankEntryName[64];
	sprintf(bankEntryName, "ootw-%02X-%d.dump", n, me->type);
	File f;
	if (!f.open(bankEntryName, _dataDir)) {
		error("Resource::readBank() unable to open '%s' file\n", bankEntryName);
	}
	f.read(dstBuf, me->unpackedSize);
#else
	char name[10];
	snprintf(name, sizeof(name), "bank%02x", me->bankNum);
	File f;
	if (f.open(name, _dataDir)) {
		f.seek(me->bankPos);
		if (me->packedSize == me->unpackedSize) {
			f.read(dstBuf, me->packedSize);
		} else {
			f.read(dstBuf, me->packedSize);
			bool ret = delphine_unpack(dstBuf, dstBuf, me->packedSize);
			if (!ret) {
				error("Resource::readBank() failed to unpack resource %d", n);
			}
		}
	} else {
		error("Resource::readBank() unable to open '%s'", name);
	}
#endif
}

void Resource::readEntries() {
	_pak.readEntries();
	if (_pak._entriesCount != 0) {
		_is15th = true;
		debug(DBG_INFO, "Using 15th anniversary edition data");
		return;
	}
	// DOS datafiles
	File f;
	if (!f.open("memlist.bin", _dataDir)) {
		warning("Resource::readEntries() unable to open 'memlist.bin' file");
		static const uint32_t bank01Sizes[] = { 244674, 244868, 0 };
		static const AmigaMemEntry *entries[] = { _memListAmigaFR, _memListAmigaEN, 0 };
		if (f.open("bank01", _dataDir)) {
			for (int i = 0; bank01Sizes[i] != 0; ++i) {
				if (f.size() == bank01Sizes[i]) {
					debug(DBG_INFO, "Using Amiga data files");
					_isAmiga = true;
					readEntriesAmiga(entries[i], 145);
					return;
				}
			}
		}
		error("No data files found in '%s'", _dataDir);
		return;
	}
	debug(DBG_INFO, "Using DOS data files");
	_numMemList = 0;
	MemEntry *me = _memList;
	while (1) {
		assert(_numMemList < ARRAYSIZE(_memList));
		me->valid = f.readByte();
		me->type = f.readByte();
		me->bufPtr = 0; f.readUint32BE();
		me->rankNum = f.readByte();
		me->bankNum = f.readByte();
		me->bankPos = f.readUint32BE();
		me->packedSize = f.readUint32BE();
		me->unpackedSize = f.readUint32BE();
		if (me->valid == 0xFF) {
			break;
		}
		++_numMemList;
		++me;
	}
}

void Resource::readEntriesAmiga(const AmigaMemEntry *entries, int count) {
	_numMemList = count;
	memset(_memList, 0, sizeof(_memList));
	for (int i = 0; i < count; ++i) {
		_memList[i].type = entries[i].type;
		_memList[i].bankNum = entries[i].bank;
		_memList[i].bankPos = entries[i].offset;
		_memList[i].packedSize = entries[i].packedSize;
		_memList[i].unpackedSize = entries[i].unpackedSize;
	}
	_memList[count].valid = 0xFF;
}

void Resource::load() {
	while (1) {
		MemEntry *it = _memList;
		MemEntry *me = 0;

		// get resource with max rankNum
		uint8_t maxNum = 0;
		uint16_t i = _numMemList;
		while (i--) {
			if (it->valid == 2 && maxNum <= it->rankNum) {
				maxNum = it->rankNum;
				me = it;
			}
			++it;
		}
		if (me == 0) {
			break; // no entry found
		}

		uint8_t *memPtr = 0;
		if (me->type == 2) {
			memPtr = _vidCurPtr;
		} else {
			memPtr = _scriptCurPtr;
			if (me->unpackedSize > uint32_t(_vidBakPtr - _scriptCurPtr)) {
				warning("Resource::load() not enough memory");
				me->valid = 0;
				continue;
			}
		}
		if (me->bankNum == 0) {
			warning("Resource::load() ec=0x%X (me->bankNum == 0)", 0xF00);
			me->valid = 0;
		} else {
			debug(DBG_BANK, "Resource::load() bufPos=%X size=%X type=%X pos=%X bankNum=%X", memPtr - _memPtrStart, me->packedSize, me->type, me->bankPos, me->bankNum);
			readBank(me, memPtr);
			if(me->type == 2) {
				_vid->copyPagePtr(_vidCurPtr);
				me->valid = 0;
			} else {
				me->bufPtr = memPtr;
				me->valid = 1;
				_scriptCurPtr += me->unpackedSize;
			}
		}
	}
}

void Resource::invalidateRes() {
	MemEntry *me = _memList;
	uint16_t i = _numMemList;
	while (i--) {
		if (me->type <= 2 || me->type > 6) {
			me->valid = 0;
		}
		++me;
	}
	_scriptCurPtr = _scriptBakPtr;
}

void Resource::invalidateAll() {
	MemEntry *me = _memList;
	uint16_t i = _numMemList;
	while (i--) {
		me->valid = 0;
		++me;
	}
	_scriptCurPtr = _memPtrStart;
}

const uint16_t Resource::_memListAudio[] = { 
	8, 0x10, 0x61, 0x66, 0xFFFF
};

void Resource::update(uint16_t num) {
	if (_is15th) {
		if (num > 16000) {
			_newPtrsId = num;
		} else if (num >= 3000) {
			loadBmp(num);
		} else { // 145, 144, 73, 72, 70, 69, 68, 67
			loadBmp(num);
		}
		return;
	}
	if (num > _numMemList) {
		_newPtrsId = num;
	} else {
		for (const uint16_t *ml = _memListAudio; *ml != 0xFFFF; ++ml) {
			if (*ml == num)
				return;
		}		
		MemEntry *me = &_memList[num];
		if (me->valid == 0) {
			me->valid = 2;
			load();
		}
	}
}

static const int resBmpList[] = { 18, 19, 67, 68, 69, 70, 71, 72, 73, 83, 144, 145, 0xFFFF };

void Resource::loadBmp(int num) {
	char name[16];
	if (num >= 3000) {
		snprintf(name, sizeof(name), "e%04d.bmp", num);
	} else {
		snprintf(name, sizeof(name), "file%03d.bmp", num);
	}
	const PakEntry *e = _pak.find(name);
	if (e) {
		uint8_t *tmp = (uint8_t *)malloc(e->size);
		if (tmp) {
			uint32_t size;
			_pak.loadData(e, tmp, &size);
			_vid->copyBitmapPtr(tmp);
			free(tmp);
		}
	}
}

uint8_t *Resource::loadDat(int num) {
	uint8_t *p = 0;
	for (int i = 0; resBmpList[i] != 0xFFFF; ++i) {
		if (resBmpList[i] == num) {
			loadBmp(num);
			return 0;
		}
	}
	char name[16];
	snprintf(name, sizeof(name), "file%03d.dat", num);
	const PakEntry *e = _pak.find(name);
	if (e) {
		uint32_t size;
		_pak.loadData(e, _scriptCurPtr, &size);
		p = _scriptCurPtr;
		_scriptCurPtr += size;
	}
	return p;
}

void Resource::setupPtrs(uint16_t ptrId) {
	if (_is15th) {
		if (ptrId >= 16001 && ptrId <= 16009) {
			_scriptCurPtr = _memPtrStart;
			static const int order[] = { 0, 1, 2, 3 };
			uint8_t **segments[4] = { &_segVideoPal, &_segCode, &_segVideo1, &_segVideo2 };
			for (int i = 0; i < 4; ++i) {
				const int num = _memListParts[ptrId - 16000][order[i]];
				if (num != 0) {
					*segments[order[i]] = loadDat(num);
				}
			}
			_curPtrsId = ptrId;
		}
		_scriptBakPtr = _scriptCurPtr;
		return;
	}
	if (ptrId != _curPtrsId) {
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
			error("Resource::setupPtrs() ec=0x%X invalid ptrId", 0xF07);
		}
		invalidateAll();
		_memList[ipal].valid = 2;
		_memList[icod].valid = 2;
		_memList[ivd1].valid = 2;
		if (ivd2 != 0) {
			_memList[ivd2].valid = 2;
		}
		load();
		_segVideoPal = _memList[ipal].bufPtr;
		_segCode = _memList[icod].bufPtr;
		_segVideo1 = _memList[ivd1].bufPtr;
		if (ivd2 != 0) {
			_segVideo2 = _memList[ivd2].bufPtr;
		}
		_curPtrsId = ptrId;
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
}
