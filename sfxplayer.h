/* 
 * Another World Interpreter 
 * (c) 2004-2005 Gregory Montoir
 */

#ifndef __SFXPLAYER_H__
#define __SFXPLAYER_H__

#include <rtems.h>
#include <tmacros.h>

#include "intern.h"

struct SfxInstrument {
	uint8 *data;
	uint16 volume;
};

struct SfxModule {
	const uint8 *data;
	uint16 curPos;
	uint8 curOrder;
	uint8 numOrder;
	uint8 orderTable[0x80];
	SfxInstrument samples[15];
};

struct SfxPattern {
	uint16 note_1;
	uint16 note_2;
	uint16 sampleStart;
	uint8 *sampleBuffer;
	uint16 sampleLen;
	uint16 loopPos;
	uint8 *loopData;
	uint16 loopLen;
	uint16 sampleVolume;
};

struct Mixer;
struct Resource;
struct SystemStub;

struct SfxPlayer {
	Mixer *_mix;
	Resource *_res;
	SystemStub *_stub;

	void *_mutex;
	void *_timerId;
	uint16 _delay;
	uint16 _resNum;
	SfxModule _sfxMod;
	int16 *_markVar;

	SfxPlayer(Mixer *mix, Resource *res, SystemStub *stub);
	void init();
	void free();

	void setEventsDelay(uint16 delay);
	void loadSfxModule(uint16 resNum, uint16 delay, uint8 pos);
	void prepareInstruments(const uint8 *p);
	void start();
	void stop();
	void handleEvents();
	void handlePattern(uint8 channel, const uint8 *patternData);

	static void eventsCallback(void *param);
};

#endif
