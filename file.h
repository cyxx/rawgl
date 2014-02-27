
/*
 * Another World engine rewrite
 * Copyright (C) 2004-2005 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifndef FILE_H__
#define FILE_H__

#include "intern.h"

struct File_impl;

struct File {
	File_impl *_impl;

	File(bool gzipped = false);
	~File();

	bool open(const char *filename, const char *directory, const char *mode="rb");
	void close();
	bool ioErr() const;
	void seek(int32_t off);
	void read(void *ptr, uint32_t size);
	uint8_t readByte();
	uint16_t readUint16BE();
	uint32_t readUint32BE();
	void write(void *ptr, uint32_t size);
	void writeByte(uint8_t b);
	void writeUint16BE(uint16_t n);
	void writeUint32BE(uint32_t n);
};

#endif
