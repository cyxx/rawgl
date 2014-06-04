
/*
 * Another World engine rewrite
 * Copyright (C) 2004-2005 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include "engine.h"
#include "systemstub.h"

static const char *USAGE = 
	"Raw - Another World Interpreter\n"
	"Usage: raw [OPTIONS]...\n"
	"  --datapath=PATH   Path to where the game is installed (default '.')\n"
	"  --language=LANG   Language of the game to use (fr,us)\n"
	"  --part=NUM        Starts at specific game part (1-9)\n"
	"  --render=NAME     Renderer to use (original,gl)\n"
	;

static const struct {
	const char *name;
	Language lang;
} LANGUAGES[] = {
	{ "fr", LANG_FR  },
	{ "us", LANG_US  },
};

static bool parseOption(const char *arg, const char *longCmd, const char **opt) {
	bool ret = false;
	if (arg[0] == '-' && arg[1] == '-') {
		if (strncmp(arg + 2, longCmd, strlen(longCmd)) == 0) {
			*opt = arg + 2 + strlen(longCmd);
			ret = true;
		}
	}
	return ret;
}

static bool parseOptionInt(const char *arg, const char *name, int *i) {
	const char *opt;
	if (parseOption(arg, name, &opt)) {
		*i = strtol(opt, 0, 0);
		return true;
	}
	return false;
}

#undef main
int main(int argc, char *argv[]) {
	const char *dataPath = "data";
	const char *language = 0;
	int part = 1;
	Language lang = LANG_FR;
	const char *render = "gl";
	for (int i = 1; i < argc; ++i) {
		bool opt = false;
		if (strlen(argv[i]) >= 2) {
			opt |= parseOption(argv[i], "datapath=", &dataPath);
			opt |= parseOption(argv[i], "language=", &language);
			opt |= parseOptionInt(argv[i], "part=", &part);
			opt |= parseOption(argv[i], "render=", &render);
		}
		if (!opt) {
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
	g_debugMask = DBG_INFO; // | DBG_VIDEO | DBG_SND | DBG_SCRIPT | DBG_BANK | DBG_SER;
	SystemStub *stub = SystemStub_OGL_create(render);
	Engine *e = new Engine(stub, dataPath, part);
	e->run(lang);
	delete e;
	delete stub;
	return 0;
}
