#ifndef _UTILITIES_RAND_H
#define _UTILITIES_RAND_H

#include <stdint.h>

typedef struct PermRange PermRange;

/*
Keyed random permutation of the block numbers [0, blocks), selected by seed
*/
PermRange * u_perm_range_init (uint64_t blocks, uint64_t seed);

/*
Return each block once in random order, then start a new permutation
*/
uint64_t u_perm_range_step (PermRange * range);

/*
The i-th block of the current permutation, for any i in [0, blocks)
*/
uint64_t u_perm_range_at (PermRange const * range, uint64_t i);

void u_perm_range_free (PermRange * range);

#endif
