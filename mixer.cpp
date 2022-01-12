
/*
 * Another World engine rewrite
 * Copyright (C) 2004-2005 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include <SDL.h>
#define MIX_INIT_FLUIDSYNTH MIX_INIT_MID // renamed with SDL2_mixer >= 2.0.2
#include <SDL_mixer.h>
#include "aifcplayer.h"
#include "file.h"
#include "mixer.h"
#include "sfxplayer.h"
#include "systemstub.h"
#include "util.h"

static uint8_t *convertMono8(SDL_AudioCVT *cvt, const uint8_t *data, int freq, int size, int *cvtLen) {
	static const int kHz = 11025;
	assert(kHz / freq <= 4);
	uint8_t *out = (uint8_t *)malloc(size * 4 * cvt->len_mult);
	if (out) {
		// point resampling
		Frac pos;
		pos.offset = 0;
		pos.inc = (freq << Frac::BITS) / kHz;
		int len = 0;
		for (; int(pos.getInt()) < size; pos.offset += pos.inc) {
			out[len] = data[pos.getInt()];
			++len;
		}
		// convert to mixer format
		cvt->len = len;
		cvt->buf = out;
		if (SDL_ConvertAudio(cvt) < 0) {
			free(out);
			return 0;
		}
		out = cvt->buf;
		*cvtLen = cvt->len_cvt;
	}
	return out;
}

static Mix_Chunk *loadAndConvertWav(SDL_RWops *rw, int freeRw, int freq, SDL_AudioFormat dstFormat, int dstChannels, int dstFreq)
{
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
	uint8_t *_samples[kMixChannels];
	SDL_AudioCVT _cvt;

	void init() {
		memset(_sounds, 0, sizeof(_sounds));
		_music = 0;
		memset(_samples, 0, sizeof(_samples));

		Mix_Init(MIX_INIT_OGG | MIX_INIT_FLUIDSYNTH);
		if (Mix_OpenAudio(kMixFreq, kMixFormat, kMixSoundChannels, kMixBufSize) < 0) {
			warning("Mix_OpenAudio failed: %s", Mix_GetError());
		}
		Mix_AllocateChannels(kMixChannels);
		memset(&_cvt, 0, sizeof(_cvt));
		if (SDL_BuildAudioCVT(&_cvt, AUDIO_S8, 1, 11025, kMixFormat, kMixSoundChannels, kMixFreq) < 0) {
			warning("SDL_BuildAudioCVT failed: %s", SDL_GetError());
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
		int len = READ_BE_UINT16(data) * 2;
		const int loopLen = READ_BE_UINT16(data + 2) * 2;
		if (loopLen != 0) {
			len = loopLen;
		}
		int sampleLen = 0;
		uint8_t *sample = convertMono8(&_cvt, data + 8, freq, len, &sampleLen);
		if (sample) {
			Mix_Chunk *chunk = Mix_QuickLoad_RAW(sample, sampleLen);
			playSound(channel, volume, chunk, (loopLen != 0) ? -1 : 0);
			_samples[channel] = sample;
		}
	}
	void playSoundWav(uint8_t channel, const uint8_t *data, int freq, uint8_t volume, int loops = 0) {
		if (memcmp(data + 8, "WAVEfmt ", 8) == 0 && READ_LE_UINT32(data + 16) == 16) {
			const uint8_t *fmt = data + 20;
			const int format = READ_LE_UINT16(fmt);
			const int channels = READ_LE_UINT16(fmt + 2);
			const int rate = READ_LE_UINT32(fmt + 4);
			const int bits = READ_LE_UINT16(fmt + 14);
			debug(DBG_SND, "wave format %d channels %d rate %d bits %d", format, channels, rate, bits);
		}
		const uint32_t size = READ_LE_UINT32(data + 4) + 8;
		SDL_RWops *rw = SDL_RWFromConstMem(data, size);
		Mix_Chunk *chunk = loadAndConvertWav(rw, 1, freq, kMixFormat, kMixSoundChannels, kMixFreq);
		playSound(channel, volume, chunk, loops);
	}
	void playSoundAiff(uint8_t channel, const uint8_t *data, uint8_t volume) {
		const uint32_t size = READ_BE_UINT32(data + 4) + 8;
		SDL_RWops *rw = SDL_RWFromConstMem(data, size);
		Mix_Chunk *chunk = Mix_LoadWAV_RW(rw, 1);
		playSound(channel, volume, chunk);
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
		Mix_HaltChannel(channel);
		freeSound(channel);
	}
	void freeSound(int channel) {
		Mix_FreeChunk(_sounds[channel]);
		_sounds[channel] = 0;
		free(_samples[channel]);
		_samples[channel] = 0;
	}
	void setChannelVolume(uint8_t channel, uint8_t volume) {
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

	static void mixSfxPlayer(void *data, uint8_t *s16buf, int len) {
		((SfxPlayer *)data)->readSamples((int16_t *)s16buf, len / sizeof(int16_t));
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
	: _aifc(0), _sfx(sfx) {
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
		_impl->stopSfxMusic();
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
