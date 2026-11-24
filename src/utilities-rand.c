/*
 * Keyed random permutation of the block numbers [0, blocks), used by -z -z -z.
 *
 * Let k be the smallest number with 2^k >= blocks.  mix() is a bijection on
 * the k-bit numbers [0, 2^k) selected by a key, built only from steps that
 * are invertible on k bits:
 *
 *   x ^= key       undone by xoring the key again
 *   x ^= x >> s    invertible for s >= 1
 *   x *= odd       invertible mod 2^k
 *
 * The steps and multipliers are those of the xxhash64 finalizer, with the
 * shifts scaled from 64 bits to k bits.  They run twice, once after xoring in
 * each half of the key; with a single round some output bits stay measurably
 * predictable at small k.
 *
 * The i-th block is mix(i), re-applied while the result is >= blocks (cycle
 * walking).  This keeps a bijection on [0, blocks), and as 2^k < 2 * blocks
 * it takes fewer than 2 applications on average.  Unlike the LFSR used
 * before, where the next block was 2x or 2x + 1 of the last one within its
 * power-of-two sub-range, consecutive blocks follow no simple pattern, any
 * position can be computed directly, and the order depends on the whole
 * seed.
 *
 * Kensler, "Correlated Multi-Jittered Sampling", Pixar Technical Memo 13-01
 * (2013) uses the same construction with 32-bit constants; Black & Rogaway,
 * "Ciphers with Arbitrary Finite Domains" (2002) describes cycle walking.
 */
#include <stdlib.h>
#include <stdint.h>

#include "utilities-rand.h"

#define PERM_C2 14029467366897019727ull  /* xxhash64 PRIME64_2 */
#define PERM_C3  1609587929392839161ull  /* xxhash64 PRIME64_3 */

struct PermRange {
  uint64_t blocks;  /* size of the permutation */
  uint64_t key;     /* key of the current permutation */
  uint64_t next;    /* position returned by the next u_perm_range_step() */
  uint64_t mask;    /* 2^k - 1 */
  int sa, sb;       /* shifts scaled to k bits */
};

// splitmix64, a fast 64-bit generator that also mixes nearby seeds well
static uint64_t splitmix64 (uint64_t x)
{
  uint64_t z = x + 0x9e3779b97f4a7c15ull;
  z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
  z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
  return z ^ (z >> 31);
}

static uint64_t mix (PermRange const * range, uint64_t x)
{
  uint64_t const m = range->mask;

  x ^= range->key & m;
  x ^= x >> range->sa; x = (x * PERM_C2) & m;
  x ^= x >> range->sb; x = (x * PERM_C3) & m;
  x ^= x >> range->sa;

  x ^= (range->key >> 32) & m;
  x ^= x >> range->sa; x = (x * PERM_C2) & m;
  x ^= x >> range->sb; x = (x * PERM_C3) & m;
  x ^= x >> range->sa;
  return x;
}

PermRange * u_perm_range_init (uint64_t blocks, uint64_t seed){
  PermRange * range = calloc(1, sizeof(PermRange)); // todo safe_malloc()
  int k = 1;

  if (range == NULL){
    return NULL;
  }
  // the smallest k with 2^k >= blocks
  while (k < 64 && (1ull << k) < blocks){
    k++;
  }
  range->blocks = blocks;
  range->key = splitmix64(seed);
  range->mask = k == 64 ? UINT64_MAX : (1ull << k) - 1;
  range->sa = (k + 1) / 2;
  range->sb = (k + 2) / 3;
  return range;
}

uint64_t u_perm_range_at (PermRange const * range, uint64_t i){
  uint64_t x = mix(range, i);

  // landed outside [0, blocks), apply the bijection again
  while (x >= range->blocks){
    x = mix(range, x);
  }
  return x;
}

uint64_t u_perm_range_step (PermRange * range){
  if (range->blocks == 0){
    // must stop
    return -1;
  }
  if (range->next == range->blocks){
    // all blocks were returned, start a new permutation
    range->next = 0;
    range->key = splitmix64(range->key);
  }
  return u_perm_range_at(range, range->next++);
}

void u_perm_range_free (PermRange * range){
  free(range);
}
