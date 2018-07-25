
#ifndef UTIL_H__
#define UTIL_H__

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void stringLower(char *p);
uint16_t READ_BE_UINT16(const uint8_t *p);
uint32_t READ_BE_UINT32(const uint8_t *p);
void makeDirectory(const char *path);

#endif // UTIL_H__
