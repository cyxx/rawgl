
/*
 * Another World engine rewrite
 * Copyright (C) 2004-2005 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifndef UTIL_H__
#define UTIL_H__

#include "intern.h"

enum {
	DBG_SCRIPT = 1 << 0,
	DBG_BANK  = 1 << 1,
	DBG_VIDEO = 1 << 2,
	DBG_SND   = 1 << 3,
	DBG_SER   = 1 << 4,
	DBG_INFO  = 1 << 5,
	DBG_PAK   = 1 << 6,
};

extern uint16_t g_debugMask;

extern void debug(uint16_t cm, const char *msg, ...);
extern void error(const char *msg, ...);
extern void warning(const char *msg, ...);

extern void string_lower(char *p);
extern void string_upper(char *p);

#endif
