
#ifndef AIFC_PLAYER_H__
#define AIFC_PLAYER_H__

#include "intern.h"
#include "file.h"

struct AifcPlayer {

	File _f;
	uint32_t _ssndOffset;
	uint32_t _ssndSize;
	uint32_t _pos;
	int16_t _sampleL, _sampleR;
	Frac _rate;

	AifcPlayer();

	bool play(int mixRate, const char *path, uint32_t offset);
	void stop();

	int8_t readSampleData();
	void decodeSamples();
	void readSamples(int16_t *buf, int len);
};

#endif
