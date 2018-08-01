
#ifndef RESOURCE_3DO_H__
#define RESOURCE_3DO_H__

#include "intern.h"
#include "file.h"

struct OperaIso;

struct Resource3do {

	const char *_dataPath;
	char _musicPath[32];
	OperaIso *_iso;

	Resource3do(const char *dataPath);
	~Resource3do();

	bool readEntries();

	uint8_t *loadFile(int num, uint8_t *dst, uint32_t *size);
	const char *getMusicName(int num, uint32_t *offset);
};

#endif
