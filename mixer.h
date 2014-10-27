
/*
 * Another World engine rewrite
 * Copyright (C) 2004-2005 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifndef MIXER_H__
#define MIXER_H__

#include "intern.h"

struct MixerChannel;
struct SfxPlayer;
struct SystemStub;

struct Mixer {
	enum {
		NUM_CHANNELS = 4
	};

	SfxPlayer *_sfx;
	SystemStub *_stub;
	MixerChannel *_channels[NUM_CHANNELS];
	int _volumes[NUM_CHANNELS];
	MixerChannel *_music;

	Mixer(SystemStub *stub);
	void init();
	void free();

	void playSoundRaw(uint8_t channel, const uint8_t *data, uint16_t freq, uint8_t volume);
	void playSoundWav(uint8_t channel, const uint8_t *data, uint8_t volume);
	void stopSound(uint8_t channel);
	void setChannelVolume(uint8_t channel, uint8_t volume);
	void playMusic(const char *path);
	void stopMusic();
	void playSfxMusic(int num);
	void stopSfxMusic();
	void stopAll();
	void mix(int8_t *buf, int len);

	static void mixCallback(void *param, uint8_t *buf, int len);
};

#endif
