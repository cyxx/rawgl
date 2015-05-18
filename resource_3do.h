
#ifndef RESOURCE_3DO_H__
#define RESOURCE_3DO_H__

#include "intern.h"
#include "file.h"

struct OperaEntry {
	char name[32];
	uint32_t offset;
	uint32_t size;
};

struct OperaIso {
	static const int BLOCKSIZE = 2048;

	File _f;
	OperaEntry _entries[300];
	int _entriesCount;

	OperaIso();

	void readEntries(int block = -1);
	const OperaEntry *findEntry(const char *name) const;
	uint8_t *readFile(const char *name, uint8_t *dst, uint32_t *size);
};

struct Resource3do {

	const char *_dataPath;
	OperaIso _iso;
	bool _useIso;

	Resource3do(const char *dataPath);
	~Resource3do();

	void readEntries();

	uint8_t *loadFile(int num, uint8_t *dst, uint32_t *size);
};

#endif
