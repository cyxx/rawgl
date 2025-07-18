
/*
 * Another World engine rewrite
 * Copyright (C) 2004-2005 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include <SDL.h>
#include <getopt.h>
#include <sys/stat.h>
#include "engine.h"
#include "graphics.h"
#include "resource.h"
#include "systemstub.h"
#include "util.h"


static const char USAGE[] =
	"raw(gl) - Another World Interpreter\n"
	"Usage: %s [OPTIONS]...\n"
	"  --datapath=PATH   Path to data files (default '.')\n"
	"  --language=LANG   Language (fr,us,de,es,it)\n"
	"  --part=NUM        Game part to start from (0-35 or 16001-16009)\n"
	"  --render=NAME     Renderer (original,software,gl)\n"
	"  --window=WxH      Windowed display size (default '640x400')\n"
	"  --fullscreen      Fullscreen display (stretched)\n"
	"  --fullscreen-ar   Fullscreen display (16:10 aspect ratio)\n"
	"  --ega-palette     Use EGA palette with DOS version\n"
	"  --demo3-joy       Use inputs from 'demo3.joy' (DOS demo)\n"
	"  --difficulty=DIFF Difficulty (easy,normal,hard)\n"
	"  --audio=AUDIO     Audio (original,remastered)\n"
	"  --mt32            Use MT32 sounds mapping with DOS version\n"
	;

static const struct {
	const char *name;
	int lang;
} LANGUAGES[] = {
	{ "fr", LANG_FR },
	{ "us", LANG_US },
	{ "de", LANG_DE },
	{ "es", LANG_ES },
	{ "it", LANG_IT },
	{ 0, -1 }
};

static const struct {
	const char *name;
	int type;
} GRAPHICS[] = {
	{ "original", GRAPHICS_ORIGINAL },
	{ "software", GRAPHICS_SOFTWARE },
	{ "gl", GRAPHICS_GL },
	{ 0,  -1 }
};

static const struct {
	const char *name;
	int difficulty;
} DIFFICULTIES[] = {
	{ "easy", DIFFICULTY_EASY },
	{ "normal", DIFFICULTY_NORMAL },
	{ "hard", DIFFICULTY_HARD },
	{ 0,  -1 }
};

bool Graphics::_is1991 = false;
bool Graphics::_use555 = false;
bool Video::_useEGA = false;
Difficulty Script::_difficulty = DIFFICULTY_NORMAL;
bool Script::_useRemasteredAudio = true;

static Graphics *createGraphics(int type) {
	switch (type) {
	case GRAPHICS_ORIGINAL:
		Graphics::_is1991 = true;
		// fall-through
	case GRAPHICS_SOFTWARE:
		debug(DBG_INFO, "Using software graphics");
		return GraphicsSoft_create();
	case GRAPHICS_GL:
		debug(DBG_INFO, "Using GL graphics");
#ifdef USE_GL
		return GraphicsGL_create();
#endif
	}
	return 0;
}

static int getGraphicsType(Resource::DataType type) {
	switch (type) {
	case Resource::DT_15TH_EDITION:
	case Resource::DT_20TH_EDITION:
		return GRAPHICS_GL;
	default:
		return GRAPHICS_ORIGINAL;
	}
}

struct Scaler {
	char name[32];
	int factor;
};

static void parseScaler(char *name, Scaler *s) {
	char *sep = strchr(name, '@');
	if (sep) {
		*sep = 0;
		strncpy(s->name, name, sizeof(s->name) - 1);
		s->name[sizeof(s->name) - 1] = 0;
	}
	if (sep) {
		s->factor = atoi(sep + 1);
	}
}

static const int DEFAULT_WINDOW_W = 640;
static const int DEFAULT_WINDOW_H = 400;

int main(int argc, char *argv[]) {
	char *dataPath = 0;
	int part = 16001;
	Language lang = LANG_FR;
	int graphicsType = GRAPHICS_GL;
	DisplayMode dm;
	dm.mode   = DisplayMode::WINDOWED;
	dm.width  = DEFAULT_WINDOW_W;
	dm.height = DEFAULT_WINDOW_H;
	dm.opengl = (graphicsType == GRAPHICS_GL);
	Scaler scaler;
	scaler.name[0] = 0;
	scaler.factor = 1;
	bool defaultGraphics = true;
	bool demo3JoyInputs = false;
	bool useMT32 = false;
	if (argc == 2) {
		// data path as the only command line argument
		struct stat st;
		if (stat(argv[1], &st) == 0 && S_ISDIR(st.st_mode)) {
			dataPath = strdup(argv[1]);
		}
	}
	while (1) {
		static struct option options[] = {
			{ "datapath", required_argument, 0, 'd' },
			{ "language", required_argument, 0, 'l' },
			{ "part",     required_argument, 0, 'p' },
			{ "render",   required_argument, 0, 'r' },
			{ "window",   required_argument, 0, 'w' },
			{ "fullscreen", no_argument,     0, 'f' },
			{ "fullscreen-ar", no_argument,  0, 'a' },
			{ "scaler",   required_argument, 0, 's' },
			{ "ega-palette", no_argument,    0, 'e' },
			{ "demo3-joy",  no_argument,     0, 'j' },
			{ "difficulty", required_argument, 0, 'i' },
			{ "audio",    required_argument, 0, 'u' },
			{ "mt32",       no_argument,     0, 'm' },
			{ "help",       no_argument,     0, 'h' },
			{ 0, 0, 0, 0 }
		};
		int index;
		const int c = getopt_long(argc, argv, "", options, &index);
		if (c == -1) {
			break;
		}
		switch (c) {
		case 'd':
			dataPath = strdup(optarg);
			break;
		case 'l':
			for (int i = 0; LANGUAGES[i].name; ++i) {
				if (strcmp(optarg, LANGUAGES[i].name) == 0) {
					lang = (Language)LANGUAGES[i].lang;
					break;
				}
			}
			break;
		case 'p':
			part = atoi(optarg);
			break;
		case 'r':
			for (int i = 0; GRAPHICS[i].name; ++i) {
				if (strcmp(optarg, GRAPHICS[i].name) == 0) {
					graphicsType = GRAPHICS[i].type;
					dm.opengl = (graphicsType == GRAPHICS_GL);
					defaultGraphics = false;
					break;
				}
			}
			break;
		case 'w':
			sscanf(optarg, "%dx%d", &dm.width, &dm.height);
			break;
		case 'f':
			dm.mode = DisplayMode::FULLSCREEN;
			break;
		case 'a':
			dm.mode = DisplayMode::FULLSCREEN_AR;
			break;
		case 's':
			parseScaler(optarg, &scaler);
			break;
		case 'e':
			Video::_useEGA = true;
			break;
		case 'j':
			demo3JoyInputs = true;
			break;
		case 'i':
			for (int i = 0; DIFFICULTIES[i].name; ++i) {
				if (strcmp(optarg, DIFFICULTIES[i].name) == 0) {
					Script::_difficulty = (Difficulty)DIFFICULTIES[i].difficulty;
					break;
				}
			}
			break;
		case 'u':
			if (strcmp(optarg, "remastered") == 0) {
				Script::_useRemasteredAudio = true;
			} else if (strcmp(optarg, "original") == 0) {
				Script::_useRemasteredAudio = false;
			}
			break;
		case 'm':
			useMT32 = true;
			break;
		case 'h':
			// fall-through
		default:
			printf(USAGE, argv[0]);
			return 0;
		}
	}
	g_debugMask = DBG_INFO; // | DBG_VIDEO | DBG_SND | DBG_SCRIPT | DBG_BANK | DBG_SER;
	Engine *e = new Engine(dataPath, part);
	if (defaultGraphics) {
		// if not set, use original software graphics for 199x and 3DO versions and GL for the anniversary releases
		graphicsType = getGraphicsType(e->_res.getDataType());
		dm.opengl = (graphicsType == GRAPHICS_GL);
	}
	if (graphicsType != GRAPHICS_GL && e->_res.getDataType() == Resource::DT_3DO) {
		graphicsType = GRAPHICS_SOFTWARE;
		Graphics::_use555 = true;
	}
	Graphics *graphics = createGraphics(graphicsType);
	if (e->_res.getDataType() == Resource::DT_20TH_EDITION) {
		switch (Script::_difficulty) {
		case DIFFICULTY_EASY:
			debug(DBG_INFO, "Using easy difficulty");
			break;
		case DIFFICULTY_NORMAL:
			debug(DBG_INFO, "Using normal difficulty");
			break;
		case DIFFICULTY_HARD:
			debug(DBG_INFO, "Using hard difficulty");
			break;
		}
	}
	if (e->_res.getDataType() == Resource::DT_15TH_EDITION || e->_res.getDataType() == Resource::DT_20TH_EDITION) {
		if (Script::_useRemasteredAudio) {
			debug(DBG_INFO, "Using remastered audio");
		} else {
			debug(DBG_INFO, "Using original audio");
		}
	}
	SystemStub *stub = SystemStub_SDL_create();
	stub->init(e->getGameTitle(lang), &dm);
	e->setSystemStub(stub, graphics);
	if (demo3JoyInputs && e->_res.getDataType() == Resource::DT_DOS) {
		e->_res.readDemo3Joy();
	}
	e->setup(lang, graphicsType, scaler.name, scaler.factor, useMT32);
	while (!stub->_pi.quit) {
		e->run();
	}
	e->finish();
	delete e;
	delete graphics;
	stub->fini();
	delete stub;
	return 0;
}
