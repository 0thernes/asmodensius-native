/* az_rng.c - Philox4x32-10, Salmon, Moraes, Dror, Shaw: "Parallel Random
 * Numbers: As Easy as 1, 2, 3" (SC11). Counter-based, no state.
 *
 * out = philox(ctr, key) is a pure function of 192 input bits. For a fixed
 * key it is a bijection on the 128-bit counter, so two different counters
 * never give the same block. The simulation uses
 *     ctr = { id, tick, stream, index },  key = seed
 * (az_rand4 in az_abi.h), which is why a draw does not depend on thread
 * schedule, on processing order, or on which other ids were drawn.
 *
 * This file is the reference. kernels/az_philox_avx2.asm is its twin and
 * must stay bit-identical to az_philox4x32_n on every input
 * (tests/test_rng.c proves it).
 *
 * One round, with (L, H) = low and high half of a 32x32 -> 64 product:
 *     c0' = H(M1 * c2) ^ c1 ^ k0        c1' = L(M1 * c2)
 *     c2' = H(M0 * c0) ^ c3 ^ k1        c3' = L(M0 * c0)
 * and the key is bumped by the Weyl constants (W0, W1) after each round.
 */
#include "az_abi.h"

#define PHILOX_M0 0xD2511F53u
#define PHILOX_M1 0xCD9E8D57u
#define PHILOX_W0 0x9E3779B9u   /* golden ratio */
#define PHILOX_W1 0xBB67AE85u   /* sqrt(3) - 1  */

void az_philox4x32(u32 out[4], const u32 ctr[4], const u32 key[2])
{
    u32 c0 = ctr[0], c1 = ctr[1], c2 = ctr[2], c3 = ctr[3];
    u32 k0 = key[0], k1 = key[1];
    for (u32 r = 0; r < 10; r++) {
        u64 p0 = (u64)PHILOX_M0 * c0;
        u64 p1 = (u64)PHILOX_M1 * c2;
        u32 n0 = (u32)(p1 >> 32) ^ c1 ^ k0;
        u32 n1 = (u32)p1;
        u32 n2 = (u32)(p0 >> 32) ^ c3 ^ k1;
        u32 n3 = (u32)p0;
        c0 = n0; c1 = n1; c2 = n2; c3 = n3;
        k0 += PHILOX_W0;
        k1 += PHILOX_W1;
    }
    out[0] = c0; out[1] = c1; out[2] = c2; out[3] = c3;
}

/* n independent blocks. Each block is read completely before it is written,
 * so out4n == ctr4n (in place) is allowed; any other overlap is not. */
void az_philox4x32_n(u32 *out4n, const u32 *ctr4n, const u32 key[2], u32 n)
{
    for (u32 i = 0; i < n; i++) az_philox4x32(&out4n[4 * i], &ctr4n[4 * i], key);
}
