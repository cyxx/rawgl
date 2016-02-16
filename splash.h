/* 
 * Another World Interpreter 
 * (c) 2004-2005 Gregory Montoir
 */

#ifndef __SPLASH_H__
#define __SPLASH_H__

#include "intern.h"

struct SystemStub;

struct Splash {
	static const char *_splashFileName;
	
	const char *_dataPath;
	SystemStub *_stub;
	Color *_pal;
	uint8 *_pic;
	
	Splash(const char *dataPath, SystemStub *stub);
	~Splash();
	
	void decodeData(char * filename);
	void handle(char * filename);
};

#endif
