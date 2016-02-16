/* 
 * Another World Interpreter 
 * (c) 2004-2005 Gregory Montoir
 */

#ifndef __FILE_H__
#define __FILE_H__

#include "intern.h"

struct File_impl;

struct File {
	File_impl *_impl;

	File(bool gzipped = false);
	~File();

	bool open(const char *filename, const char *directory, const char *mode="rb");
	void close();
	bool ioErr() const;
	uint32 size();
	void seek(int32 off);
	void read(void *ptr, uint32 sz);
	uint8 readByte();
	uint16 readUint16LE();
	uint32 readUint32LE();
	uint16 readUint16BE();
	uint32 readUint32BE();
	void write(void *ptr, uint32 sz);
	void writeByte(uint8 b);
	void writeUint16BE(uint16 n);
	void writeUint32BE(uint32 n);
};

#endif
