/* 
 * Another World Interpreter 
 * (c) 2004-2005 Gregory Montoir
 */

#ifndef __SYS_H__
#define __SYS_H__

typedef unsigned char uint8;
typedef signed char int8;
typedef unsigned short uint16;
typedef signed short int16;
typedef unsigned long uint32;
typedef signed long int32;

#if defined SYS_LITTLE_ENDIAN

inline uint16 READ_BE_UINT16(const void *ptr) {
	const uint8 *b = (const uint8 *)ptr;
	return (b[0] << 8) | b[1];
}

inline uint32 READ_BE_UINT32(const void *ptr) {
	const uint8 *b = (const uint8 *)ptr;
	return (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3];
}

#elif defined SYS_BIG_ENDIAN

inline uint16 READ_BE_UINT16(const void *ptr) {
#if defined SYS_NEED_ALIGNMENT
	uint16 r;
	memcpy(&r, ptr, 2);
	return r;
#else	
	return *(const uint16 *)ptr;
#endif
}

inline uint32 READ_BE_UINT32(const void *ptr) {
#if defined SYS_NEED_ALIGNMENT
	uint32 r;
	memcpy(&r, ptr, 4);
	return r;
#else
	return *(const uint32 *)ptr;
#endif
}

#else

#error No endianness defined

#endif

#endif
