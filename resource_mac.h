
#ifndef RESOURCE_MAC_H__
#define RESOURCE_MAC_H__

#include "intern.h"

struct ResourceMac {

	static const char *FILE017;

	const char *_dataPath;

	ResourceMac(const char *dataPath);
	~ResourceMac();

	uint8_t *loadFile(int num, uint8_t *dst, uint32_t *size);
};

#endif
