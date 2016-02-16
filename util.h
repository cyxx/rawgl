/* 
 * Another World Interpreter 
 * (c) 2004-2005 Gregory Montoir
 */

#ifndef __UTIL_H__
#define __UTIL_H__

#include "intern.h"

extern void error(const char *msg, ...);
extern void warning(const char *msg, ...);
extern void info(const char *msg, ...);

extern void string_lower(char *p);
extern void string_upper(char *p);

#endif
