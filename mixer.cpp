
/*
 * Another World engine rewrite
 * Copyright (C) 2004-2005 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include <SDL.h>
#include <SDL_mixer.h>
#include "file.h"
#include "mixer.h"
#include "sfxplayer.h"
#include "systemstub.h"

struct Mixer_impl {

	static const int kMixFreq = 22050;
	static const int kMixBufSize = 4096;

	Mix_Chunk *_sounds[4];
	uint8_t *_samples[4];
	Mix_Music *_music;

	void init() {
		memset(_sounds, 0, sizeof(_sounds));
		memset(_samples, 0, sizeof(_samples));
		_music = 0;

		Mix_Init(MIX_INIT_OGG | MIX_INIT_FLUIDSYNTH);
		if (Mix_OpenAudio(kMixFreq, AUDIO_S16SYS, 2, kMixBufSize) < 0) {
			warning("Mix_OpenAudio failed: %s", Mix_GetError());
		}
		Mix_AllocateChannels(4);
	}
	void quit() {
		stopAll();
		Mix_CloseAudio();
		Mix_Quit();
	}

	static uint8_t *convertSampleRaw(const uint8_t *data, int len, int fmt, int channels, int freq, uint32_t *size) {
		SDL_AudioCVT cvt;
		memset(&cvt, 0, sizeof(cvt));
		if (SDL_BuildAudioCVT(&cvt, fmt, channels, freq, AUDIO_S16SYS, 2, kMixFreq) < 0) {
			warning("Failed to resample from %d Hz", freq);
			return 0;
		}
		if (cvt.needed) {
			cvt.len = len;
			cvt.buf = (uint8_t *)calloc(1, len * cvt.len_mult);
			if (cvt.buf) {
				memcpy(cvt.buf, data, len);
				SDL_ConvertAudio(&cvt);
			}
			*size = cvt.len_cvt;
			return cvt.buf;
		} else {
			uint8_t *buf = (uint8_t *)malloc(len);
			if (buf) {
				memcpy(buf, data, len);
			}
			*size = len;
			return buf;
		}
	}

	void playSoundRaw(uint8_t channel, const uint8_t *data, uint16_t freq, uint8_t volume) {
		stopSound(channel);
		const int len = READ_BE_UINT16(data) * 2;
//		const int loopLen = READ_BE_UINT16(data + 2) * 2;
		data += 8;
		uint32_t size;
		uint8_t *sample = convertSampleRaw(data + 8, len, AUDIO_S8, 1, freq, &size);
		if (sample) {
			Mix_Chunk *chunk = Mix_QuickLoad_RAW(sample, size);
			playSound(channel, volume, chunk);
			_samples[channel] = sample;
		}
	}
	void playSoundWav(uint8_t channel, const uint8_t *data, uint8_t volume) {
		uint32_t size = READ_LE_UINT32(data + 4) + 8;
		// check format for QuickLoad
		bool canQuickLoad = (AUDIO_S16SYS == AUDIO_S16LSB);
		if (canQuickLoad) {
			if (memcmp(data + 8, "WAVEfmt ", 8) == 0 && READ_LE_UINT32(data + 16) == 16) {
				const uint8_t *fmt = data + 20;
				const int format = READ_LE_UINT16(fmt);
				const int channels = READ_LE_UINT16(fmt + 2);
				const int rate = READ_LE_UINT32(fmt + 4);
				const int bits = READ_LE_UINT16(fmt + 14);
				debug(DBG_SND, "wave format %d channels %d rate %d bits %d", format, channels, rate, bits);
				canQuickLoad = (format == 1 && channels == 2 && rate == kMixFreq && bits == 16);
			}
                }
		if (canQuickLoad) {
			Mix_Chunk *chunk = Mix_QuickLoad_WAV(const_cast<uint8_t *>(data));
			playSound(channel, volume, chunk);
		} else {
			SDL_RWops *rw = SDL_RWFromConstMem(data, size);
			Mix_Chunk *chunk = Mix_LoadWAV_RW(rw, 1);
			playSound(channel, volume, chunk);
		}
	}
	void playSoundAiff(uint8_t channel, const uint8_t *data, uint8_t volume) {
#if 1
		const uint32_t size = READ_BE_UINT32(data + 4) + 8;
		SDL_RWops *rw = SDL_RWFromConstMem(data, size);
		Mix_Chunk *chunk = Mix_LoadWAV_RW(rw, 1);
		playSound(channel, volume, chunk);
#else
		uint8_t *sample = 0;
		uint32_t sampleSize;
		int channels = 0;
		int fmt = -1;
		int rate = 0;
		const uint32_t size = READ_BE_UINT32(data + 4);
		if (memcmp(data + 8, "AIFF", 4) == 0) {
			const uint32_t size = READ_BE_UINT32(data + 4);
			for (int offset = 12; offset < size; ) {
				const uint32_t sz = READ_BE_UINT32(data + offset + 4);
				if (memcmp(data + offset, "COMM", 4) == 0) {
					channels = READ_BE_UINT16(data + offset + 8);
					const int bits = READ_BE_UINT16(data + offset + 14);
					rate = ieee754_80bits(data + offset + 16);
					debug(DBG_SND, "aiff channels %d rate %d bits %d", channels, rate, bits);
					if (bits == 8) {
						fmt = AUDIO_S8;
					} else if (bits == 16) {
						fmt = AUDIO_S16MSB;
					} else {
						break;
					}
				} else if (memcmp(data + offset, "SSND", 4) == 0) {
					sample = convertSampleRaw(data + offset + 8, sz, fmt, channels, rate, &sampleSize);
					break;
				}
				offset += sz + 8;
			}
		}
		if (sample) {
			Mix_Chunk *chunk = Mix_QuickLoad_RAW(sample, sampleSize);
			playSound(channel, volume, chunk);
			_samples[channel] = sample;
		}
#endif
	}
	void playSound(uint8_t channel, int volume, Mix_Chunk *chunk) {
		if (chunk) {
			Mix_PlayChannel(channel, chunk, 0);
		}
		setChannelVolume(channel, volume);
		_sounds[channel] = chunk;
	}
	void stopSound(uint8_t channel) {
		Mix_HaltChannel(channel);
		Mix_FreeChunk(_sounds[channel]);
		_sounds[channel] = 0;
		free(_samples[channel]);
		_samples[channel] = 0;
	}
	void setChannelVolume(uint8_t channel, uint8_t volume) {
		Mix_Volume(channel, volume * MIX_MAX_VOLUME / 63);
	}

	void playMusic(const char *path) {
		stopMusic();
		_music = Mix_LoadMUS(path);
		if (_music) {
			Mix_VolumeMusic(MIX_MAX_VOLUME / 2);
			Mix_PlayMusic(_music, 0);
		} else {
			warning("Failed to load music '%s', %s", path, Mix_GetError());
		}
	}
	void stopMusic() {
		Mix_HaltMusic();
		Mix_FreeMusic(_music);
		_music = 0;
	}

	static void mixSfxPlayer(void *data, uint8_t *s16buf, int len) {
		len /= 2;
		int8_t s8buf[len];
		memset(s8buf, 0, sizeof(s8buf));
		((SfxPlayer *)data)->readSamples(s8buf, len / 2);
		for (int i = 0; i < len; ++i) {
			*(int16_t *)&s16buf[i * 2] = 256 * (int16_t)s8buf[i];
		}
	}

	void playSfxMusic(SfxPlayer *sfx) {
		Mix_HookMusic(mixSfxPlayer, sfx);
	}
	void stopSfxMusic() {
		Mix_HookMusic(0, 0);
	}

	void stopAll() {
		for (int i = 0; i < 4; ++i) {
			stopSound(i);
		}
		stopMusic();
		stopSfxMusic();
	}
};

Mixer::Mixer(SfxPlayer *sfx)
	: _sfx(sfx) {
}

void Mixer::init() {
	_impl = new Mixer_impl();
	_impl->init();
}

void Mixer::quit() {
	stopAll();
	if (_impl) {
		_impl->quit();
		delete _impl;
	}
}

void Mixer::playSoundRaw(uint8_t channel, const uint8_t *data, uint16_t freq, uint8_t volume) {
	debug(DBG_SND, "Mixer::playChannel(%d, %d, %d)", channel, freq, volume);
	if (_impl) {
		return _impl->playSoundRaw(channel, data, freq, volume);
	}
}

void Mixer::playSoundWav(uint8_t channel, const uint8_t *data, uint8_t volume) {
	debug(DBG_SND, "Mixer::playSoundWav(%d, %d)", channel, volume);
	if (_impl) {
		return _impl->playSoundWav(channel, data, volume);
	}
}

void Mixer::playSoundAiff(uint8_t channel, const uint8_t *data, uint8_t volume) {
	debug(DBG_SND, "Mixer::playSoundAiff(%d, %d)", channel, volume);
	if (_impl) {
		return _impl->playSoundAiff(channel, data, volume);
	}
}

void Mixer::stopSound(uint8_t channel) {
	debug(DBG_SND, "Mixer::stopChannel(%d)", channel);
	if (_impl) {
		return _impl->stopSound(channel);
	}
}

void Mixer::setChannelVolume(uint8_t channel, uint8_t volume) {
	debug(DBG_SND, "Mixer::setChannelVolume(%d, %d)", channel, volume);
	if (_impl) {
		return _impl->setChannelVolume(channel, volume);
	}
}

void Mixer::playMusic(const char *path) {
	debug(DBG_SND, "Mixer::playMusic(%s)", path);
	if (_impl) {
		return _impl->playMusic(path);
	}
}

void Mixer::stopMusic() {
	debug(DBG_SND, "Mixer::stopMusic()");
	if (_impl) {
		return _impl->stopMusic();
	}
}

void Mixer::playSfxMusic(int num) {
	debug(DBG_SND, "Mixer::playSfxMusic(%d)", num);
	if (_impl && _sfx) {
		_sfx->play(Mixer_impl::kMixFreq);
		return _impl->playSfxMusic(_sfx);
	}
}

void Mixer::stopSfxMusic() {
	debug(DBG_SND, "Mixer::stopSfxMusic()");
	if (_impl && _sfx) {
		_sfx->stop();
		return _impl->stopSfxMusic();
	}
}

void Mixer::stopAll() {
	debug(DBG_SND, "Mixer::stopAll()");
	if (_impl) {
		return _impl->stopAll();
	}
}
