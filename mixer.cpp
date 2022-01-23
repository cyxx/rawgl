
/*
 * Another World engine rewrite
 * Copyright (C) 2004-2005 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include <SDL.h>
#define MIX_INIT_FLUIDSYNTH MIX_INIT_MID // renamed with SDL2_mixer >= 2.0.2
#include <SDL_mixer.h>
#include <map>
#include "aifcplayer.h"
#include "mixer.h"
#include "sfxplayer.h"
#include "util.h"

static int16_t toS16(int a) {
	if (a <= -128) {
		return -32768;
	} else if (a >= 127) {
		return 32767;
	} else {
		const uint8_t u8 = (a ^ 0x80);
		return ((u8 << 8) | u8) - 32768;
	}
}

static int16_t mixS16(int sample1, int sample2) {
	const int sample = sample1 + sample2;
	return sample < -32768 ? -32768 : ((sample > 32767 ? 32767 : sample));
}

struct MixerChannel {
	const uint8_t *_data;
	Frac _pos;
	uint32_t _len;
	uint32_t _loopLen, _loopPos;
	int _volume;

	void init(const uint8_t *data, int freq, int volume, int mixingFreq) {
		_data = data + 8;
		_pos.offset = 0;
		_pos.inc = (freq << Frac::BITS) / mixingFreq;

		const int len = READ_BE_UINT16(data) * 2;
		_loopLen = READ_BE_UINT16(data + 2) * 2;
		_loopPos = _loopLen ? len : 0;
		_len = len;

		_volume = volume;
	}

	void mix(int16_t &sample) {
		if (_data) {
			const uint32_t pos = _pos.getInt();
			_pos.offset += _pos.inc;
			if (_loopLen != 0) {
				if (pos >= _loopPos + _loopLen) {
					_pos.offset = _loopPos << Frac::BITS;
				}
			} else {
				if (pos >= _len) {
					_data = 0;
					return;
				}
			}
			sample = mixS16(sample, toS16(((int8_t)_data[pos]) * _volume / 64));
		}
	}
};

static Mix_Chunk *loadAndConvertWav(SDL_RWops *rw, int freeRw, int freq, SDL_AudioFormat dstFormat, int dstChannels, int dstFreq) {
	SDL_AudioSpec wavSpec;
	Uint8 *wavBuf;
	Uint32 wavLen;
	SDL_AudioCVT cvt;

	if (!SDL_LoadWAV_RW(rw, freeRw, &wavSpec, &wavBuf, &wavLen)) {
		return 0;
	}

	if (wavSpec.freq == 22050 || wavSpec.freq == 44100 || wavSpec.freq == 48000) {
		freq = (int)(freq * (wavSpec.freq / 9943.0f));
	}

	if (SDL_BuildAudioCVT(&cvt, wavSpec.format, wavSpec.channels, freq, dstFormat, dstChannels, dstFreq) < 0) {
		SDL_free(wavBuf);
		return 0;
	}

	cvt.len = wavLen;
	if (cvt.len_mult > 1) {
		cvt.buf = (Uint8 *)SDL_calloc(1, wavLen * cvt.len_mult);
		if (cvt.buf == 0) {
			SDL_free(wavBuf);
			return 0;
		}
		SDL_memcpy(cvt.buf, wavBuf, wavLen);
		SDL_free(wavBuf);
	} else {
		cvt.buf = wavBuf;
	}

	if (SDL_ConvertAudio(&cvt) < 0) {
		SDL_free(cvt.buf);
		return 0;
	}

	Mix_Chunk *chunk = Mix_QuickLoad_RAW(cvt.buf, cvt.len_cvt);
	if (chunk == 0) {
		SDL_free(cvt.buf);
		return 0;
	}

	chunk->allocated = 1; // free allocated buffer when freeing chunk
	return chunk;
}

struct Mixer_impl {

	static const int kMixFreq = 44100;
	static const SDL_AudioFormat kMixFormat = AUDIO_S16SYS;
	static const int kMixSoundChannels = 2;
	static const int kMixBufSize = 4096;
	static const int kMixChannels = 4;

	Mix_Chunk *_sounds[kMixChannels];
	Mix_Music *_music;
	MixerChannel _channels[kMixChannels]; // 0,3:left 1,2:right
	SfxPlayer *_sfx;
	std::map<int, Mix_Chunk *> _preloads; // AIFF preloads (3DO)

	void init(bool softwareMixer) {
		memset(_sounds, 0, sizeof(_sounds));
		_music = 0;
		memset(_channels, 0, sizeof(_channels));
		_sfx = 0;

		Mix_Init(MIX_INIT_OGG | MIX_INIT_FLUIDSYNTH);
		if (Mix_OpenAudio(kMixFreq, kMixFormat, kMixSoundChannels, kMixBufSize) < 0) {
			warning("Mix_OpenAudio failed: %s", Mix_GetError());
		}
		if (softwareMixer) {
			Mix_HookMusic(mixAudio, this);
		} else {
			Mix_AllocateChannels(kMixChannels);
		}
	}
	void quit() {
		stopAll();
		Mix_CloseAudio();
		Mix_Quit();
	}

	void update() {
		for (int i = 0; i < kMixChannels; ++i) {
			if (_sounds[i] && !Mix_Playing(i)) {
				freeSound(i);
			}
		}
	}

	void playSoundRaw(uint8_t channel, const uint8_t *data, int freq, uint8_t volume) {
		SDL_LockAudio();
		_channels[channel].init(data, freq, volume, kMixFreq);
		SDL_UnlockAudio();
	}
	void playSoundWav(uint8_t channel, const uint8_t *data, int freq, uint8_t volume, int loops = 0) {
		const uint32_t size = READ_LE_UINT32(data + 4) + 8;
		SDL_RWops *rw = SDL_RWFromConstMem(data, size);
		Mix_Chunk *chunk = loadAndConvertWav(rw, 1, freq, kMixFormat, kMixSoundChannels, kMixFreq);
		playSound(channel, volume, chunk, loops);
	}
	void playSound(uint8_t channel, int volume, Mix_Chunk *chunk, int loops = 0) {
		stopSound(channel);
		if (chunk) {
			Mix_PlayChannel(channel, chunk, loops);
		}
		setChannelVolume(channel, volume);
		_sounds[channel] = chunk;
	}
	void stopSound(uint8_t channel) {
		SDL_LockAudio();
		_channels[channel]._data = 0;
		SDL_UnlockAudio();
		Mix_HaltChannel(channel);
		freeSound(channel);
	}
	void freeSound(int channel) {
		Mix_FreeChunk(_sounds[channel]);
		_sounds[channel] = 0;
	}
	void setChannelVolume(uint8_t channel, uint8_t volume) {
		SDL_LockAudio();
		_channels[channel]._volume = volume;
		SDL_UnlockAudio();
		Mix_Volume(channel, volume * MIX_MAX_VOLUME / 63);
	}

	void playMusic(const char *path, int loops = 0) {
		stopMusic();
		_music = Mix_LoadMUS(path);
		if (_music) {
			Mix_VolumeMusic(MIX_MAX_VOLUME / 2);
			Mix_PlayMusic(_music, loops);
		} else {
			warning("Failed to load music '%s', %s", path, Mix_GetError());
		}
	}
	void stopMusic() {
		Mix_HaltMusic();
		Mix_FreeMusic(_music);
		_music = 0;
	}

	static void mixAifcPlayer(void *data, uint8_t *s16buf, int len) {
		((AifcPlayer *)data)->readSamples((int16_t *)s16buf, len / 2);
	}
	void playAifcMusic(AifcPlayer *aifc) {
		Mix_HookMusic(mixAifcPlayer, aifc);
	}
	void stopAifcMusic() {
		Mix_HookMusic(0, 0);
	}

	void playSfxMusic(SfxPlayer *sfx) {
		SDL_LockAudio();
		_sfx = sfx;
		_sfx->play(kMixFreq);
		SDL_UnlockAudio();
	}
	void stopSfxMusic() {
		SDL_LockAudio();
		if (_sfx) {
			_sfx->stop();
			_sfx = 0;
		}
		SDL_UnlockAudio();
	}

	void mixChannels(int16_t *samples, int count) {
		for (int i = 0; i < count; i += 2) {
			_channels[0].mix(*samples);
			_channels[3].mix(*samples);
			++samples;
			_channels[1].mix(*samples);
			_channels[2].mix(*samples);
			++samples;
                }
	}

	static void mixAudio(void *data, uint8_t *s16buf, int len) {
		memset(s16buf, 0, len);
		Mixer_impl *mixer = (Mixer_impl *)data;
		mixer->mixChannels((int16_t *)s16buf, len / sizeof(int16_t));
		if (mixer->_sfx) {
			mixer->_sfx->readSamples((int16_t *)s16buf, len / sizeof(int16_t));
		}
	}

	void stopAll() {
		for (int i = 0; i < kMixChannels; ++i) {
			stopSound(i);
		}
		stopMusic();
		stopSfxMusic();
		for (std::map<int, Mix_Chunk *>::iterator it = _preloads.begin(); it != _preloads.end(); ++it) {
			debug(DBG_SND, "Flush preload %d", it->first);
			Mix_FreeChunk(it->second);
		}
		_preloads.clear();
	}

	void preloadSoundAiff(int num, const uint8_t *data) {
		if (_preloads.find(num) != _preloads.end()) {
			warning("AIFF sound %d is already preloaded", num);
		} else {
			const uint32_t size = READ_BE_UINT32(data + 4) + 8;
			SDL_RWops *rw = SDL_RWFromConstMem(data, size);
			Mix_Chunk *chunk = Mix_LoadWAV_RW(rw, 1);
			_preloads[num] = chunk;
		}
	}

	void playSoundAiff(int channel, int num, int volume) {
		if (_preloads.find(num) == _preloads.end()) {
			warning("AIFF sound %d is not preloaded", num);
		} else {
			Mix_Chunk *chunk = _preloads[num];
			Mix_PlayChannel(channel, chunk, 0);
			Mix_Volume(channel, volume * MIX_MAX_VOLUME / 63);
		}
	}
};

Mixer::Mixer(SfxPlayer *sfx)
	: _aifc(0), _sfx(sfx) {
}

void Mixer::init(bool softwareMixer) {
	_impl = new Mixer_impl();
	_impl->init(softwareMixer);
}

void Mixer::quit() {
	stopAll();
	if (_impl) {
		_impl->quit();
		delete _impl;
	}
	delete _aifc;
}

void Mixer::update() {
	if (_impl) {
		_impl->update();
	}
}

void Mixer::playSoundRaw(uint8_t channel, const uint8_t *data, uint16_t freq, uint8_t volume) {
	debug(DBG_SND, "Mixer::playChannel(%d, %d, %d)", channel, freq, volume);
	if (_impl) {
		return _impl->playSoundRaw(channel, data, freq, volume);
	}
}

void Mixer::playSoundWav(uint8_t channel, const uint8_t *data, uint16_t freq, uint8_t volume, uint8_t loop) {
	debug(DBG_SND, "Mixer::playSoundWav(%d, %d, %d)", channel, volume, loop);
	if (_impl) {
		return _impl->playSoundWav(channel, data, freq, volume, (loop != 0) ? -1 : 0);
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

void Mixer::playMusic(const char *path, uint8_t loop) {
	debug(DBG_SND, "Mixer::playMusic(%s, %d)", path, loop);
	if (_impl) {
		return _impl->playMusic(path, (loop != 0) ? -1 : 0);
	}
}

void Mixer::stopMusic() {
	debug(DBG_SND, "Mixer::stopMusic()");
	if (_impl) {
		return _impl->stopMusic();
	}
}

void Mixer::playAifcMusic(const char *path, uint32_t offset) {
	debug(DBG_SND, "Mixer::playAifcMusic(%s)", path);
	if (!_aifc) {
		_aifc = new AifcPlayer();
	}
	if (_impl) {
		_impl->stopAifcMusic();
		if (_aifc->play(Mixer_impl::kMixFreq, path, offset)) {
			_impl->playAifcMusic(_aifc);
		}
	}
}

void Mixer::stopAifcMusic() {
	debug(DBG_SND, "Mixer::stopAifcMusic()");
	if (_impl && _aifc) {
		_aifc->stop();
		_impl->stopAifcMusic();
	}
}

void Mixer::playSfxMusic(int num) {
	debug(DBG_SND, "Mixer::playSfxMusic(%d)", num);
	if (_impl && _sfx) {
		return _impl->playSfxMusic(_sfx);
	}
}

void Mixer::stopSfxMusic() {
	debug(DBG_SND, "Mixer::stopSfxMusic()");
	if (_impl && _sfx) {
		return _impl->stopSfxMusic();
	}
}

void Mixer::stopAll() {
	debug(DBG_SND, "Mixer::stopAll()");
	if (_impl) {
		return _impl->stopAll();
	}
}

void Mixer::preloadSoundAiff(uint8_t num, const uint8_t *data) {
	debug(DBG_SND, "Mixer::preloadSoundAiff(num:%d, data:%p)", num, data);
	if (_impl) {
		return _impl->preloadSoundAiff(num, data);
	}
}

void Mixer::playSoundAiff(uint8_t channel, uint8_t num, uint8_t volume) {
	debug(DBG_SND, "Mixer::playSoundAiff()");
	if (_impl) {
		return _impl->playSoundAiff(channel, num, volume);
	}
}
