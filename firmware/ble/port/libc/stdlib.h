/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef FELUCCA_LIBC_STDLIB_H
#define FELUCCA_LIBC_STDLIB_H
#include <stddef.h>
void *malloc(size_t);
void *calloc(size_t,size_t);
void *realloc(void *,size_t);
void free(void *);
#define abs(x) ((x)<0 ? -(x) : (x))
#endif
