/* 
 * Another World Interpreter 
 * (c) 2004-2005 Gregory Montoir
 */

#include "graphics.h"
#include "file.h"
#include "serializer.h"
#include "resource.h"


Resource::Resource(Graphics *gfx, const char *dataDir) 
	: _gfx(gfx), _dataDir(dataDir), _curSeqId(0), _newSeqId(0) {
}

void Resource::prepare() {	
	File indexFile;
	if (!indexFile.open(_indexFileName, _dataDir)) {
		error("Unable to open the '%s' file", _indexFileName);
	}
	indexFile.readUint16BE(); // version
	uint16 count = indexFile.readUint16BE();
	
	if (!_dataFile.open(_dataFileName, _dataDir)) {
		error("Unable to open the '%s' file", _dataFileName);
	}
	_dataFile.readUint16BE(); // version
	if (_dataFile.readUint16BE() != count) {
		error("Files '%s' and '%s' are mismatching", _indexFileName, _dataFileName);
	}

	memset(_resDataList, 0, sizeof(_resDataList));
	_resDataCount = 0;
	uint32 offset = 4;
	while (count--) {
		uint8 index = indexFile.readByte();
		assert(index < ARRAYSIZE(_resDataList));
		ResDataEntry *rde = &_resDataList[index];
		rde->valid = true;
		rde->state = RS_FREE;
		rde->type = indexFile.readByte();
		rde->size = indexFile.readUint32BE();
		rde->offset = offset;
		offset += rde->size;
		_resDataCount = MAX(index, _resDataCount);
	}
	++_resDataCount;
	if (indexFile.ioErr()) {
		warning("I/O error when reading '%s'", _indexFileName);
	}
}

void Resource::loadMarked() {
	for (int i = 0; i < _resDataCount; ++i) {
		ResDataEntry *rde = &_resDataList[i];
		if (rde->state == RS_TO_LOAD) {
			assert(rde->valid);
			_dataFile.seek(rde->offset);
			uint8 *p = (uint8 *)malloc(rde->size);
			if (!p) {
				error("Not enough memory to load data");
			}
			assert(p);
			_dataFile.read(p, rde->size);
			if (_dataFile.ioErr()) {
				warning("I/O error when reading '%s'", _dataFileName);
			}
			if (rde->type == RT_BITMAP) {
				_gfx->copyBitmapToPage(p);
				rde->state = RS_FREE;
				free(p);
			} else {
				rde->state = RS_LOADED;
				rde->bufPtr = p;
			}
		}
	}
}

void Resource::invalidateSnd() {
	for (int i = 0; i < _resDataCount; ++i) {
		ResDataEntry *rde = &_resDataList[i];
		if (rde->type <= 2 || rde->type > 6) {
			rde->state = RS_FREE;
			free(rde->bufPtr);
			rde->bufPtr = 0;
		}
	}
}

void Resource::invalidateAll() {
	for (int i = 0; i < _resDataCount; ++i) {
		ResDataEntry *rde = &_resDataList[i];
		rde->state = RS_FREE;
		free(rde->bufPtr);
		rde->bufPtr = 0;
	}
}

void Resource::loadData(uint16 num) {
	if (num < _resDataCount) {
		ResDataEntry *rde = &_resDataList[num];
		if (rde->state == RS_FREE) {
			rde->state = RS_TO_LOAD;
			loadMarked();
		}
	} else {
		_newSeqId = num;
	}
}

void Resource::loadDataSeq(uint16 seqId) {
	assert(seqId >= 16000 && seqId <= 16009);
	if (seqId != _curSeqId) {
		invalidateAll();
		uint16 seq = seqId - 16000;
		uint8 iData = _resDataSeqList[seq][0];
		uint8 iScript = _resDataSeqList[seq][1];
		uint8 iGfx1 = _resDataSeqList[seq][2];
		uint8 iGfx2 = _resDataSeqList[seq][3];
		_resDataList[iData].state = RS_TO_LOAD;
		_resDataList[iScript].state = RS_TO_LOAD;
		_resDataList[iGfx1].state = RS_TO_LOAD;
		if (iGfx2 != 0) {
			_resDataList[iGfx2].state = RS_TO_LOAD;
		}
		loadMarked();
		_dataPal = _resDataList[iData].bufPtr;
		_dataScript = _resDataList[iScript].bufPtr;
		_dataGfx1 = _resDataList[iGfx1].bufPtr;
		if (iGfx2 != 0) {
			_dataGfx2 = _resDataList[iGfx2].bufPtr;
		} else {
			_dataGfx2 = 0;
		}
		_curSeqId = seqId;
	}
}

void Resource::saveOrLoad(Serializer &ser) {
	uint8 currentDataList[4];
	uint8 loadedDataList[64];
	uint8 loadedDataCount = 0;
	if (ser._mode == Serializer::SM_SAVE) {
		memset(currentDataList, 0, sizeof(currentDataList));
		memset(loadedDataList, 0, sizeof(loadedDataList));
		for (int i = 0; i < _resDataCount; ++i) {
			ResDataEntry *rde = &_resDataList[i];
			if (rde->state == RS_LOADED) {
				assert(loadedDataCount < 64);
				loadedDataList[loadedDataCount] = i;
				++loadedDataCount;
				if (rde->bufPtr == _dataPal) {
					currentDataList[0] = i;
				} else if (rde->bufPtr == _dataScript) {
					currentDataList[1] = i;
				} else if (rde->bufPtr == _dataGfx1) {
					currentDataList[2] = i;
				} else if (rde->bufPtr == _dataGfx2) {
					currentDataList[3] = i;
				}
			}
		}
	}
	Serializer::Entry entries[] = {
		SE_INT(&loadedDataCount, Serializer::SES_INT8, VER(1)),
		SE_ARRAY(currentDataList, 4, Serializer::SES_INT8, VER(1)),
		SE_ARRAY(loadedDataList, 64, Serializer::SES_INT8, VER(1)),
		SE_INT(&_curSeqId, Serializer::SES_INT16, VER(1)),
		SE_INT(&_newSeqId, Serializer::SES_INT16, VER(1)),
		SE_END()
	};
	ser.saveOrLoadEntries(entries);
	if (ser._mode == Serializer::SM_LOAD) {
		invalidateAll();
		for (int i = 0; i < loadedDataCount; ++i) {
			uint8 index = loadedDataList[i];
			_resDataList[index].state = RS_TO_LOAD;
		}
		loadMarked();
		uint8 **dataPtrs[] = { &_dataPal, &_dataScript, &_dataGfx1, &_dataGfx2 };
		for (int i = 0; i < 4; ++i) {
			uint8 index = currentDataList[i];
			if (index != 0) {
				*(dataPtrs[i]) = _resDataList[index].bufPtr;
			} else {
				*(dataPtrs[i]) = 0;
			}
		}
	}
}
