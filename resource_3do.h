
#ifndef RESOURCE_3DO_H__
#define RESOURCE_3DO_H__

#include "intern.h"
#include "file.h"

struct OperaIso;

struct Resource3do {

	const char *_dataPath;
	char _musicPath[32];
	char _cpakPath[64];
	OperaIso *_iso;

	Resource3do(const char *dataPath);
	~Resource3do();

	bool readEntries();

	uint8_t *loadFile(int num, uint8_t *dst, uint32_t *size);
	uint16_t *loadShape555(const char *name, int *w, int *h);
	const char *getMusicName(int num, uint32_t *offset);
	const char *getCpak(const char *name, uint32_t *offset);
};

#endif
