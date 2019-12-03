
#ifndef RESOURCE_NTH_H__
#define RESOURCE_NTH_H__

#include "intern.h"

struct ResourceNth {
	virtual ~ResourceNth() {
	}

	virtual bool init() = 0;
	virtual uint8_t *load(const char *name) = 0;
	virtual uint8_t *loadBmp(int num) = 0;
	virtual void preloadDat(int part, int type, int num) {}
	virtual uint8_t *loadDat(int num, uint8_t *dst, uint32_t *size) = 0;
	virtual uint8_t *loadWav(int num, uint8_t *dst, uint32_t *size) = 0;
	virtual const char *getString(Language lang, int num) = 0;
	virtual const char *getMusicName(int num) = 0;
	virtual void getBitmapSize(int *w, int *h) = 0;

	static ResourceNth *create(int edition, const char *dataPath);
};

#endif
