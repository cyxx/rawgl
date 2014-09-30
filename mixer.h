
/*
 * Another World engine rewrite
 * Copyright (C) 2004-2005 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifndef MIXER_H__
#define MIXER_H__

#include "intern.h"

struct MixerChunk {
	const uint8_t *data;
	uint16_t len;
	uint16_t loopPos;
	uint16_t loopLen;
	enum {
		FMT_S8,
		FMT_U8,
		FMT_S16,
	} fmt;

	MixerChunk() {
		memset(this, 0, sizeof(*this));
	}

	void readRaw(uint8_t *buf);
	void readWav(uint8_t *buf);
};

struct OldMixerChannel {
	uint8_t active;
	uint8_t volume;
	MixerChunk chunk;
	uint32_t chunkPos;
	uint32_t chunkInc;
};

struct MixerChannel;
struct Serializer;
struct SystemStub;

struct Mixer {
	enum {
		NUM_CHANNELS = 4
	};

	SystemStub *_stub;
	OldMixerChannel _channels[NUM_CHANNELS];
	MixerChannel *_music;

	Mixer(SystemStub *stub);
	void init();
	void free();

	void playChannel(uint8_t channel, const MixerChunk *mc, uint16_t freq, uint8_t volume);
	void stopChannel(uint8_t channel);
	void setChannelVolume(uint8_t channel, uint8_t volume);
	void playMusic(const char *path);
	void stopMusic();
	void stopAll();
	void mix(int8_t *buf, int len);

	static void mixCallback(void *param, uint8_t *buf, int len);
};

#endif
