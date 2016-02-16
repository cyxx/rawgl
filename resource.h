/* 
 * Another World Interpreter 
 * (c) 2004-2005 Gregory Montoir
 */

#ifndef __RESOURCE_H__
#define __RESOURCE_H__

#include "intern.h"
#include "file.h"

struct ResDataEntry {
	bool valid;
	uint8 state;
	uint8 type;
	uint32 size;
	uint32 offset;
	uint8 *bufPtr;
};

struct Serializer;
struct Graphics;

struct Resource {
	enum ResType {
		RT_SOUND   = 0,
		RT_MUSIC   = 1,
		RT_BITMAP  = 2,
		RT_PAL     = 3,
		RT_SCRIPT  = 4,
		RT_GFX     = 5
	};
	enum ResState {
		RS_FREE    = 0,
		RS_LOADED  = 1,
		RS_TO_LOAD = 2
	};
	
	static const char *_dataFileName;
	static const char *_indexFileName;
	static const uint16 _resDataSeqList[][4];
	
	Graphics *_gfx;
	File _dataFile;
	const char *_dataDir;
	ResDataEntry _resDataList[150];
	uint16 _resDataCount;
	uint16 _curSeqId, _newSeqId;
	uint8 *_dataPal;
	uint8 *_dataScript;
	uint8 *_dataGfx1;
	uint8 *_dataGfx2;
	
	Resource(Graphics *gfx, const char *dataDir);
	
	void prepare();
	void loadMarked();
	void invalidateAll();
	void invalidateSnd();
	void loadData(uint16 num);
	void loadDataSeq(uint16 seqId);
	void saveOrLoad(Serializer &ser);
};

#endif
