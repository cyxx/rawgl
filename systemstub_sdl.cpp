
/*
 * Another World engine rewrite
 * Copyright (C) 2004-2005 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include <SDL.h>
#include "graphics.h"
#include "systemstub.h"
#include "util.h"

struct SystemStub_SDL : SystemStub {
	int _w, _h;
	SDL_Window *_window;
	SDL_Renderer *_renderer;
	int _texW, _texH;
	SDL_Texture *_texture;

	SystemStub_SDL();
	virtual ~SystemStub_SDL() {}

	virtual void init(int windowW, int windowH, const char *title);
	virtual void fini();

	virtual void prepareScreen(int &w, int &h) const { w = _w; h = _h; }
	virtual void updateScreen();
	virtual void setScreenPixels565(const uint16_t *data, int w, int h);

	virtual void processEvents();
	virtual void sleep(uint32_t duration);
	virtual uint32_t getTimeStamp();
};

SystemStub_SDL::SystemStub_SDL()
	: _w(0), _h(0), _window(0), _renderer(0), _texW(0), _texH(0), _texture(0) {
}

void SystemStub_SDL::init(int windowW, int windowH, const char *title) {
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
	SDL_ShowCursor(SDL_DISABLE);
	_window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowW, windowH, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
	_renderer = SDL_CreateRenderer(_window, -1, SDL_RENDERER_ACCELERATED);
	SDL_GetWindowSize(_window, &_w, &_h);
}

void SystemStub_SDL::fini() {
	if (_texture) {
		SDL_DestroyTexture(_texture);
	}
	SDL_DestroyRenderer(_renderer);
	SDL_DestroyWindow(_window);
	SDL_Quit();
}

void SystemStub_SDL::updateScreen() {
	SDL_RenderPresent(_renderer);
}

void SystemStub_SDL::setScreenPixels565(const uint16_t *data, int w, int h) {
	if (_texW != w || _texH != h) {
		if (_texture) {
			SDL_DestroyTexture(_texture);
			_texture = 0;
		}
	}
	if (!_texture) {
		_texture = SDL_CreateTexture(_renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, w, h);
		if (!_texture) {
			return;
		}
		_texW = w;
		_texH = h;
	}
	SDL_UpdateTexture(_texture, 0, data, w * sizeof(uint16_t));
	SDL_RenderCopy(_renderer, _texture, 0, 0);
}

void SystemStub_SDL::processEvents() {
	SDL_Event ev;
	while(SDL_PollEvent(&ev)) {
		switch (ev.type) {
		case SDL_QUIT:
			_pi.quit = true;
			break;
		case SDL_WINDOWEVENT:
			if (ev.window.event == SDL_WINDOWEVENT_RESIZED) {
				_w = ev.window.data1;
				_h = ev.window.data2;
			} else if (ev.window.event == SDL_WINDOWEVENT_CLOSE) {
				_pi.quit = true;
			}
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
				} else if (ev.key.keysym.sym == SDLK_x) {
					_pi.quit = true;
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

void SystemStub_SDL::sleep(uint32_t duration) {
	SDL_Delay(duration);
}

uint32_t SystemStub_SDL::getTimeStamp() {
	return SDL_GetTicks();
}

SystemStub *SystemStub_SDL_create() {
	return new SystemStub_SDL();
}
