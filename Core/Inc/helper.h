/*
 * helper.h
 *
 * Small replacements for libc routines used inside the kernel.
 * Do not use string.h / memcpy / memset from newlib in kernel code.
 */

#ifndef INC_HELPER_H_
#define INC_HELPER_H_

#include "common.h"

void *memcpy(void *dest, const void *src, U32 n);
void *memset(void *s, int c, U32 n);

#endif /* INC_HELPER_H_ */
