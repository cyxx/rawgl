
/*
 * Another World engine rewrite
 * Copyright (C) 2004-2005 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifndef MIXER_H__
#define MIXER_H__

#include "intern.h"

struct AifcPlayer;
struct SfxPlayer;
struct Mixer_impl;

struct Mixer {
	AifcPlayer *_aifc;
	SfxPlayer *_sfx;
	Mixer_impl *_impl;

	Mixer(SfxPlayer *sfx);
	void init();
	void quit();
	void update();

	void playSoundRaw(uint8_t channel, const uint8_t *data, uint16_t freq, uint8_t volume);
	void playSoundWav(uint8_t channel, const uint8_t *data, uint16_t freq, uint8_t volume, uint8_t loop);
	void playSoundAiff(uint8_t channel, const uint8_t *data, uint8_t volume);
	void stopSound(uint8_t channel);
	void setChannelVolume(uint8_t channel, uint8_t volume);
	void playMusic(const char *path, uint8_t loop);
	void stopMusic();
	void playAifcMusic(const char *path, uint32_t offset);
	void stopAifcMusic();
	void playSfxMusic(int num);
	void stopSfxMusic();
	void stopAll();
};

#endif
