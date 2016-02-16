/* Raw - Another World Interpreter
 * Copyright (C) 2004 Gregory Montoir
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <SDL.h>
#include "systemstub.h"
#include "util.h"

struct SDLStub : SystemStub {
	typedef void (SDLStub::*ScaleProc)(uint16 *dst, uint16 dstPitch, const uint16 *src, uint16 srcPitch, uint16 w, uint16 h);

	enum {
		SCREEN_W = 320,
		SCREEN_H = 240,
		SOUND_SAMPLE_RATE = 22050
	};

	struct Scaler {
		const char *name;
		ScaleProc proc;
		uint8 factor;
	};
	
	static const Scaler _scalers[];

	uint8 *_offscreen;
	SDL_Surface *_screen;
	SDL_Surface *_sclscreen;
	bool _fullscreen;
	uint8 _scaler;
	uint16 _pal[16];

	virtual ~SDLStub() {}
	virtual void init(const char *title);
	virtual void destroy();
	virtual void setPalette(uint8 s, uint8 n, const uint8 *buf);
	virtual void copyRect(uint16 x, uint16 y, uint16 w, uint16 h, const uint8 *buf, uint32 pitch);
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
	void switchGfxMode(bool fullscreen, uint8 scaler);

	void point1x(uint16 *dst, uint16 dstPitch, const uint16 *src, uint16 srcPitch, uint16 w, uint16 h);
	void point2x(uint16 *dst, uint16 dstPitch, const uint16 *src, uint16 srcPitch, uint16 w, uint16 h);
	void point3x(uint16 *dst, uint16 dstPitch, const uint16 *src, uint16 srcPitch, uint16 w, uint16 h);
};

const SDLStub::Scaler SDLStub::_scalers[] = {
	{ "Point1x", &SDLStub::point1x, 1 },
	{ "Point2x", &SDLStub::point2x, 2 },
	{ "Point3x", &SDLStub::point3x, 3 }
};

SystemStub *SystemStub_SDL_create() {
	return new SDLStub();
}

void SDLStub::init(const char *title) {
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER | SDL_INIT_JOYSTICK);
	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
	SDL_ShowCursor(SDL_DISABLE);
	SDL_JoystickOpen(0);
	SDL_WM_SetCaption(title, NULL);
	memset(&_pi, 0, sizeof(_pi));
	_offscreen = (uint8 *)malloc(SCREEN_W * SCREEN_H * 2);
	if (!_offscreen) {
		error("Unable to allocate offscreen buffer");
	}
	_fullscreen = true;
	_scaler = 0;
	prepareGfxMode();
}

void SDLStub::destroy() {
	cleanupGfxMode();
	SDL_Quit();
}

void SDLStub::setPalette(uint8 s, uint8 n, const uint8 *buf) {
	assert(s + n <= 16);
	for (int i = s; i < s + n; ++i) {
		uint8 c[3];
		for (int j = 0; j < 3; ++j) {
			uint8 col = buf[i * 3 + j];
			c[j] =  (col << 2) | (col & 3);
		}
		_pal[i] = SDL_MapRGB(_screen->format, c[0], c[1], c[2]);
	}	
}

void SDLStub::copyRect(uint16 x, uint16 y, uint16 w, uint16 h, const uint8 *buf, uint32 pitch) {
	buf += y * pitch + x;
	uint16 *p = (uint16 *)_offscreen;
	while (h--) {
		for (int i = 0; i < w / 2; ++i) {
			uint8 p1 = *(buf + i) >> 4;
			uint8 p2 = *(buf + i) & 0xF;
			*(p + i * 2 + 0) = _pal[p1];
			*(p + i * 2 + 1) = _pal[p2];
		}
		p += SCREEN_W;
		buf += pitch;
	}
	SDL_LockSurface(_sclscreen);
	(this->*_scalers[_scaler].proc)((uint16 *)_sclscreen->pixels, _sclscreen->pitch, (uint16 *)_offscreen, SCREEN_W, SCREEN_W, SCREEN_H);
	SDL_UnlockSurface(_sclscreen);
	SDL_Rect dest;
	dest.x = 0;
	dest.y = 20;
	SDL_BlitSurface(_sclscreen, NULL, _screen, &dest);
	SDL_UpdateRect(_screen, 0, 0, 0, 0);
}

void SDLStub::processEvents() {
	SDL_Event ev;
	while(SDL_PollEvent(&ev)) {
		switch (ev.type) {
		case SDL_QUIT:
			_pi.quit = true;
			break;
		case SDL_JOYBUTTONUP:
			switch(ev.jbutton.button) {
			case GP2X_BUTTON_R:
				_pi.button_R = false;
				_pi.dirMask &= ~PlayerInput::DIR_UP;
				break;
			case GP2X_BUTTON_L:
				_pi.button_L = false;
				break;
			case GP2X_BUTTON_UPLEFT:
				_pi.dirMask &= ~PlayerInput::DIR_LEFT;
				//_pi.dirMask &= ~PlayerInput::DIR_UP;
				break;			
			case GP2X_BUTTON_UPRIGHT:
				//_pi.dirMask &= ~PlayerInput::DIR_UP;
				_pi.dirMask &= ~PlayerInput::DIR_RIGHT;
				break;		
			case GP2X_BUTTON_DOWNLEFT:
				_pi.dirMask &= ~PlayerInput::DIR_LEFT;
				_pi.dirMask &= ~PlayerInput::DIR_DOWN;
				break;					
			case GP2X_BUTTON_DOWNRIGHT:
				_pi.dirMask &= ~PlayerInput::DIR_RIGHT;
				_pi.dirMask &= ~PlayerInput::DIR_DOWN;
				break;
			case GP2X_BUTTON_LEFT:
				_pi.dirMask &= ~PlayerInput::DIR_LEFT;
				break;
			case GP2X_BUTTON_RIGHT:
				_pi.dirMask &= ~PlayerInput::DIR_RIGHT;
				break;
			case GP2X_BUTTON_UP:
				_pi.dirMask &= ~PlayerInput::DIR_UP;
				break;
			case GP2X_BUTTON_DOWN:
				_pi.dirMask &= ~PlayerInput::DIR_DOWN;
				break;
			case GP2X_BUTTON_A:
				_pi.button = false;
				break;
			default:
				break;
			}
			break;
		case SDL_JOYBUTTONDOWN:
			switch(ev.jbutton.button) {
			case GP2X_BUTTON_R:
				_pi.button_R = true;
				_pi.dirMask |= PlayerInput::DIR_UP;
				break;
			case GP2X_BUTTON_L:
				_pi.button_L = true;
				break;
			case GP2X_BUTTON_UPLEFT:
				_pi.dirMask |= PlayerInput::DIR_LEFT;
				//_pi.dirMask |= PlayerInput::DIR_UP;
				break;			
			case GP2X_BUTTON_UPRIGHT:
				_pi.dirMask |= PlayerInput::DIR_RIGHT;
				//_pi.dirMask |= PlayerInput::DIR_UP;
				break;		
			case GP2X_BUTTON_DOWNLEFT:
				_pi.dirMask |= PlayerInput::DIR_LEFT;
				_pi.dirMask |= PlayerInput::DIR_DOWN;
				break;					
			case GP2X_BUTTON_DOWNRIGHT:
				_pi.dirMask |= PlayerInput::DIR_RIGHT;
				_pi.dirMask |= PlayerInput::DIR_DOWN;
				break;
			case GP2X_BUTTON_LEFT:
				_pi.dirMask |= PlayerInput::DIR_LEFT;
				break;
			case GP2X_BUTTON_RIGHT:
				_pi.dirMask |= PlayerInput::DIR_RIGHT;
				break;
			case GP2X_BUTTON_UP:
				_pi.dirMask |= PlayerInput::DIR_UP;
				break;
			case GP2X_BUTTON_DOWN:
				_pi.dirMask |= PlayerInput::DIR_DOWN;
				break;
			case GP2X_BUTTON_A:
				_pi.button = true;
				break;
			/*	
			case GP2X_BUTTON_B:
				_pi.stateSlot = 1; //Next save state slot
				break;
			*/
			case GP2X_BUTTON_Y:
				_pi.save = true;  //Save State
				break;
			case GP2X_BUTTON_X:
				_pi.load = true;  //Load State
				break;
			case GP2X_BUTTON_SELECT:
				_pi.code = true;
				break;
			case GP2X_BUTTON_START:
				_pi.pause = true;
				break;
			case GP2X_BUTTON_VOLUP:
				_pi.volumeUp = true;
				break;
			case GP2X_BUTTON_VOLDOWN:
				_pi.volumeDown = true;
				break;
			default:
				break;
			}
			break;
		default:
			break;
		}
	}
	if (_pi.button_R && _pi.button_L) {
		_pi.quit = true;
	}
}

void SDLStub::sleep(uint32 duration) {
	SDL_Delay(duration);
}

uint32 SDLStub::getTimeStamp() {
	return SDL_GetTicks();	
}

void SDLStub::startAudio(AudioCallback callback, void *param) {
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
		printf("SDLStub::startAudio() unable to open sound device\n");
	}
}

void SDLStub::stopAudio() {
	SDL_CloseAudio();
}

uint32 SDLStub::getOutputSampleRate() {
	return SOUND_SAMPLE_RATE;
}

void *SDLStub::addTimer(uint32 delay, TimerCallback callback, void *param) {
	return SDL_AddTimer(delay, (SDL_NewTimerCallback)callback, param);
}

void SDLStub::removeTimer(void *timerId) {
	SDL_RemoveTimer((SDL_TimerID)timerId);
}

void *SDLStub::createMutex() {
	return SDL_CreateMutex();
}

void SDLStub::destroyMutex(void *mutex) {
	SDL_DestroyMutex((SDL_mutex *)mutex);
}

void SDLStub::lockMutex(void *mutex) {
	SDL_mutexP((SDL_mutex *)mutex);
}

void SDLStub::unlockMutex(void *mutex) {
	SDL_mutexV((SDL_mutex *)mutex);
}

void SDLStub::prepareGfxMode() {
	int w = SCREEN_W * _scalers[_scaler].factor;
	int h = SCREEN_H * _scalers[_scaler].factor;

	_screen = SDL_SetVideoMode(w, h, 16, VIDEO_MODE);

	if (!_screen) {
		error("SDLStub::prepareGfxMode() unable to allocate _screen buffer");
	}
	_sclscreen = SDL_CreateRGBSurface(SDL_SWSURFACE, w, h, 16,
						_screen->format->Rmask,
						_screen->format->Gmask,
						_screen->format->Bmask,
						_screen->format->Amask);
	if (!_sclscreen) {
		error("SDLStub::prepareGfxMode() unable to allocate _sclscreen buffer");
	}
}

void SDLStub::cleanupGfxMode() {
	if (_offscreen) {
		free(_offscreen);
		_offscreen = 0;
	}
	if (_sclscreen) {
		SDL_FreeSurface(_sclscreen);
		_sclscreen = 0;
	}
	if (_screen) {
		SDL_FreeSurface(_screen);
		_screen = 0;
	}
}

void SDLStub::switchGfxMode(bool fullscreen, uint8 scaler) {
	SDL_Surface *prev_sclscreen = _sclscreen;
	SDL_FreeSurface(_screen); 	
	_fullscreen = fullscreen;
	_scaler = scaler;
	prepareGfxMode();
	SDL_BlitSurface(prev_sclscreen, NULL, _sclscreen, NULL);
	SDL_FreeSurface(prev_sclscreen);
}

void SDLStub::point1x(uint16 *dst, uint16 dstPitch, const uint16 *src, uint16 srcPitch, uint16 w, uint16 h) {
	dstPitch >>= 1;
	while (h--) {
		memcpy(dst, src, w * 2);
		dst += dstPitch;
		src += dstPitch;
	}
}

void SDLStub::point2x(uint16 *dst, uint16 dstPitch, const uint16 *src, uint16 srcPitch, uint16 w, uint16 h) {
	dstPitch >>= 1;
	while (h--) {
		uint16 *p = dst;
		for (int i = 0; i < w; ++i, p += 2) {
			uint16 c = *(src + i);
			*(p + 0) = c;
			*(p + 1) = c;
			*(p + 0 + dstPitch) = c;
			*(p + 1 + dstPitch) = c;
		}
		dst += dstPitch * 2;
		src += srcPitch;
	}
}

void SDLStub::point3x(uint16 *dst, uint16 dstPitch, const uint16 *src, uint16 srcPitch, uint16 w, uint16 h) {
	dstPitch >>= 1;
	while (h--) {
		uint16 *p = dst;
		for (int i = 0; i < w; ++i, p += 3) {
			uint16 c = *(src + i);
			*(p + 0) = c;
			*(p + 1) = c;
			*(p + 2) = c;
			*(p + 0 + dstPitch) = c;
			*(p + 1 + dstPitch) = c;
			*(p + 2 + dstPitch) = c;
			*(p + 0 + dstPitch * 2) = c;
			*(p + 1 + dstPitch * 2) = c;
			*(p + 2 + dstPitch * 2) = c;
		}
		dst += dstPitch * 3;
		src += srcPitch;
	}
}
