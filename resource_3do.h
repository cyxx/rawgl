
#ifndef RESOURCE_3DO_H__
#define RESOURCE_3DO_H__

#include "intern.h"

struct Resource3do {

	const char *_dataPath;

	Resource3do(const char *dataPath);
	~Resource3do();

	uint8_t *loadFile(int num, uint8_t *dst, uint32_t *size);
};

#endif
