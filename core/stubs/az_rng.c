/* az_rng.c (reference) - Philox4x32-10, Salmon et al., "Parallel Random
 * Numbers: As Easy as 1, 2, 3" (SC11). Counter-based, no state. */
#include "../az_abi.h"

#define PHILOX_M0 0xD2511F53u
#define PHILOX_M1 0xCD9E8D57u
#define PHILOX_W0 0x9E3779B9u
#define PHILOX_W1 0xBB67AE85u

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

void az_philox4x32_n(u32 *out4n, const u32 *ctr4n, const u32 key[2], u32 n)
{
    for (u32 i = 0; i < n; i++) az_philox4x32(&out4n[4 * i], &ctr4n[4 * i], key);
}
