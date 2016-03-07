
#ifndef AIFC_PLAYER_H__
#define AIFC_PLAYER_H__

#include "intern.h"
#include "file.h"

struct AifcPlayer {

	File _f;
	uint32_t _ssndOffset;
	uint32_t _ssndSize;
	uint32_t _pos;
	int16_t _samples[2];

	AifcPlayer();

	bool play(int mixRate, const char *path);
	void stop();

	int8_t readData();
	void readSamples(int16_t *buf, int len);
};

#endif
