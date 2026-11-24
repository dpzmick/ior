// Check that u_perm_range_step() returns a permutation of [0, blocks):
// every block exactly once per "blocks" calls, for many block counts and
// seeds, including after the permutation restarts, and that the order
// depends on the seed.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <utilities-rand.h>

static int check_perm(uint64_t blocks, uint64_t seed, int passes)
{
  unsigned char *seen = malloc(blocks ? blocks : 1);
  PermRange *range = u_perm_range_init(blocks, seed);
  int pass;

  if (seen == NULL || range == NULL) {
    fprintf(stderr, "out of memory for %llu blocks\n", (unsigned long long) blocks);
    exit(1);
  }
  for (pass = 0; pass < passes; pass++) {
    memset(seen, 0, blocks);
    for (uint64_t i = 0; i < blocks; i++) {
      uint64_t b = u_perm_range_step(range);

      if (b >= blocks || seen[b]) {
        fprintf(stderr, "blocks=%llu seed=%llu pass=%d step=%llu: %s block %llu\n",
                (unsigned long long) blocks, (unsigned long long) seed, pass,
                (unsigned long long) i, b >= blocks ? "out of range" : "repeated",
                (unsigned long long) b);
        return 1;
      }
      seen[b] = 1;
    }
  }
  u_perm_range_free(range);
  free(seen);
  return 0;
}

static int cmp_u64(const void *a, const void *b)
{
  uint64_t x = *(const uint64_t *) a, y = *(const uint64_t *) b;

  return x < y ? -1 : x > y;
}

// the first steps of a very large range are in range and do not repeat
static int check_large(uint64_t blocks, uint64_t seed, size_t steps)
{
  uint64_t *v = malloc(steps * sizeof(*v));
  PermRange *range = u_perm_range_init(blocks, seed);
  size_t i;

  for (i = 0; i < steps; i++) {
    v[i] = u_perm_range_step(range);
    if (v[i] >= blocks) {
      fprintf(stderr, "blocks=%llu seed=%llu: block %llu out of range\n",
              (unsigned long long) blocks, (unsigned long long) seed,
              (unsigned long long) v[i]);
      return 1;
    }
  }
  qsort(v, steps, sizeof(*v), cmp_u64);
  for (i = 1; i < steps; i++) {
    if (v[i] == v[i - 1]) {
      fprintf(stderr, "blocks=%llu seed=%llu: block %llu repeated\n",
              (unsigned long long) blocks, (unsigned long long) seed,
              (unsigned long long) v[i]);
      return 1;
    }
  }
  u_perm_range_free(range);
  free(v);
  return 0;
}

// the same seed gives the same order, u_perm_range_at() agrees with the
// steps, and seeds differing in one bit give different orders
static int check_seeds(uint64_t blocks)
{
  PermRange *a = u_perm_range_init(blocks, 42), *b = u_perm_range_init(blocks, 42);
  int rc = 0;

  for (uint64_t i = 0; i < blocks && i < 100000; i++) {
    uint64_t x = u_perm_range_step(a);

    if (x != u_perm_range_step(b) || x != u_perm_range_at(b, i)) {
      fprintf(stderr, "blocks=%llu: seed 42 gave two orders\n", (unsigned long long) blocks);
      rc = 1;
      break;
    }
  }
  u_perm_range_free(a);
  u_perm_range_free(b);
  for (int bit = 0; bit < 64; bit++) {
    PermRange *c = u_perm_range_init(blocks, 42), *d = u_perm_range_init(blocks, 42 ^ (1ULL << bit));
    uint64_t same = 0;

    for (uint64_t i = 0; i < 64; i++)
      same += u_perm_range_at(c, i) == u_perm_range_at(d, i);
    if (same > 8) {
      fprintf(stderr, "blocks=%llu: seeds differing in bit %d share %llu of the first 64 blocks\n",
              (unsigned long long) blocks, bit, (unsigned long long) same);
      rc = 1;
    }
    u_perm_range_free(c);
    u_perm_range_free(d);
  }
  return rc;
}

int main(void)
{
  int rc = 0;
  uint64_t blocks, seed;
  int k;

  // an empty range stops instead of searching forever
  {
    PermRange *empty = u_perm_range_init(0, 1);

    if (u_perm_range_step(empty) != UINT64_MAX) {
      fprintf(stderr, "blocks=0: step did not return -1\n");
      rc = 1;
    }
    u_perm_range_free(empty);
  }

  // every small block count, including those below 8
  for (blocks = 0; blocks <= 2048; blocks++)
    for (seed = 1; seed <= 16; seed++)
      rc |= check_perm(blocks, seed, 2);

  // every width up to 2^22, exact powers of two and just above them
  for (k = 0; k <= 22; k++) {
    rc |= check_perm(1ULL << k, 1, 1);
    rc |= check_perm((1ULL << k) + 1, 3, 1);
    rc |= check_perm((1ULL << k) + 1000003, 7, 1);
  }

  // many ranks with IO500-like file sizes: multiples of 2MiB / 4KiB
  for (seed = 1; seed <= 100000; seed += 997)
    rc |= check_perm(512 * 1234 + 8 * (seed % 64), seed, 1);
  for (seed = 100000; seed <= 1000000; seed += 99991)
    rc |= check_perm(512 * 1234, seed, 1);

  // the widest ranges, up to 2^64 - 1 blocks
  rc |= check_large((1ULL << 40) + (1ULL << 35) + 12345, 1, 1000000);
  rc |= check_large(UINT64_MAX, 123456789, 1000000);
  rc |= check_large(1ULL << 63, 99999, 1000000);
  rc |= check_large((1ULL << 63) + 1, 5, 1000000);

  rc |= check_seeds(1000);
  rc |= check_seeds(76800);
  rc |= check_seeds(1ULL << 34);

  printf("permutation test %s\n", rc ? "FAILED" : "passed");
  return rc;
}
