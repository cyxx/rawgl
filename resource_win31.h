
#ifndef RESOURCE_WIN31_H__
#define RESOURCE_WIN31_H__

#include "intern.h"
#include "file.h"

struct Win31BankEntry {
	char name[16];
	uint8_t type;
	uint32_t offset;
	uint32_t size;
	uint32_t packedSize;
};

struct ResourceWin31 {

	static const char *FILENAME;

	File _f;
	Win31BankEntry *_entries;
	int _entriesCount;

	ResourceWin31(const char *dataPath);

	void readEntries();
};

#endif
