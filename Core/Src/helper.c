/*
 * helper.c
 *
 * Byte-oriented memcpy / memset for kernel use (no libc).
 */

#include "helper.h"

void *memcpy(void *dest, const void *src, U32 n)
{
	U8 *d = (U8 *)dest;
	const U8 *s = (const U8 *)src;

	for (U32 i = 0; i < n; i++)
		d[i] = s[i];

	return dest;
}

void *memset(void *s, int c, U32 n)
{
	U8 *p = (U8 *)s;
	U8 byte = (U8)c;

	for (U32 i = 0; i < n; i++)
		p[i] = byte;

	return s;
}
