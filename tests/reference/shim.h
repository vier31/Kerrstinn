// Force-included (-include) into the original engine sources for
// reference_check. Swaps the C library's rand() for kerr::Rand, so both
// flavours can be compared, and pads every allocation with zeros, so the
// original's out-of-bounds reads see 0 (what the port's guards return).
#pragma once

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void ref_srand (unsigned int seed);
int ref_rand();
void* ref_malloc (size_t n);
void* ref_realloc (void* p, size_t n);

#define srand ref_srand
#define rand ref_rand
#define malloc ref_malloc
#define realloc ref_realloc
