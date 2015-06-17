
#ifndef RESOURCE_3DO_H__
#define RESOURCE_3DO_H__

#include "intern.h"
#include "file.h"

struct Resource3do {

	const char *_dataPath;
	char _musicPath[32];

	Resource3do(const char *dataPath);
	~Resource3do();

	void readEntries();

	uint8_t *loadFile(int num, uint8_t *dst, uint32_t *size);
	const char *getMusicName(int num);
};

#endif
