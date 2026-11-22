/* Hosted SHA-256 (FIPS 180-4). Input is bytes; the 32 digest bytes are in
 * the standard big-endian order. Call init/update/final once per stream. */
#ifndef AZ_SHA256_H
#define AZ_SHA256_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct AzSha256 {
    uint32_t h[8];
    uint64_t bytes;
    size_t used;
    unsigned char block[64];
} AzSha256;

static inline uint32_t az_sha256_rotr(uint32_t x, unsigned n) { return (x >> n) | (x << (32 - n)); }

static inline void az_sha256_block(AzSha256 *s, const unsigned char *data)
{
    static const uint32_t k[64] = {
        0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u,
        0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu,
        0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu,
        0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau, 0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
        0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu,
        0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
        0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u, 0x19a4c116u,
        0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
        0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u,
        0xc67178f2u};
    uint32_t w[64];
    for (int i = 0; i < 16; i++) {
        const unsigned char *p = data + 4 * i;
        w[i] = (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
    }
    for (int i = 16; i < 64; i++) {
        uint32_t x = w[i - 15], y = w[i - 2];
        uint32_t small0 = az_sha256_rotr(x, 7) ^ az_sha256_rotr(x, 18) ^ (x >> 3);
        uint32_t small1 = az_sha256_rotr(y, 17) ^ az_sha256_rotr(y, 19) ^ (y >> 10);
        w[i] = w[i - 16] + small0 + w[i - 7] + small1;
    }
    uint32_t a = s->h[0], b = s->h[1], c = s->h[2], d = s->h[3];
    uint32_t e = s->h[4], f = s->h[5], g = s->h[6], h = s->h[7];
    for (int i = 0; i < 64; i++) {
        uint32_t big1 = az_sha256_rotr(e, 6) ^ az_sha256_rotr(e, 11) ^ az_sha256_rotr(e, 25);
        uint32_t big0 = az_sha256_rotr(a, 2) ^ az_sha256_rotr(a, 13) ^ az_sha256_rotr(a, 22);
        uint32_t t1 = h + big1 + ((e & f) ^ (~e & g)) + k[i] + w[i];
        uint32_t t2 = big0 + ((a & b) ^ (a & c) ^ (b & c));
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    s->h[0] += a;
    s->h[1] += b;
    s->h[2] += c;
    s->h[3] += d;
    s->h[4] += e;
    s->h[5] += f;
    s->h[6] += g;
    s->h[7] += h;
}

static inline void az_sha256_init(AzSha256 *s)
{
    const AzSha256 initial = {{0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au, 0x510e527fu, 0x9b05688cu,
                               0x1f83d9abu, 0x5be0cd19u},
                              0,
                              0,
                              {0}};
    *s = initial;
}

static inline void az_sha256_update(AzSha256 *s, const void *input, size_t len)
{
    const unsigned char *p = (const unsigned char *)input;
    s->bytes += (uint64_t)len;
    while (len) {
        size_t amount = 64 - s->used;
        if (amount > len)
            amount = len;
        memcpy(s->block + s->used, p, amount);
        s->used += amount;
        p += amount;
        len -= amount;
        if (s->used == 64) {
            az_sha256_block(s, s->block);
            s->used = 0;
        }
    }
}

static inline void az_sha256_final(AzSha256 *s, unsigned char digest[32])
{
    uint64_t bits = s->bytes << 3;
    s->block[s->used++] = 0x80;
    if (s->used > 56) {
        memset(s->block + s->used, 0, 64 - s->used);
        az_sha256_block(s, s->block);
        s->used = 0;
    }
    memset(s->block + s->used, 0, 56 - s->used);
    for (int i = 0; i < 8; i++)
        s->block[63 - i] = (unsigned char)(bits >> (8 * i));
    az_sha256_block(s, s->block);
    for (int i = 0; i < 32; i++)
        digest[i] = (unsigned char)(s->h[i / 4] >> (24 - 8 * (i % 4)));
}

static inline void az_sha256(const void *data, size_t len, unsigned char digest[32])
{
    AzSha256 s;
    az_sha256_init(&s);
    az_sha256_update(&s, data, len);
    az_sha256_final(&s, digest);
}

#endif
