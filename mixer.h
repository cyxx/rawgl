/* 
 * Another World Interpreter 
 * (c) 2004-2005 Gregory Montoir
 */

#ifndef __MIXER_H__
#define __MIXER_H__

#include "intern.h"

struct MixerChunk {
	const uint8 *data;
	uint16 len;
	uint16 loopPos;
	uint16 loopLen;
};

struct MixerChannel {
	uint8 active;
	uint8 volume;
	MixerChunk chunk;
	uint32 chunkPos;
	uint32 chunkInc;
};

struct SystemStub;

struct Mixer {
	enum {
		NUM_CHANNELS = 4
	};

	void *_mutex;
	SystemStub *_stub;
	MixerChannel _channels[NUM_CHANNELS];

	Mixer(SystemStub *stub);
	void init();
	void free();

	void playChannel(uint8 channel, const MixerChunk *mc, uint16 freq, uint8 volume);
	void stopChannel(uint8 channel);
	void setChannelVolume(uint8 channel, uint8 volume);
	void stopAll();
	void mix(void *sbuf);

	static void mixCallback(void *param, void *sbuf);
};

#endif
