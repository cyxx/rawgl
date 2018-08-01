
/*
 * Another World engine rewrite
 * Copyright (C) 2004-2005 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifndef RESOURCE_H__
#define RESOURCE_H__

#include "intern.h"

struct MemEntry {
	uint8_t status;        // 0x0
	uint8_t type;          // 0x1, Resource::ResType
	uint8_t *bufPtr;       // 0x2
	uint8_t rankNum;       // 0x6
	uint8_t bankNum;       // 0x7
	uint32_t bankPos;      // 0x8
	uint32_t packedSize;   // 0xC
	uint32_t unpackedSize; // 0x12
};

struct AmigaMemEntry {
	uint8_t type;
	uint8_t bank;
	uint32_t offset;
	uint32_t packedSize;
	uint32_t unpackedSize;
};

struct ResourceNth;
struct ResourceWin31;
struct Resource3do;
struct ResourceMac;
struct Video;

struct Resource {
	enum ResType {
		RT_SOUND  = 0,
		RT_MUSIC  = 1,
		RT_BITMAP = 2, // full screen video buffer, size=0x7D00
		RT_PAL    = 3, // palette (1024=vga + 1024=ega), size=2048
		RT_SCRIPT = 4,
		RT_VERTICES = 5,
		RT_UNK    = 6,
	};

	enum DataType {
		DT_DOS,
		DT_AMIGA,
		DT_15TH_EDITION,
		DT_20TH_EDITION,
		DT_WIN31,
		DT_3DO,
		DT_MAC,
	};

	enum {
		MEM_BLOCK_SIZE = 1 * 1024 * 1024,
		ENTRIES_COUNT = 146,
	};

	enum {
		STATUS_NULL,
		STATUS_LOADED,
		STATUS_TOLOAD,
	};

	static const AmigaMemEntry _memListAmigaFR[ENTRIES_COUNT];
	static const AmigaMemEntry _memListAmigaEN[ENTRIES_COUNT];

	Video *_vid;
	const char *_dataDir;
	MemEntry _memList[ENTRIES_COUNT + 1];
	uint16_t _numMemList;
	uint16_t _currentPart, _nextPart;
	uint8_t *_memPtrStart, *_scriptBakPtr, *_scriptCurPtr, *_vidBakPtr, *_vidCurPtr;
	bool _useSegVideo2;
	uint8_t *_segVideoPal;
	uint8_t *_segCode;
	uint8_t *_segVideo1;
	uint8_t *_segVideo2;
	const char *_bankPrefix;
	DataType _dataType;
	ResourceNth *_nth;
	ResourceWin31 *_win31;
	Resource3do *_3do;
	ResourceMac *_mac;

	Resource(Video *vid, const char *dataDir);
	~Resource();

	DataType getDataType() const { return _dataType; }
	void detectVersion();
	const char *getGameTitle(Language lang) const;
	bool readBank(const MemEntry *me, uint8_t *dstBuf);
	void readEntries();
	void readEntriesAmiga(const AmigaMemEntry *entries, int count);
	void dumpEntries();
	void load();
	void invalidateAll();
	void invalidateRes();	
	void update(uint16_t num);
	void loadBmp(int num);
	uint8_t *loadDat(int num);
	void loadFont();
	void loadHeads();
	uint8_t *loadWav(int num);
	const char *getString(int num);
	const char *getMusicPath(int num, char *buf, int bufSize, uint32_t *offset = 0);
	void setupPart(int part);
	void allocMemBlock();
	void freeMemBlock();
};

#endif
