
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
	const char *_dataPath;
	Win31BankEntry *_entries;
	int _entriesCount;
	uint8_t *_textBuf;
	const char *_stringsTable[614];

	ResourceWin31(const char *dataPath);
	~ResourceWin31();

	bool readEntries();
	uint8_t *loadFile(int num, uint8_t *dst, uint32_t *size);
	void readStrings();
	const char *getString(int num) const;
	const char *getMusicName(int num) const;
};

#endif
