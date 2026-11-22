/* az_png.c - PNG writer without any library.
 *
 * The image data is a zlib stream made of stored (uncompressed) deflate
 * blocks, so the file is large but the writer is a page of code and cannot
 * be wrong about Huffman tables. CRC-32 guards every chunk, Adler-32 the
 * zlib stream.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "az_view.h"

/* CRC-32 (IEEE 802.3, reflected polynomial 0xEDB88320), four bits at a time */
u32 az_png_crc32(u32 crc, const u8 *data, size_t n)
{
    static const u32 nib[16] = {
        0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu, 0x76DC4190u, 0x6B6B51F4u, 0x4DB26158u, 0x5005713Cu,
        0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu, 0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu
    };
    u32 c = ~crc;
    for (size_t i = 0; i < n; i++) {
        c ^= data[i];
        c = (c >> 4) ^ nib[c & 15u];
        c = (c >> 4) ^ nib[c & 15u];
    }
    return ~c;
}

/* Adler-32: two sums modulo 65521; 5552 bytes is the longest run that
 * cannot overflow 32 bits before the reduction */
u32 az_png_adler32(u32 adler, const u8 *data, size_t n)
{
    u32 a = adler & 0xFFFFu, b = adler >> 16;
    while (n) {
        size_t k = n < 5552 ? n : 5552;
        for (size_t i = 0; i < k; i++) { a += data[i]; b += a; }
        a %= 65521u; b %= 65521u;
        data += k; n -= k;
    }
    return (b << 16) | a;
}

static void put_be32(u8 *p, u32 v)
{
    p[0] = (u8)(v >> 24); p[1] = (u8)(v >> 16); p[2] = (u8)(v >> 8); p[3] = (u8)v;
}

/* finish a chunk whose length field starts at `at` and whose data is `len` bytes */
static size_t chunk_close(u8 *buf, size_t at, const char type[4], size_t len)
{
    put_be32(buf + at, (u32)len);
    memcpy(buf + at + 4, type, 4);
    put_be32(buf + at + 8 + len, az_png_crc32(0, buf + at + 4, len + 4));
    return at + 12 + len;
}

u8 *az_png_encode(const u32 *pixels, u32 w, u32 h, size_t *out_len)
{
    if (out_len) *out_len = 0;
    if (!pixels || w == 0 || h == 0 || w > 16384 || h > 16384) return NULL;

    bool opaque = true;
    size_t npx = (size_t)w * h;
    for (size_t i = 0; i < npx; i++) if ((pixels[i] >> 24) != 0xFFu) { opaque = false; break; }
    u32 bpp = opaque ? 3u : 4u;

    size_t row = 1 + (size_t)w * bpp;                  /* filter byte + pixels */
    size_t raw = row * h;
    size_t n_blocks = (raw + 65534) / 65535;
    size_t zlen = 2 + n_blocks * 5 + raw + 4;          /* a single IDAT holds it: < 2^31 */
    size_t total = 8 + (12 + 13) + (12 + zlen) + 12;
    u8 *buf = (u8 *)malloc(total);
    if (!buf) return NULL;

    static const u8 sig[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
    memcpy(buf, sig, 8);
    size_t at = 8;

    u8 *ih = buf + at + 8;
    put_be32(ih, w); put_be32(ih + 4, h);
    ih[8] = 8;                    /* bit depth */
    ih[9] = opaque ? 2 : 6;       /* colour type: RGB or RGBA */
    ih[10] = 0; ih[11] = 0; ih[12] = 0;
    at = chunk_close(buf, at, "IHDR", 13);

    u8 *z = buf + at + 8;
    size_t zp = 0;
    z[zp++] = 0x78; z[zp++] = 0x01;                    /* deflate, 32K window, no preset dictionary */
    u32 s1 = 1, s2 = 0, pending = 0;                   /* Adler-32 sums, reduced every 5552 bytes */
    size_t done = 0;                                   /* raw bytes emitted so far */
    size_t left = 0;                                   /* raw bytes left in the open block */
    for (u32 y = 0; y < h; y++) {
        const u32 *src = pixels + (size_t)y * w;
        for (size_t k = 0; k < row; k++) {
            if (left == 0) {
                size_t rem = raw - done;
                size_t len = rem < 65535 ? rem : 65535;
                z[zp++] = (len == rem) ? 1 : 0;        /* BFINAL, BTYPE = 00 (stored) */
                z[zp++] = (u8)len; z[zp++] = (u8)(len >> 8);
                z[zp++] = (u8)~len; z[zp++] = (u8)(~len >> 8);
                left = len;
            }
            u8 b;
            if (k == 0) {
                b = 0;                                 /* filter type 0: none */
            } else {
                u32 p = src[(k - 1) / bpp];
                u32 c = (u32)((k - 1) % bpp);
                b = (u8)(c == 0 ? p >> 16 : c == 1 ? p >> 8 : c == 2 ? p : p >> 24);
            }
            z[zp++] = b;
            s1 += b; s2 += s1;
            if (++pending == 5552) { s1 %= 65521u; s2 %= 65521u; pending = 0; }
            done++; left--;
        }
    }
    s1 %= 65521u; s2 %= 65521u;
    put_be32(z + zp, (s2 << 16) | s1); zp += 4;
    at = chunk_close(buf, at, "IDAT", zp);
    at = chunk_close(buf, at, "IEND", 0);

    if (zp != zlen || at != total) { free(buf); return NULL; }   /* size arithmetic must agree */
    if (out_len) *out_len = total;
    return buf;
}

bool az_png_write(const char *path, const u32 *pixels, u32 w, u32 h)
{
    size_t n = 0;
    u8 *buf = az_png_encode(pixels, w, h, &n);
    if (!buf) return false;
    FILE *f = fopen(path, "wb");
    bool ok = false;
    if (f) {
        ok = fwrite(buf, 1, n, f) == n;
        if (fclose(f) != 0) ok = false;
    }
    free(buf);
    return ok;
}
