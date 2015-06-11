
/*
 * Another World engine rewrite
 * Copyright (C) 2004-2005 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include <SDL.h>
#include "graphics.h"
#include "systemstub.h"
#include "util.h"

static const int DEF_SCREEN_W = 800;
static const int DEF_SCREEN_H = 600;

struct SystemStub_OGL : SystemStub {
	int _w, _h;
	bool _fullscreen;

	SystemStub_OGL() {}
	virtual ~SystemStub_OGL() {}

	virtual void init(const char *title);
	virtual void fini();

	virtual void getScreenSize(int &w, int &h) const { w = _w; h = _h; }
	virtual void updateScreen();

	virtual void processEvents();
	virtual void sleep(uint32_t duration);
	virtual uint32_t getTimeStamp();

	void resize(int w, int h);
	void switchGfxMode(bool fullscreen);
};

SystemStub *SystemStub_OGL_create() {
	return new SystemStub_OGL();
}

void SystemStub_OGL::init(const char *title) {
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
	SDL_ShowCursor(SDL_DISABLE);
	SDL_WM_SetCaption(title, NULL);
	memset(&_pi, 0, sizeof(_pi));
	_fullscreen = false;
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	resize(DEF_SCREEN_W, DEF_SCREEN_H);
}

void SystemStub_OGL::fini() {
	SDL_Quit();
}

void SystemStub_OGL::updateScreen() {
	SDL_GL_SwapBuffers();
}

void SystemStub_OGL::processEvents() {
	SDL_Event ev;
	while(SDL_PollEvent(&ev)) {
		switch (ev.type) {
		case SDL_QUIT:
			_pi.quit = true;
			break;
		case SDL_VIDEORESIZE:
			resize(ev.resize.w, ev.resize.h);
			break;			
		case SDL_KEYUP:
			switch(ev.key.keysym.sym) {
			case SDLK_LEFT:
				_pi.dirMask &= ~PlayerInput::DIR_LEFT;
				break;
			case SDLK_RIGHT:
				_pi.dirMask &= ~PlayerInput::DIR_RIGHT;
				break;
			case SDLK_UP:
				_pi.dirMask &= ~PlayerInput::DIR_UP;
				break;
			case SDLK_DOWN:
				_pi.dirMask &= ~PlayerInput::DIR_DOWN;
				break;
			case SDLK_SPACE:
			case SDLK_RETURN:
				_pi.button = false;
				break;
			default:
				break;
			}
			break;
		case SDL_KEYDOWN:
			if (ev.key.keysym.mod & KMOD_ALT) {
				if (ev.key.keysym.sym == SDLK_RETURN || ev.key.keysym.sym == SDLK_KP_ENTER) {
					switchGfxMode(!_fullscreen);
				}
				break;
			} else if (ev.key.keysym.mod & KMOD_CTRL) {
				if (ev.key.keysym.sym == SDLK_f) {
					_pi.fastMode = true;
				}
				break;
			}
			_pi.lastChar = ev.key.keysym.sym;
			switch(ev.key.keysym.sym) {
			case SDLK_LEFT:
				_pi.dirMask |= PlayerInput::DIR_LEFT;
				break;
			case SDLK_RIGHT:
				_pi.dirMask |= PlayerInput::DIR_RIGHT;
				break;
			case SDLK_UP:
				_pi.dirMask |= PlayerInput::DIR_UP;
				break;
			case SDLK_DOWN:
				_pi.dirMask |= PlayerInput::DIR_DOWN;
				break;
			case SDLK_SPACE:
			case SDLK_RETURN:
				_pi.button = true;
				break;
			case SDLK_c:
				_pi.code = true;
				break;
			case SDLK_p:
				_pi.pause = true;
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

void SystemStub_OGL::sleep(uint32_t duration) {
	SDL_Delay(duration);
}

uint32_t SystemStub_OGL::getTimeStamp() {
	return SDL_GetTicks();
}

void SystemStub_OGL::resize(int w, int h) {
	_w = w;
	_h = h;
	SDL_SetVideoMode(_w, _h, 0, SDL_OPENGL | (_fullscreen ? SDL_FULLSCREEN : SDL_RESIZABLE));
}

void SystemStub_OGL::switchGfxMode(bool fullscreen) {
	_fullscreen = fullscreen;
	const int flags = _fullscreen ? SDL_FULLSCREEN : SDL_RESIZABLE;
	SDL_SetVideoMode(_w, _h, 0, SDL_OPENGL | flags);
}
