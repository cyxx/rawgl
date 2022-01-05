
/*
 * Another World engine rewrite
 * Copyright (C) 2004-2005 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifndef SFXPLAYER_H__
#define SFXPLAYER_H__

#include "intern.h"

struct SfxInstrument {
	uint8_t *data;
	uint16_t volume;
};

struct SfxModule {
	const uint8_t *data;
	uint16_t curPos;
	uint8_t curOrder;
	uint8_t numOrder;
	uint8_t *orderTable;
	SfxInstrument samples[15];
};

struct SfxPattern {
	uint16_t note_1;
	uint16_t note_2;
	uint16_t sampleStart;
	uint8_t *sampleBuffer;
	uint16_t sampleLen;
	uint16_t loopPos;
	uint16_t loopLen;
	uint16_t sampleVolume;
};

struct SfxChannel {
	uint8_t *sampleData;
	uint16_t sampleLen;
	uint16_t sampleLoopPos;
	uint16_t sampleLoopLen;
	uint16_t volume;
	Frac pos;
};

struct Resource;

struct SfxPlayer {
	enum {
		NUM_CHANNELS = 4
	};

	Resource *_res;

	uint16_t _delay;
	uint16_t _resNum;
	SfxModule _sfxMod;
	int16_t *_syncVar;
	bool _playing;
	int _rate;
	int _samplesLeft;
	SfxChannel _channels[NUM_CHANNELS];

	SfxPlayer(Resource *res);

	void setEventsDelay(uint16_t delay);
	void loadSfxModule(uint16_t resNum, uint16_t delay, uint8_t pos);
	void prepareInstruments(const uint8_t *p);
	void play(int rate);
	void mixSamples(int16_t *buf, int len);
	void readSamples(int16_t *buf, int len);
	void start();
	void stop();
	void handleEvents();
	void handlePattern(uint8_t channel, const uint8_t *patternData);
};

#endif
