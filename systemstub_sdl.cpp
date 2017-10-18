
/*
 * Another World engine rewrite
 * Copyright (C) 2004-2005 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include <SDL.h>
#include "graphics.h"
#include "systemstub.h"
#include "util.h"

struct SystemStub_SDL : SystemStub {

	static const int kJoystickIndex = 0;
	static const int kJoystickCommitValue = 16384;
	static const float kAspectRatio;

	int _w, _h;
	float _aspectRatio[4];
	SDL_Window *_window;
	SDL_Renderer *_renderer;
	SDL_GLContext _glcontext;
	int _texW, _texH;
	SDL_Texture *_texture;
	SDL_Joystick *_joystick;
	SDL_GameController *_controller;

	SystemStub_SDL();
	virtual ~SystemStub_SDL() {}

	virtual void init(const char *title, const DisplayMode *dm);
	virtual void fini();

	virtual void prepareScreen(int &w, int &h, float ar[4]);
	virtual void updateScreen();
	virtual void setScreenPixels565(const uint16_t *data, int w, int h);

	virtual void processEvents();
	virtual void sleep(uint32_t duration);
	virtual uint32_t getTimeStamp();

	void setAspectRatio(int w, int h);
};

const float SystemStub_SDL::kAspectRatio = 4 / 3.;

SystemStub_SDL::SystemStub_SDL()
	: _w(0), _h(0), _window(0), _renderer(0), _texW(0), _texH(0), _texture(0) {
}

void SystemStub_SDL::init(const char *title, const DisplayMode *dm) {
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER);
	SDL_ShowCursor(SDL_DISABLE);

	int windowW = 0;
	int windowH = 0;
	int flags = dm->opengl ? SDL_WINDOW_OPENGL : 0;
	if (dm->mode == DisplayMode::WINDOWED) {
		flags |= SDL_WINDOW_RESIZABLE;
		windowW = dm->width;
		windowH = dm->height;
	} else {
		flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
	}
	_window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowW, windowH, flags);
	SDL_GetWindowSize(_window, &_w, &_h);

	if (dm->opengl) {
		_glcontext = SDL_GL_CreateContext(_window);
	} else {
		_renderer = SDL_CreateRenderer(_window, -1, SDL_RENDERER_ACCELERATED);
		SDL_SetRenderDrawColor(_renderer, 0, 0, 0, 255);
		SDL_RenderClear(_renderer);
	}
	if (dm->mode == DisplayMode::FULLSCREEN_AR) {
		setAspectRatio(_w, _h);
		if (!dm->opengl) {
			SDL_RenderSetLogicalSize(_renderer, 320, 200);
		}
	} else {
		_aspectRatio[0] = _aspectRatio[1] = 0.;
		_aspectRatio[2] = _aspectRatio[3] = 1.;
	}
	_joystick = 0;
	_controller = 0;
	if (SDL_NumJoysticks() > 0) {
		SDL_GameControllerAddMappingsFromFile("gamecontrollerdb.txt");
		if (SDL_IsGameController(kJoystickIndex)) {
			_controller = SDL_GameControllerOpen(kJoystickIndex);
		}
		if (!_controller) {
			_joystick = SDL_JoystickOpen(kJoystickIndex);
		}
	}
}

void SystemStub_SDL::fini() {
	if (_texture) {
		SDL_DestroyTexture(_texture);
	}
	if (_joystick) {
		SDL_JoystickClose(_joystick);
		_joystick = 0;
	}
	if (_controller) {
		SDL_GameControllerClose(_controller);
		_controller = 0;
	}
	if (_renderer) {
		SDL_DestroyRenderer(_renderer);
		_renderer = 0;
	}
	SDL_GL_DeleteContext(_glcontext);
	SDL_DestroyWindow(_window);
	SDL_Quit();
}

void SystemStub_SDL::prepareScreen(int &w, int &h, float ar[4]) {
	w = _w;
	h = _h;
	ar[0] = _aspectRatio[0];
	ar[1] = _aspectRatio[1];
	ar[2] = _aspectRatio[2];
	ar[3] = _aspectRatio[3];
}

void SystemStub_SDL::updateScreen() {
	if (_renderer) {
		SDL_RenderPresent(_renderer);
	} else {
		SDL_GL_SwapWindow(_window);
	}
}

void SystemStub_SDL::setScreenPixels565(const uint16_t *data, int w, int h) {
	if (_renderer) {
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
		case SDL_JOYHATMOTION:
			if (_joystick) {
				_pi.dirMask = 0;
				if (ev.jhat.value & SDL_HAT_UP) {
					_pi.dirMask |= PlayerInput::DIR_UP;
				}
				if (ev.jhat.value & SDL_HAT_DOWN) {
					_pi.dirMask |= PlayerInput::DIR_DOWN;
				}
				if (ev.jhat.value & SDL_HAT_LEFT) {
					_pi.dirMask |= PlayerInput::DIR_LEFT;
				}
				if (ev.jhat.value & SDL_HAT_RIGHT) {
					_pi.dirMask |= PlayerInput::DIR_RIGHT;
				}
			}
			break;
		case SDL_JOYAXISMOTION:
			if (_joystick) {
				switch (ev.jaxis.axis) {
				case 0:
					_pi.dirMask &= ~(PlayerInput::DIR_RIGHT | PlayerInput::DIR_LEFT);
					if (ev.jaxis.value > kJoystickCommitValue) {
						_pi.dirMask |= PlayerInput::DIR_RIGHT;
					} else if (ev.jaxis.value < -kJoystickCommitValue) {
						_pi.dirMask |= PlayerInput::DIR_LEFT;
					}
					break;
				case 1:
					_pi.dirMask &= ~(PlayerInput::DIR_UP | PlayerInput::DIR_DOWN);
					if (ev.jaxis.value > kJoystickCommitValue) {
						_pi.dirMask |= PlayerInput::DIR_DOWN;
					} else if (ev.jaxis.value < -kJoystickCommitValue) {
						_pi.dirMask |= PlayerInput::DIR_UP;
					}
					break;
				}
			}
			break;
		case SDL_JOYBUTTONDOWN:
		case SDL_JOYBUTTONUP:
			if (_joystick) {
				_pi.button = (ev.jbutton.state == SDL_PRESSED);
			}
			break;
		case SDL_CONTROLLERAXISMOTION:
			if (_controller) {
				switch (ev.caxis.axis) {
				case SDL_CONTROLLER_AXIS_LEFTX:
				case SDL_CONTROLLER_AXIS_RIGHTX:
					if (ev.caxis.value < -kJoystickCommitValue) {
						_pi.dirMask |= PlayerInput::DIR_LEFT;
					} else {
						_pi.dirMask &= ~PlayerInput::DIR_LEFT;
					}
					if (ev.caxis.value > kJoystickCommitValue) {
						_pi.dirMask |= PlayerInput::DIR_RIGHT;
					} else {
						_pi.dirMask &= ~PlayerInput::DIR_RIGHT;
					}
					break;
				case SDL_CONTROLLER_AXIS_LEFTY:
				case SDL_CONTROLLER_AXIS_RIGHTY:
					if (ev.caxis.value < -kJoystickCommitValue) {
						_pi.dirMask |= PlayerInput::DIR_UP;
					} else {
						_pi.dirMask &= ~PlayerInput::DIR_UP;
					}
					if (ev.caxis.value > kJoystickCommitValue) {
						_pi.dirMask |= PlayerInput::DIR_DOWN;
					} else {
						_pi.dirMask &= ~PlayerInput::DIR_DOWN;
					}
					break;
				}
			}
			break;
		case SDL_CONTROLLERBUTTONDOWN:
		case SDL_CONTROLLERBUTTONUP:
			if (_controller) {
				_pi.button = (ev.jbutton.state == SDL_PRESSED);
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

void SystemStub_SDL::setAspectRatio(int w, int h) {
	const float currentAspectRatio = w / (float)h;
	if (int(currentAspectRatio * 100) == int(kAspectRatio * 100)) {
		_aspectRatio[0] = 0.;
		_aspectRatio[1] = 0.;
		_aspectRatio[2] = 1.;
		_aspectRatio[3] = 1.;
		return;
	}
	// pillar box
	if (currentAspectRatio > kAspectRatio) {
		const float inset = 1. - kAspectRatio / currentAspectRatio;
		_aspectRatio[0] = inset / 2;
		_aspectRatio[1] = 0.;
		_aspectRatio[2] = 1. - inset;
		_aspectRatio[3] = 1.;
		return;
	}
	// letter box
	if (currentAspectRatio < kAspectRatio) {
		const float inset = 1. - currentAspectRatio / kAspectRatio;
		_aspectRatio[0] = 0.;
		_aspectRatio[1] = inset / 2;
		_aspectRatio[2] = 1.;
		_aspectRatio[3] = 1. - inset;
		return;
	}
}

SystemStub *SystemStub_SDL_create() {
	return new SystemStub_SDL();
}
