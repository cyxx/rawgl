
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
	"  --version=VER     Version of the game to load : fr, eur, us (default)\n"
	"  --part=NUM        Starts at specific game part (1-9)\n"
	;

static const struct {
	const char *name;
	Engine::Version ver;
} VERSIONS[] = {
	{ "fr",  Engine::VER_FR  },
	{ "us",  Engine::VER_US  },
	{ "eur", Engine::VER_EUR }
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
	const char *version = 0;
	int part = 1;
	Engine::Version ver = Engine::VER_US;
	for (int i = 1; i < argc; ++i) {
		bool opt = false;
		if (strlen(argv[i]) >= 2) {
			opt |= parseOption(argv[i], "datapath=", &dataPath);
			opt |= parseOption(argv[i], "version=", &version);
			opt |= parseOptionInt(argv[i], "part=", &part);
		}
		if (!opt) {
			printf("%s\n", USAGE);
			return 0;
		}
	}
	if (version) {
		for (unsigned int j = 0; j < ARRAYSIZE(VERSIONS); ++j) {
			if (strcmp(version, VERSIONS[j].name) == 0) {
				ver = VERSIONS[j].ver;
				break;
			}
		}
	}
	g_debugMask = DBG_INFO; // | DBG_VIDEO | DBG_SND | DBG_SCRIPT | DBG_BANK | DBG_SER;
	SystemStub *stub = SystemStub_OGL_create();
	Engine *e = new Engine(stub, dataPath, part);
	e->run(ver);
	delete e;
	delete stub;
	return 0;
}
