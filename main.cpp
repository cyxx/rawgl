
/*
 * Another World engine rewrite
 * Copyright (C) 2004-2005 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include <getopt.h>
#include "engine.h"
#include "systemstub.h"
#include "util.h"


static const char *USAGE = 
	"Raw - Another World Interpreter\n"
	"Usage: raw [OPTIONS]...\n"
	"  --datapath=PATH   Path to where the game is installed (default '.')\n"
	"  --language=LANG   Language of the game to use (fr,us)\n"
	"  --part=NUM        Starts at specific game part (0-35 or 16001-16009)\n"
	"  --render=NAME     Renderer to use (original,gl)\n"
	;

static const struct {
	const char *name;
	Language lang;
} LANGUAGES[] = {
	{ "fr", LANG_FR  },
	{ "us", LANG_US  },
};

#undef main
int main(int argc, char *argv[]) {
	const char *dataPath = ".";
	const char *language = 0;
	int part = 16001;
	Language lang = LANG_FR;
	const char *render = "gl";
	while (1) {
		static struct option options[] = {
			{ "datapath", required_argument, 0, 'd' },
			{ "language", required_argument, 0, 'l' },
			{ "part",     required_argument, 0, 'p' },
			{ "render",   required_argument, 0, 'r' },
			{ 0, 0, 0, 0 }
		};
		int index;
		const int c = getopt_long(argc, argv, "", options, &index);
		if (c == -1) {
			break;
		}
		switch (c) {
		case 'd':
			dataPath = optarg;
			break;
		case 'l':
			language = optarg;
			break;
		case 'p':
			part = atoi(optarg);
			break;
		case 'r':
			render = optarg;
			break;
		default:
			printf("%s\n", USAGE);
			return 0;
		}
	}
	if (language) {
		for (unsigned int j = 0; j < ARRAYSIZE(LANGUAGES); ++j) {
			if (strcmp(language, LANGUAGES[j].name) == 0) {
				lang = LANGUAGES[j].lang;
				break;
			}
		}
	}
	if (render) {
		extern void setRender(const char *);
		setRender(render);
	}
	g_debugMask = DBG_INFO; // | DBG_VIDEO | DBG_SND | DBG_SCRIPT | DBG_BANK | DBG_SER;
	SystemStub *stub = SystemStub_OGL_create();
	Engine *e = new Engine(stub, dataPath, part);
	e->run(lang);
	delete e;
	delete stub;
	return 0;
}
