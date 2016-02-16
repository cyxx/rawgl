/* 
 * Another World Interpreter 
 * (c) 2004-2005 Gregory Montoir
 */

#include <SDL.h>
#include "systemstub.h"


struct SystemStub_SDL : SystemStub {
	typedef void (SystemStub_SDL::*ScaleProc)(uint16 *dst, uint16 dstPitch, const uint16 *src, uint16 srcPitch, uint16 w, uint16 h);

	enum {
		SOUND_SAMPLE_RATE = 22050
	};

	SDL_Surface *_screen;
	bool _fullscreen;
	uint16 _pal[256];
	uint16 _screenW, _screenH;

	virtual ~SystemStub_SDL() {}
	virtual void init(const char *title, uint16 w, uint16 h);
	virtual void destroy();
	virtual void setPalette(const Color *colors, int n);
	virtual void copyRect(const uint8 *buf, uint32 pitch);
	virtual void processEvents();
	virtual void sleep(uint32 duration);
	virtual uint32 getTimeStamp();
	virtual void startAudio(AudioCallback callback, void *param);
	virtual void stopAudio();
	virtual uint32 getOutputSampleRate();
	virtual void *addTimer(uint32 delay, TimerCallback callback, void *param);
	virtual void removeTimer(void *timerId);
	virtual void *createMutex();
	virtual void destroyMutex(void *mutex);
	virtual void lockMutex(void *mutex);
	virtual void unlockMutex(void *mutex);

	void prepareGfxMode();
	void cleanupGfxMode();
	void switchGfxMode(bool fullscreen);
};

SystemStub *SystemStub_SDL_create() {
	return new SystemStub_SDL();
}

void SystemStub_SDL::init(const char *title, uint16 w, uint16 h) {
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER);
	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
	SDL_ShowCursor(SDL_DISABLE);
	SDL_WM_SetCaption(title, NULL);
	memset(&pi, 0, sizeof(pi));
	_fullscreen = false;
	_screenW = w;
	_screenH = h;
	prepareGfxMode();
}

void SystemStub_SDL::destroy() {
	cleanupGfxMode();
	SDL_Quit();
}

void SystemStub_SDL::setPalette(const Color *colors, int n) {
	assert(n <= 256);
	for (int i = 0; i < n; ++i) {
		const Color *c = &colors[i];
		uint8 r = (c->r << 2) | (c->r & 3);
		uint8 g = (c->g << 2) | (c->g & 3);
		uint8 b = (c->b << 2) | (c->b & 3);
		_pal[i] = SDL_MapRGB(_screen->format, r, g, b);
	}	
}

void SystemStub_SDL::copyRect(const uint8 *buf, uint32 pitch) {
	SDL_LockSurface(_screen);
	uint16 *p = (uint16 *)_screen->pixels;
	uint16 h = _screenH;
	while (h--) {
		for (int i = 0; i < _screenW; ++i) {
			uint8 col = buf[i];
			p[i] = _pal[col];
		}
		p += _screen->pitch >> 1;
		buf += pitch;
	}
	SDL_UnlockSurface(_screen);
	SDL_UpdateRect(_screen, 0, 0, 0, 0);
}

void SystemStub_SDL::processEvents() {
	SDL_Event ev;
	while (SDL_PollEvent(&ev)) {
		switch (ev.type) {
		case SDL_QUIT:
			pi.quit = true;
			break;
		case SDL_KEYUP:
			switch (ev.key.keysym.sym) {
			case SDLK_LEFT:
				pi.dirMask &= ~PlayerInput::DIR_LEFT;
				break;
			case SDLK_RIGHT:
				pi.dirMask &= ~PlayerInput::DIR_RIGHT;
				break;
			case SDLK_UP:
				pi.dirMask &= ~PlayerInput::DIR_UP;
				break;
			case SDLK_DOWN:
				pi.dirMask &= ~PlayerInput::DIR_DOWN;
				break;
			case SDLK_SPACE:
			case SDLK_RETURN:
				pi.button = false;
				break;
			default:
				break;
			}
			break;
		case SDL_KEYDOWN:
			if (ev.key.keysym.mod & KMOD_ALT) {
				if (ev.key.keysym.sym == SDLK_RETURN) {
					switchGfxMode(!_fullscreen);
				} else if (ev.key.keysym.sym == SDLK_x) {
					pi.quit = true;
				}
				break;
			} else if (ev.key.keysym.mod & KMOD_CTRL) {
				if (ev.key.keysym.sym == SDLK_s) {
					pi.save = true;
				} else if (ev.key.keysym.sym == SDLK_l) {
					pi.load = true;
				} else if (ev.key.keysym.sym == SDLK_KP_PLUS) {
					pi.stateSlot = +1;
				} else if (ev.key.keysym.sym == SDLK_KP_MINUS) {
					pi.stateSlot = -1;
				}
				break;
			}
			pi.lastChar = ev.key.keysym.sym;
			switch (ev.key.keysym.sym) {
			case SDLK_LEFT:
				pi.dirMask |= PlayerInput::DIR_LEFT;
				break;
			case SDLK_RIGHT:
				pi.dirMask |= PlayerInput::DIR_RIGHT;
				break;
			case SDLK_UP:
				pi.dirMask |= PlayerInput::DIR_UP;
				break;
			case SDLK_DOWN:
				pi.dirMask |= PlayerInput::DIR_DOWN;
				break;
			case SDLK_SPACE:
			case SDLK_RETURN:
				pi.button = true;
				break;
			case SDLK_c:
				pi.code = true;
				break;
			case SDLK_p:
				pi.pause = true;
				break;
			default:
				break;
			}
			break;
		default:
			break;
		}
	}
}

void SystemStub_SDL::sleep(uint32 duration) {
	SDL_Delay(duration);
}

uint32 SystemStub_SDL::getTimeStamp() {
	return SDL_GetTicks();
}

void SystemStub_SDL::startAudio(AudioCallback callback, void *param) {
	SDL_AudioSpec desired;
	memset(&desired, 0, sizeof(desired));
	desired.freq = SOUND_SAMPLE_RATE;
	desired.format = AUDIO_S8;
	desired.channels = 1;
	desired.samples = 2048;
	desired.callback = callback;
	desired.userdata = param;
	if (SDL_OpenAudio(&desired, NULL) == 0) {
		SDL_PauseAudio(0);
	} else {
		error("SystemStub_SDL::startAudio() unable to open sound device");
	}
}

void SystemStub_SDL::stopAudio() {
	SDL_CloseAudio();
}

uint32 SystemStub_SDL::getOutputSampleRate() {
	return SOUND_SAMPLE_RATE;
}

void *SystemStub_SDL::addTimer(uint32 delay, TimerCallback callback, void *param) {
	return SDL_AddTimer(delay, (SDL_NewTimerCallback)callback, param);
}

void SystemStub_SDL::removeTimer(void *timerId) {
	SDL_RemoveTimer((SDL_TimerID)timerId);
}

void *SystemStub_SDL::createMutex() {
	return SDL_CreateMutex();
}

void SystemStub_SDL::destroyMutex(void *mutex) {
	SDL_DestroyMutex((SDL_mutex *)mutex);
}

void SystemStub_SDL::lockMutex(void *mutex) {
	SDL_mutexP((SDL_mutex *)mutex);
}

void SystemStub_SDL::unlockMutex(void *mutex) {
	SDL_mutexV((SDL_mutex *)mutex);
}

void SystemStub_SDL::prepareGfxMode() {
	uint32 flags = SDL_HWSURFACE;
	if (_fullscreen) {
		flags |= SDL_FULLSCREEN;
	}
	_screen = SDL_SetVideoMode(_screenW, _screenH, 16, flags);
	if (!_screen) {
		error("SystemStub_SDL::prepareGfxMode() can't switch to %dx%dx16 fullscreen=%d", _screenW, _screenH, _fullscreen);
	}
}

void SystemStub_SDL::cleanupGfxMode() {
	if (_screen) {
		SDL_FreeSurface(_screen);
		_screen = 0;
	}
}

void SystemStub_SDL::switchGfxMode(bool fullscreen) {
	SDL_FreeSurface(_screen);
	_fullscreen = fullscreen;
	prepareGfxMode();
}
