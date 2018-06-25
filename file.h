
/*
 * Another World engine rewrite
 * Copyright (C) 2004-2005 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifndef FILE_H__
#define FILE_H__

#include <stdio.h>
#include <stdint.h>

struct File_impl;

struct File {
	File();
	~File();

	File_impl *_impl;

	bool open(const char *filepath);
	bool open(const char *filename, const char *path);
	bool openForWriting(const char *filepath);
	void close();
	bool ioErr() const;
	uint32_t size();
	void seek(int off, int whence = SEEK_SET);
	int read(void *ptr, uint32_t len);
	uint8_t readByte();
	uint16_t readUint16LE();
	uint32_t readUint32LE();
	uint16_t readUint16BE();
	uint32_t readUint32BE();
	int write(void *ptr, uint32_t size);
	void writeByte(uint8_t b);
	void writeUint16LE(uint16_t n);
	void writeUint32LE(uint32_t n);
	void writeUint16BE(uint16_t n);
	void writeUint32BE(uint32_t n);
};

void dumpFile(const char *filename, const uint8_t *p, int size);

#endif
