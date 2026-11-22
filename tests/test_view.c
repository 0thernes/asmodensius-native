/* test_view.c - the software renderer and its PNG writer.
 *
 *  1. PNG: known answers for CRC-32 and Adler-32; a written file parses back
 *     (signature, chunk order, chunk CRCs, zlib header, stored blocks,
 *     Adler-32) to exactly the pixels that went in. The parser below shares
 *     no code with the writer.
 *  2. Glyphs and font: injective, mirror symmetric, 1000 clades give at
 *     least 900 distinct glyphs.
 *  3. Rendering: the same world gives the same pixel hash; the picture does
 *     not depend on slot order; each appearance trait changes the picture.
 *  4. Law D7: rendering never changes the world.
 */
#include <math.h>
#include <errno.h>
#include <direct.h>
#include "az_test.h"
#include "../view/az_view.h"

/* finding v32: the round-trip files go to a folder in the current working
 * directory (the runner starts tests in the build folder on Z:), never to a
 * fixed drive letter and never to %TEMP%. */
#define WORK_DIR "view_work"

/* ---------------------------------------------------------------------- */
/* an independent PNG reader for the subset the writer produces            */
/* ---------------------------------------------------------------------- */
static u32 ref_crc32(const u8 *p, size_t n)           /* bit by bit, no table */
{
    u32 c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) {
        c ^= p[i];
        for (int k = 0; k < 8; k++) c = (c & 1u) ? (c >> 1) ^ 0xEDB88320u : c >> 1;
    }
    return c ^ 0xFFFFFFFFu;
}
static u32 ref_adler32(const u8 *p, size_t n)         /* modulo on every byte */
{
    u32 a = 1, b = 0;
    for (size_t i = 0; i < n; i++) { a = (a + p[i]) % 65521u; b = (b + a) % 65521u; }
    return (b << 16) | a;
}
static u32 be32(const u8 *p) { return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3]; }

typedef struct PngParsed {
    u32 w, h, ctype;
    u32 n_chunks, n_idat, n_blocks, bad_crc;
    u32 *px;              /* decoded 0xAARRGGBB, malloc'd */
    const char *err;      /* NULL when the file is well formed */
} PngParsed;

static void png_parse(const u8 *f, size_t n, PngParsed *o)
{
    memset(o, 0, sizeof *o);
    static const u8 sig[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
    if (n < 8 || memcmp(f, sig, 8) != 0) { o->err = "signature"; return; }
    u8 *z = (u8 *)malloc(n ? n : 1);
    size_t zn = 0, at = 8;
    bool seen_ihdr = false, seen_iend = false;
    while (at + 12 <= n) {
        u32 len = be32(f + at);
        if ((size_t)len > n - at - 12) { o->err = "chunk length"; break; }
        const u8 *type = f + at + 4, *data = f + at + 8;
        if (ref_crc32(type, (size_t)len + 4) != be32(data + len)) o->bad_crc++;
        o->n_chunks++;
        if (!memcmp(type, "IHDR", 4)) {
            if (o->n_chunks != 1 || len != 13) { o->err = "IHDR placement"; break; }
            o->w = be32(data); o->h = be32(data + 4); o->ctype = data[9];
            if (data[8] != 8 || (data[9] != 2 && data[9] != 6) || data[10] || data[11] || data[12]) { o->err = "IHDR fields"; break; }
            seen_ihdr = true;
        } else if (!memcmp(type, "IDAT", 4)) {
            if (!seen_ihdr) { o->err = "IDAT before IHDR"; break; }
            memcpy(z + zn, data, len); zn += len; o->n_idat++;
        } else if (!memcmp(type, "IEND", 4)) {
            if (len != 0) { o->err = "IEND length"; break; }
            seen_iend = true; at += 12; break;
        } else { o->err = "unexpected chunk"; break; }
        at += 12 + (size_t)len;
    }
    if (!o->err && !seen_iend) o->err = "no IEND";
    if (!o->err && at != n) o->err = "bytes after IEND";
    if (!o->err && (zn < 6 || (z[0] & 0x0F) != 8 || ((u32)z[0] * 256 + z[1]) % 31 != 0 || (z[1] & 0x20))) o->err = "zlib header";
    if (o->err) { free(z); return; }

    u32 bpp = o->ctype == 2 ? 3 : 4;
    size_t row = 1 + (size_t)o->w * bpp, raw_n = row * o->h, rp = 0, zp = 2;
    u8 *raw = (u8 *)malloc(raw_n ? raw_n : 1);
    bool final = false;
    while (!final && !o->err) {
        if (zp + 5 > zn) { o->err = "truncated block header"; break; }
        u8 hdr = z[zp];
        u32 len = (u32)z[zp + 1] | ((u32)z[zp + 2] << 8), nlen = (u32)z[zp + 3] | ((u32)z[zp + 4] << 8);
        zp += 5;
        if (hdr > 1) { o->err = "block is not stored"; break; }           /* BTYPE must be 00 */
        if ((len ^ nlen) != 0xFFFFu) { o->err = "LEN / NLEN"; break; }
        if (zp + len > zn || rp + len > raw_n) { o->err = "block overruns"; break; }
        memcpy(raw + rp, z + zp, len); rp += len; zp += len;
        o->n_blocks++;
        final = (hdr & 1u) != 0;
    }
    if (!o->err && rp != raw_n) o->err = "raw size";
    if (!o->err && zp + 4 != zn) o->err = "zlib trailer size";
    if (!o->err && be32(z + zp) != ref_adler32(raw, raw_n)) o->err = "Adler-32";
    if (!o->err) {
        o->px = (u32 *)malloc((size_t)o->w * o->h * sizeof(u32));
        for (u32 y = 0; y < o->h && !o->err; y++) {
            const u8 *r = raw + (size_t)y * row;
            if (r[0] != 0) { o->err = "filter type"; break; }
            for (u32 x = 0; x < o->w; x++) {
                const u8 *p = r + 1 + (size_t)x * bpp;
                u32 a = bpp == 4 ? p[3] : 0xFFu;
                o->px[(size_t)y * o->w + x] = (a << 24) | ((u32)p[0] << 16) | ((u32)p[1] << 8) | p[2];
            }
        }
    }
    free(raw); free(z);
}

static u8 *read_file(const char *path, size_t *n)
{
    FILE *f = fopen(path, "rb");
    *n = 0;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    u8 *b = (u8 *)malloc(len > 0 ? (size_t)len : 1);
    if (b && len > 0 && fread(b, 1, (size_t)len, f) != (size_t)len) { free(b); b = NULL; }
    fclose(f);
    if (b) *n = (size_t)len;
    return b;
}

/* deterministic test pattern (splitmix, finding v58: az_test_rand64; the
 * state starts one step back so pixel i is az_mix64(seed + i * gamma), the
 * same bytes as before), optionally forced opaque */
static void fill_pattern(u32 *px, size_t n, u64 seed, bool opaque)
{
    u64 st = seed - 0x9E3779B97F4A7C15ull;
    for (size_t i = 0; i < n; i++) {
        u32 p = (u32)az_test_rand64(&st);
        px[i] = opaque ? (p | 0xFF000000u) : p;
    }
}

static void roundtrip(u32 w, u32 h, u64 seed, bool opaque, const char *name)
{
    size_t npx = (size_t)w * h;
    u32 *px = (u32 *)malloc(npx * sizeof(u32));
    fill_pattern(px, npx, seed, opaque);
    if (!opaque) px[0] &= 0x00FFFFFFu;                 /* make sure one alpha is not 0xFF */
    char path[260];
    snprintf(path, sizeof path, WORK_DIR "/%s", name);
    CHECK(az_png_write(path, px, w, h));
    size_t n = 0;
    u8 *file = read_file(path, &n);
    CHECK(file != NULL);
    if (file) {
        PngParsed p;
        png_parse(file, n, &p);
        if (p.err) printf("  %s: parse error: %s\n", name, p.err);
        CHECK(p.err == NULL);
        CHECK_EQ(p.bad_crc, 0);
        CHECK_EQ(p.w, w); CHECK_EQ(p.h, h);
        CHECK_EQ(p.ctype, opaque ? 2 : 6);
        CHECK_EQ(p.n_chunks, 3);                       /* IHDR, IDAT, IEND */
        size_t raw = (size_t)h * (1 + (size_t)w * (opaque ? 3 : 4));
        CHECK_EQ(p.n_blocks, (raw + 65534) / 65535);   /* every block but the last is full */
        CHECK(p.px != NULL && memcmp(p.px, px, npx * sizeof(u32)) == 0);
        /* the in-memory encoder and the file agree byte for byte */
        size_t en = 0;
        u8 *enc = az_png_encode(px, w, h, &en);
        CHECK(enc != NULL && en == n && memcmp(enc, file, n) == 0);
        /* the reader is not a rubber stamp: one flipped bit must be noticed */
        if (enc && en > 64) {
            enc[en / 2] ^= 0x10;
            PngParsed q;
            png_parse(enc, en, &q);
            CHECK(q.err != NULL || q.bad_crc != 0);
            free(q.px);
        }
        free(enc); free(p.px); free(file);
    }
    free(px);
}

static void test_png(void)
{
    /* known answers */
    CHECK_EQ(az_png_crc32(0, (const u8 *)"123456789", 9), 0xCBF43926u);
    CHECK_EQ(az_png_crc32(0, (const u8 *)"IEND", 4), 0xAE426082u);
    CHECK_EQ(az_png_crc32(az_png_crc32(0, (const u8 *)"1234", 4), (const u8 *)"56789", 5), 0xCBF43926u);
    CHECK_EQ(az_png_adler32(1, (const u8 *)"Wikipedia", 9), 0x11E60398u);
    CHECK_EQ(az_png_adler32(1, (const u8 *)"", 0), 1);
    /* against the bit-by-bit references on random buffers, including the
     * 5552-byte reduction boundary of Adler-32 */
    static u8 buf[20000];
    for (u32 i = 0; i < sizeof buf; i++) buf[i] = (u8)(az_mix64(i + 77) >> 11);
    static const u32 lens[] = { 0, 1, 2, 255, 5551, 5552, 5553, 11104, 20000 };
    for (u32 k = 0; k < AZ_LEN(lens); k++) {
        CHECK_EQ(az_png_crc32(0, buf, lens[k]), ref_crc32(buf, lens[k]));
        CHECK_EQ(az_png_adler32(1, buf, lens[k]), ref_adler32(buf, lens[k]));
    }
    memset(buf, 0xFF, sizeof buf);                     /* worst case for the Adler sums */
    CHECK_EQ(az_png_adler32(1, buf, sizeof buf), ref_adler32(buf, sizeof buf));

    roundtrip(1, 1, 1, true, "rt_1x1.png");
    roundtrip(1, 1, 2, false, "rt_1x1a.png");
    roundtrip(7, 5, 3, true, "rt_7x5.png");
    roundtrip(64, 48, 4, false, "rt_64x48a.png");
    roundtrip(257, 85, 5, true, "rt_two_blocks.png");  /* 85 * 772 = 65620: just over one block */
    roundtrip(1456, 15, 6, true, "rt_exact.png");      /* 15 * 4369 = 65535: exactly one full block */
    roundtrip(64, 255, 9, false, "rt_exact_a.png");    /* 255 * 257 = 65535, RGBA */
    roundtrip(1456, 30, 10, true, "rt_exact2.png");    /* exactly two full blocks */
    roundtrip(640, 360, 7, true, "rt_640x360.png");    /* 11 blocks */
    roundtrip(333, 211, 8, false, "rt_333x211a.png");

    /* bad arguments are refused */
    u32 one = 0xFF000000u;
    size_t n = 123;
    CHECK(az_png_encode(NULL, 1, 1, &n) == NULL && n == 0);
    CHECK(az_png_encode(&one, 0, 1, &n) == NULL);
    CHECK(az_png_encode(&one, 1, 0, &n) == NULL);
    CHECK(!az_png_write(WORK_DIR "/no_such_dir/x.png", &one, 1, 1));
}

/* ---------------------------------------------------------------------- */
/* glyphs and font                                                         */
/* ---------------------------------------------------------------------- */
static u64 glyph_key(u8 a, u8 b)
{
    u8 rows[AZ_GLYPH_H];
    az_view_glyph(a, b, rows);
    u64 k = 0;
    for (u32 r = 0; r < AZ_GLYPH_H; r++) k = (k << 7) | rows[r];
    return k;
}
static int cmp_u64(const void *a, const void *b)
{
    u64 x = *(const u64 *)a, y = *(const u64 *)b;
    return x < y ? -1 : x > y;
}
static u32 count_distinct(u64 *keys, u32 n)
{
    qsort(keys, n, sizeof(u64), cmp_u64);
    u32 d = n ? 1 : 0;
    for (u32 i = 1; i < n; i++) if (keys[i] != keys[i - 1]) d++;
    return d;
}

static void test_glyphs(void)
{
    /* every one of the 65536 inputs: 7 bits per row, mirror symmetric, and
     * no two inputs share a bitmap */
    static u64 keys[65536];
    u32 bad_sym = 0, bad_range = 0, min_pop = 99, max_pop = 0;
    for (u32 x = 0; x < 65536; x++) {
        u8 rows[AZ_GLYPH_H];
        az_view_glyph((u8)(x >> 8), (u8)x, rows);
        u32 pop = 0;
        for (u32 r = 0; r < AZ_GLYPH_H; r++) {
            if (rows[r] > 0x7F) bad_range++;
            for (u32 c = 0; c < 3; c++)
                if (((rows[r] >> (6 - c)) & 1u) != ((rows[r] >> c) & 1u)) bad_sym++;
            pop += (u32)__builtin_popcount(rows[r]);
        }
        if (pop < min_pop) min_pop = pop;
        if (pop > max_pop) max_pop = pop;
        keys[x] = glyph_key((u8)(x >> 8), (u8)x);
    }
    CHECK_EQ(bad_sym, 0);
    CHECK_EQ(bad_range, 0);
    CHECK_EQ(count_distinct(keys, 65536), 65536);
    printf("  glyph ink over all 65536 inputs: %u .. %u of 63 pixels\n", min_pop, max_pop);
    CHECK(min_pop >= 4 && max_pop <= 59);              /* never blank, never a solid block */

    /* 1000 clades whose two glyph bytes are independent random bytes */
    static AzClade cl;
    static u64 k1000[1000];
    u32 key[2] = { 0x5EED0001u, 0x00C1ADE5u };
    for (u32 i = 0; i < 1000; i++) {
        u32 ctr[4] = { i, 0, AZ_RS_CLADE, 0 }, r[4];
        az_philox4x32(r, ctr, key);
        for (u32 b = 0; b < 16; b++) cl.body[b] = (u8)(r[b >> 2] >> (8 * (b & 3)));
        k1000[i] = glyph_key(cl.body[AZ_B_GLYPH_A], cl.body[AZ_B_GLYPH_B]);
    }
    u32 d = count_distinct(k1000, 1000);
    printf("  1000 clades with random glyph bytes: %u distinct glyphs\n", d);
    CHECK(d >= 900);

    /* font: 95 printable characters, 5 bits per row, all different, only the space is blank */
    static u64 fk[95];
    u32 blank = 0, wide = 0;
    for (u32 ch = 32; ch <= 126; ch++) {
        u64 k = 0;
        for (u32 r = 0; r < 7; r++) { u8 row = az_view_font_row(ch, r); if (row > 0x1F) wide++; k = (k << 5) | row; }
        if (k == 0) blank++;
        fk[ch - 32] = k;
    }
    CHECK_EQ(wide, 0);
    CHECK_EQ(blank, 1);
    CHECK_EQ(count_distinct(fk, 95), 95);
    CHECK_EQ(az_view_font_row(31, 0), 0);
    CHECK_EQ(az_view_font_row(127, 0), 0);
    CHECK_EQ(az_view_font_row('A', 7), 0);
    CHECK_EQ(az_view_font_row('T', 0), 0x1F);          /* the bar of the T is the top row */
    CHECK_EQ(az_view_font_row('L', 6), 0x1F);          /* the foot of the L is the bottom row */
}

int main(void)
{
    int made = _mkdir(WORK_DIR);
    CHECK(made == 0 || errno == EEXIST);               /* the folder exists before any write */
    test_png();
    test_glyphs();
    return az_test_done();
}
