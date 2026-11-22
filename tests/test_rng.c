/* test_rng.c - Philox4x32-10: the C reference (core/az_rng.c) and its AVX2
 * twin (kernels/az_philox_avx2.asm).
 *
 *   1. the three known answers published with Random123 (kat_vectors)
 *   2. the 16 vectors of tests/vectors/philox_v40.json (the TypeScript lab)
 *   3. structure: batch == single, in place, bijection, input sensitivity
 *   4. a draw for one id does not depend on which other ids are drawn, or
 *      in which order
 *   5. twin == reference for n = 0..40 at four alignments, guard bytes
 *      before and after both arrays untouched, counters not modified
 *   6. twin with both arrays flush against inaccessible pages (a read or
 *      write of one byte outside faults and fails the test)
 *   7. twin == reference for 1,000,000 random blocks, then 250,000 more
 *      shaped like the simulation's counters
 *   8. callee-saved registers survive the twin (with a negative control)
 *   9. az_kernels_install installs the twin
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "az_test.h"
#include "../kernels/az_kernels.h"

/* ---- test-local generator (splitmix64); never used by the simulation ---- */
static u64 tr_state = 0x243F6A8885A308D3ull;
static u64 tr_next(void) { return az_test_rand64(&tr_state); }
static u32 tr_u32(void) { return (u32)(tr_next() >> 32); }
static u32 tr_below(u32 n) { return (u32)(((tr_next() >> 32) * (u64)n) >> 32); }

/* ---- 2. vectors of tests/vectors/philox_v40.json, in file order ---- */
typedef struct { u32 ctr[4], key[2], out[4]; } PhiloxVec;
static const PhiloxVec V40[16] = {
    { { 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u }, { 0x00000000u, 0x00000000u }, { 0x6627e8d5u, 0xe169c58du, 0xbc57ac4cu, 0x9b00dbd8u } },
    { { 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu }, { 0xffffffffu, 0xffffffffu }, { 0x408f276du, 0x41c83b0eu, 0xa20bc7c6u, 0x6d5451fdu } },
    { { 0x243f6a88u, 0x85a308d3u, 0x13198a2eu, 0x03707344u }, { 0xa4093822u, 0x299f31d0u }, { 0xd16cfe09u, 0x94fdccebu, 0x5001e420u, 0x24126ea1u } },
    { { 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u }, { 0x00000000u, 0x00000000u }, { 0xf8e4cca4u, 0x5cb200dbu, 0xb1a574ebu, 0x097eff67u } },
    { { 0x00000000u, 0x00000000u, 0x00000000u, 0x00000001u }, { 0x00000000u, 0x00000000u }, { 0x2dce73e5u, 0x1348e23fu, 0xfcf8e0ecu, 0xa287aadbu } },
    { { 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u }, { 0x00000001u, 0x00000000u }, { 0xe3e80670u, 0xe50a0ebcu, 0x95f222c0u, 0xb615aa27u } },
    { { 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u }, { 0x00000000u, 0x00000001u }, { 0xfdde3e0bu, 0xfa7e58b6u, 0x3380ec46u, 0xd8d55c4fu } },
    { { 0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u }, { 0x80000000u, 0x80000000u }, { 0xd5975905u, 0xe0e51b81u, 0x85c6d8e7u, 0x312fb2d4u } },
    { { 0xffff0000u, 0x0000ffffu, 0xffff0000u, 0x0000ffffu }, { 0x0000ffffu, 0xffff0000u }, { 0x338d7681u, 0xd2d13cb4u, 0xa42b89a5u, 0x768b384eu } },
    { { 0x00000000u, 0x00000003u, 0x00000005u, 0x0000002au }, { 0x9e3779b9u, 0x85ebca6bu }, { 0x247e4bf2u, 0x91357688u, 0xe7e208fdu, 0x8f8f77f1u } },
    { { 0x000001ffu, 0x00000dfcu, 0x000019f8u, 0x0000002au }, { 0x9e377846u, 0x85ebca6bu }, { 0xad734692u, 0xbe146aa3u, 0xd8e6ebe2u, 0x446cd637u } },
    { { 0x00000000u, 0x00000000u, 0x00000000u, 0x00000001u }, { 0x85ebcbfau, 0xc2b2ae35u }, { 0x47acbd6fu, 0x5102c751u, 0x8b526c1du, 0x05387cdbu } },
    { { 0x00000000u, 0x00000000u, 0x00000000u, 0x00000002u }, { 0x0bd79547u, 0xc2b2ae35u }, { 0xd26d0ef1u, 0xf64ba294u, 0xb2ab6433u, 0x56113dfdu } },
    { { 0x00000000u, 0x00000000u, 0x00000000u, 0x00000002u }, { 0x0bd79547u, 0x85655c6au }, { 0x7a05ad0fu, 0xfaf58c1du, 0xee70c298u, 0x910e3a25u } },
    { { 0x00003e7fu, 0x00000001u, 0x00000000u, 0x00000003u }, { 0x91ddb503u, 0xc2b2ae35u }, { 0xc392329du, 0xdd3e4602u, 0xae18ac18u, 0x60b42eddu } },
    { { 0xffffffffu, 0xffffffffu, 0xffffffffu, 0x00000004u }, { 0xe850d653u, 0x3d4d51cbu }, { 0x62a79840u, 0x9f1207c4u, 0xd2adfbc9u, 0x57a0d2fcu } },
};

static bool eq4(const u32 a[4], const u32 b[4])
{
    return a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && a[3] == b[3];
}

static void test_known_answers(void)
{
    /* Random123 kat_vectors, philox4x32 with 10 rounds: all-zero input,
     * all-ones input, and the digits of pi */
    u32 out[4];
    const u32 c0[4] = { 0, 0, 0, 0 }, k0[2] = { 0, 0 };
    az_philox4x32(out, c0, k0);
    CHECK_EQ(out[0], 0x6627e8d5u); CHECK_EQ(out[1], 0xe169c58du);
    CHECK_EQ(out[2], 0xbc57ac4cu); CHECK_EQ(out[3], 0x9b00dbd8u);
    const u32 c1[4] = { 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu }, k1[2] = { 0xffffffffu, 0xffffffffu };
    az_philox4x32(out, c1, k1);
    CHECK_EQ(out[0], 0x408f276du); CHECK_EQ(out[1], 0x41c83b0eu);
    CHECK_EQ(out[2], 0xa20bc7c6u); CHECK_EQ(out[3], 0x6d5451fdu);
    const u32 c2[4] = { 0x243f6a88u, 0x85a308d3u, 0x13198a2eu, 0x03707344u }, k2[2] = { 0xa4093822u, 0x299f31d0u };
    az_philox4x32(out, c2, k2);
    CHECK_EQ(out[0], 0xd16cfe09u); CHECK_EQ(out[1], 0x94fdccebu);
    CHECK_EQ(out[2], 0x5001e420u); CHECK_EQ(out[3], 0x24126ea1u);
}

static void test_v40_vectors(void)
{
    for (int i = 0; i < 16; i++) {
        u32 out[4];
        az_philox4x32(out, V40[i].ctr, V40[i].key);
        for (int j = 0; j < 4; j++) CHECK_EQ(out[j], V40[i].out[j]);
        u32 one[4] = { 0, 0, 0, 0 };
        az_philox4x32_n(one, V40[i].ctr, V40[i].key, 1);
        CHECK(eq4(one, V40[i].out));
    }
}

/* ---- 3. structure of the reference ---- */
static int cmp_block(const void *a, const void *b) { return memcmp(a, b, 16); }

static u32 hamming4(const u32 a[4], const u32 b[4])
{
    u32 d = 0;
    for (int j = 0; j < 4; j++) d += (u32)__builtin_popcount(a[j] ^ b[j]);
    return d;
}

static void test_structure(void)
{
    enum { N = 4096 };
    static u32 ctr[4 * N], a[4 * N], b[4 * N];
    const u32 key[2] = { 0x9E3779B9u, 0x85EBCA6Bu };
    for (u32 i = 0; i < N; i++) {
        ctr[4 * i] = i; ctr[4 * i + 1] = 7; ctr[4 * i + 2] = AZ_RS_NOISE; ctr[4 * i + 3] = 0;
    }
    /* batch == single */
    az_philox4x32_n(a, ctr, key, N);
    bool same = true;
    for (u32 i = 0; i < N; i++) {
        u32 o[4];
        az_philox4x32(o, &ctr[4 * i], key);
        same = same && eq4(o, &a[4 * i]);
    }
    CHECK(same);
    /* in place */
    memcpy(b, ctr, sizeof b);
    az_philox4x32_n(b, b, key, N);
    CHECK(memcmp(a, b, sizeof a) == 0);
    u32 one[4] = { 5, 7, AZ_RS_NOISE, 0 }, ref1[4];
    az_philox4x32(ref1, one, key);
    az_philox4x32(one, one, key);
    CHECK(eq4(one, ref1));
    /* n = 0 writes nothing */
    u32 z[4] = { 1, 2, 3, 4 };
    az_philox4x32_n(z, ctr, key, 0);
    CHECK(z[0] == 1 && z[1] == 2 && z[2] == 3 && z[3] == 4);
    /* a bijection for a fixed key: N different counters, N different blocks */
    memcpy(b, a, sizeof b);
    qsort(b, N, 16, cmp_block);
    u32 dup = 0;
    for (u32 i = 1; i < N; i++) dup += memcmp(&b[4 * i], &b[4 * (i - 1)], 16) == 0;
    CHECK_EQ(dup, 0);
    /* every one of the 192 input bits matters: flipping it changes about
     * half of the 128 output bits (mean over 256 inputs; 8 sigma = 2.83) */
    for (u32 bit = 0; bit < 192; bit++) {
        u32 total = 0;
        for (u32 i = 0; i < 256; i++) {
            u32 in[6] = { i, 3, AZ_RS_MATE, 0, key[0], key[1] }, o0[4], o1[4];
            az_philox4x32(o0, in, &in[4]);
            in[bit >> 5] ^= 1u << (bit & 31);
            az_philox4x32(o1, in, &in[4]);
            total += hamming4(o0, o1);
        }
        CHECK(total > 256 * 61 && total < 256 * 67);
    }
}

/* ---- 4. a draw does not depend on other draws or on their order ---- */
static AzWorld tw;   /* az_rand4 reads only key and tick */

static void test_order_independence(void)
{
    enum { N = 2000 };
    static u32 fwd[N][4], shuf[N][4], perm[N];
    static u32 ctr[4 * N], pctr[4 * N], out[4 * N], pout[4 * N];
    tw.key[0] = 0xA4093822u; tw.key[1] = 0x299F31D0u; tw.tick = 12345;
    az_tick_keys(&tw);

    for (u32 id = 1; id <= N; id++) az_rand4(&tw, id, AZ_RS_MATE, 3, fwd[id - 1]);
    for (u32 i = 0; i < N; i++) perm[i] = i;
    for (u32 i = N - 1; i > 0; i--) {
        u32 j = tr_below(i + 1), t = perm[i];
        perm[i] = perm[j]; perm[j] = t;
    }
    /* shuffled order, with unrelated draws in between */
    u32 sink = 0;
    for (u32 k = 0; k < N; k++) {
        u32 id = perm[k] + 1, junk[4];
        az_rand4(&tw, tr_u32(), AZ_RS_NOISE, tr_u32(), junk);
        sink ^= junk[0] ^ az_rand(&tw, id, AZ_RS_CONTEST, 3) ^ az_rand(&tw, id + 1, AZ_RS_MATE, 4);
        az_rand4(&tw, id, AZ_RS_MATE, 3, shuf[id - 1]);
    }
    CHECK(memcmp(fwd, shuf, sizeof fwd) == 0);
    /* only some of the ids drawn */
    bool same = true;
    for (u32 id = N; id >= 1; id -= 3) {
        u32 o[4];
        az_rand4(&tw, id, AZ_RS_MATE, 3, o);
        same = same && eq4(o, fwd[id - 1]);
        if (id < 3) break;
    }
    CHECK(same);
    (void)sink;

    /* the batch form: permuting the counters permutes the blocks */
    for (u32 i = 0; i < N; i++) {
        ctr[4 * i] = i + 1; ctr[4 * i + 1] = tw.tick; ctr[4 * i + 2] = AZ_RS_MATE; ctr[4 * i + 3] = 3;
    }
    for (u32 k = 0; k < N; k++) memcpy(&pctr[4 * k], &ctr[4 * perm[k]], 16);
    az_philox4x32_n(out, ctr, tw.key, N);
    az_philox4x32_n(pout, pctr, tw.key, N);
    CHECK(memcmp(out, fwd, sizeof fwd) == 0);
    same = true;
    for (u32 k = 0; k < N; k++) same = same && eq4(&pout[4 * k], &out[4 * perm[k]]);
    CHECK(same);
    /* and so does cutting the batch at different places */
    memset(pout, 0, sizeof pout);
    for (u32 done = 0; done < N;) {
        u32 n = 1 + tr_below(37);
        if (n > N - done) n = N - done;
        az_philox4x32_n(&pout[4 * done], &ctr[4 * done], tw.key, n);
        done += n;
    }
    CHECK(memcmp(out, pout, sizeof out) == 0);

    /* id, tick, stream, index and seed each select a different block */
    u32 base[4], o[4];
    az_rand4(&tw, 77, AZ_RS_MATE, 3, base);
    az_rand4(&tw, 78, AZ_RS_MATE, 3, o);      CHECK(!eq4(o, base));
    az_rand4(&tw, 77, AZ_RS_MEIOSIS, 3, o);   CHECK(!eq4(o, base));
    az_rand4(&tw, 77, AZ_RS_MATE, 4, o);      CHECK(!eq4(o, base));
    tw.tick++;
    az_rand4(&tw, 77, AZ_RS_MATE, 3, o);      CHECK(!eq4(o, base));
    tw.tick--;
    tw.key[1] ^= 1; az_tick_keys(&tw);
    az_rand4(&tw, 77, AZ_RS_MATE, 3, o);      CHECK(!eq4(o, base));
    tw.key[1] ^= 1; az_tick_keys(&tw);
    az_rand4(&tw, 77, AZ_RS_MATE, 3, o);      CHECK(eq4(o, base));
    /* 64-bit ids, indices and ticks: the high halves select other blocks too,
     * and no two of them cancel */
    {
        u32 h1[4], h2[4], h3[4], h4[4];
        az_rand4(&tw, 77ull | (1ull << 32), AZ_RS_MATE, 3, h1);        CHECK(!eq4(h1, base));
        az_rand4(&tw, 77, AZ_RS_MATE, 3ull | (1ull << 32), h2);        CHECK(!eq4(h2, base));
        CHECK(!eq4(h1, h2));
        tw.tick += 1ull << 32; az_tick_keys(&tw);
        az_rand4(&tw, 77, AZ_RS_MATE, 3, h3);                          CHECK(!eq4(h3, base));
        CHECK(!eq4(h3, h1)); CHECK(!eq4(h3, h2));
        az_rand4(&tw, 77ull | (1ull << 32), AZ_RS_MATE, 3, h4);        CHECK(!eq4(h4, base));
        CHECK(!eq4(h4, h1)); CHECK(!eq4(h4, h3));
        CHECK(az_is_initiator(&tw, 77) == az_is_initiator(&tw, 77));   /* a pure function */
        tw.tick -= 1ull << 32; az_tick_keys(&tw);
        az_rand4(&tw, 77, AZ_RS_MATE, 3, o);                           CHECK(eq4(o, base));
        /* the role hash: about half of the ids above 2^32 initiate, and the
         * roles of an id and of the id 2^32 above it are not tied together */
        u32 init = 0, same_role = 0;
        for (u64 id = 1; id <= 4096; id++) {
            const bool hi = az_is_initiator(&tw, id | (5ull << 32));
            init += hi ? 1u : 0u;
            same_role += hi == az_is_initiator(&tw, id) ? 1u : 0u;
        }
        CHECK(init > 1848 && init < 2248);
        CHECK(same_role > 1848 && same_role < 2248);
    }
    /* az_rand is word 0 of az_rand4; az_rand_below stays below its bound */
    CHECK_EQ(az_rand(&tw, 77, AZ_RS_MATE, 3), base[0]);
    bool in_range = true;
    u32 hist[7] = { 0 };
    for (u32 id = 1; id <= 7000; id++) {
        u32 v = az_rand_below(&tw, id, AZ_RS_ROLE, 0, 7);
        in_range = in_range && v < 7;
        if (v < 7) hist[v]++;
    }
    CHECK(in_range);
    for (int v = 0; v < 7; v++) CHECK(hist[v] > 800 && hist[v] < 1200);   /* expect 1000, sd 29 */
    CHECK_EQ(az_rand_below(&tw, 1, AZ_RS_ROLE, 0, 1), 0);
}

#ifdef AZ_HAVE_PHILOX_AVX2
/* ---- 5. twin == reference, n = 0..40, guard bytes ---- */
enum { GUARD = 64, SMALL_MAX = 40, SMALL_BYTES = GUARD + 16 + 16 * SMALL_MAX + GUARD };

static void fill_words(u32 *p, u32 n_words)
{
    static const u32 edge[8] = { 0, 1, 0xFFFFFFFFu, 0x80000000u, 0x7FFFFFFFu, 0xFFFF0000u, 0x0000FFFFu, 2 };
    for (u32 i = 0; i < n_words; i++) {
        u32 r = tr_u32();
        p[i] = (r & 7) == 0 ? edge[(r >> 3) & 7] : tr_u32();   /* one word in eight is an edge value */
    }
}

static bool all_bytes(const u8 *p, size_t n, u8 v)
{
    for (size_t i = 0; i < n; i++) if (p[i] != v) return false;
    return true;
}

static void test_twin_small(void)
{
    static _Alignas(64) u8 cbuf[SMALL_BYTES], csnap[SMALL_BYTES], obuf[SMALL_BYTES];
    static u32 ref[4 * SMALL_MAX];
    for (u32 off = 0; off < 16; off += 4) {
        for (u32 n = 0; n <= SMALL_MAX; n++) {
            u32 key[2] = { tr_u32(), tr_u32() };
            const u32 key0 = key[0], key1 = key[1];
            const size_t bytes = 16 * (size_t)n;
            memset(cbuf, 0xC5, sizeof cbuf);
            memset(obuf, 0x5A, sizeof obuf);
            u32 *ctr = (u32 *)(void *)(cbuf + GUARD + off);
            u32 *out = (u32 *)(void *)(obuf + GUARD + off);
            fill_words(ctr, 4 * n);
            memcpy(csnap, cbuf, sizeof cbuf);
            az_philox4x32_n(ref, ctr, key, n);

            az_philox_n_avx2(out, ctr, key, n);
            CHECK(memcmp(out, ref, bytes) == 0);
            CHECK(all_bytes(obuf, GUARD + off, 0x5A));
            CHECK(all_bytes(obuf + GUARD + off + bytes, sizeof obuf - (GUARD + off + bytes), 0x5A));
            CHECK(memcmp(cbuf, csnap, sizeof cbuf) == 0);      /* counters and their guards */
            CHECK(key[0] == key0 && key[1] == key1);

            az_philox_n_avx2(ctr, ctr, key, n);                /* in place */
            CHECK(memcmp(ctr, ref, bytes) == 0);
            CHECK(memcmp(cbuf, csnap, GUARD + off) == 0);
            CHECK(memcmp(cbuf + GUARD + off + bytes, csnap + GUARD + off + bytes,
                         sizeof cbuf - (GUARD + off + bytes)) == 0);
        }
    }
}

/* ---- 6. twin with the arrays flush against inaccessible pages ---- */
enum { PAGE = 4096 };
static volatile LONG tr_fault_armed = 0;

static LONG CALLBACK tr_fault_handler(EXCEPTION_POINTERS *ep)
{
    if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && tr_fault_armed) {
        printf("FAIL %s: access violation %s address %p while the twin ran: it touched a byte outside its arrays\n",
               __FILE__, ep->ExceptionRecord->ExceptionInformation[0] ? "writing" : "reading",
               (void *)ep->ExceptionRecord->ExceptionInformation[1]);
        printf("FAILED: %d checks, %d failures\n", az_checks, az_failures + 1);
        fflush(stdout);
        ExitProcess(3);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

/* three pages; the middle one is usable, its neighbours are PAGE_NOACCESS */
static u8 *fenced_page(void)
{
    u8 *p = (u8 *)VirtualAlloc(NULL, 3 * PAGE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    DWORD old;
    if (!p) return NULL;
    if (!VirtualProtect(p, PAGE, PAGE_NOACCESS, &old)) return NULL;
    if (!VirtualProtect(p + 2 * PAGE, PAGE, PAGE_NOACCESS, &old)) return NULL;
    return p + PAGE;
}

static bool is_noaccess(const u8 *p)
{
    MEMORY_BASIC_INFORMATION mbi;
    return VirtualQuery(p, &mbi, sizeof mbi) == sizeof mbi && mbi.Protect == PAGE_NOACCESS;
}

static void test_twin_fenced(void)
{
    static u32 ref[4 * SMALL_MAX];
    u8 *cp = fenced_page(), *op = fenced_page();
    CHECK(cp != NULL && op != NULL);
    if (!cp || !op) return;
    /* the fences are real: the bytes just outside the usable page are inaccessible */
    CHECK(is_noaccess(cp - 1) && is_noaccess(cp + PAGE));
    CHECK(is_noaccess(op - 1) && is_noaccess(op + PAGE));
    PVOID h = AddVectoredExceptionHandler(1, tr_fault_handler);
    CHECK(h != NULL);
    for (u32 n = 0; n <= SMALL_MAX; n++) {
        const size_t bytes = 16 * (size_t)n;
        u32 key[2] = { tr_u32(), tr_u32() };
        for (int side = 0; side < 4; side++) {
            /* side bit 0: counters end at the fence (else start at it); bit 1: same for out */
            u32 *ctr = (u32 *)(void *)((side & 1) ? cp + PAGE - bytes : cp);
            u32 *out = (u32 *)(void *)((side & 2) ? op + PAGE - bytes : op);
            fill_words(ctr, 4 * n);
            az_philox4x32_n(ref, ctr, key, n);
            InterlockedExchange(&tr_fault_armed, 1);
            az_philox_n_avx2(out, ctr, key, n);
            InterlockedExchange(&tr_fault_armed, 0);
            CHECK(memcmp(out, ref, bytes) == 0);
            InterlockedExchange(&tr_fault_armed, 1);
            az_philox_n_avx2(ctr, ctr, key, n);                /* in place, fenced */
            InterlockedExchange(&tr_fault_armed, 0);
            CHECK(memcmp(ctr, ref, bytes) == 0);
        }
    }
    /* the key too: its 8 bytes end at a fence, then start at one */
    {
        u32 ctr[4 * 9], out[4 * 9];
        fill_words(ctr, 4 * 9);
        for (int side = 0; side < 2; side++) {
            u32 *key = (u32 *)(void *)(side ? cp + PAGE - 8 : cp);
            key[0] = tr_u32(); key[1] = tr_u32();
            az_philox4x32_n(ref, ctr, key, 9);
            InterlockedExchange(&tr_fault_armed, 1);
            az_philox_n_avx2(out, ctr, key, 9);
            InterlockedExchange(&tr_fault_armed, 0);
            CHECK(memcmp(out, ref, sizeof out) == 0);
        }
    }
    RemoveVectoredExceptionHandler(h);
    VirtualFree(cp - PAGE, 0, MEM_RELEASE);
    VirtualFree(op - PAGE, 0, MEM_RELEASE);
}

/* ---- 7. twin == reference, 1,000,000 random blocks ---- */
static void test_twin_million(void)
{
    enum { CHUNK = 8192, TOTAL = 1000000, SHAPED = 250000 };
    static u32 ctr[4 * CHUNK], a[4 * CHUNK], b[4 * CHUNK];
    /* pass 0: random counters and keys (one word in eight an edge value);
     * pass 1: the simulation's shape, { id, tick, stream, index } with consecutive ids */
    for (int pass = 0; pass < 2; pass++) {
        const u32 total = pass ? SHAPED : TOTAL;
        u32 done = 0, chunks = 0, bad_chunks = 0;
        while (done < total) {
            u32 n = 1 + tr_below(CHUNK);
            if (n > total - done) n = total - done;
            u32 key[2];
            fill_words(key, 2);
            if (pass == 0) {
                fill_words(ctr, 4 * n);
            } else {
                u32 id0 = tr_u32(), tick = tr_u32() >> 8, stream = 1 + tr_below(27), idx = tr_below(4);
                for (u32 i = 0; i < n; i++) {
                    ctr[4 * i] = id0 + i; ctr[4 * i + 1] = tick; ctr[4 * i + 2] = stream; ctr[4 * i + 3] = idx;
                }
            }
            az_philox4x32_n(a, ctr, key, n);
            memset(b, 0xEE, 16 * (size_t)n);
            az_philox_n_avx2(b, ctr, key, n);
            if (memcmp(a, b, 16 * (size_t)n) != 0) bad_chunks++;
            done += n;
            chunks++;
        }
        CHECK_EQ(done, total);
        CHECK_EQ(bad_chunks, 0);
        printf("twin vs reference: %u %s blocks in %u calls, %u calls differ\n", done,
               pass ? "simulation-shaped" : "random", chunks, bad_chunks);
    }
}
/* ---- 8. callee-saved registers survive the twin ---- */
/* rng_abi_call loads sentinels into every callee-saved register (Microsoft
 * x64: rbx rbp rsi rdi r12..r15 and xmm6..xmm15), calls fn(arg0..arg3) and
 * records what the registers hold afterwards. Written here, in the test, so
 * that the kernel object contains nothing but the kernel. */
typedef struct RngAbiFrame {
    u64 arg[4];         /*   0 */
    u64 gpr_in[8];      /*  32  rbx rbp rsi rdi r12 r13 r14 r15 */
    u64 xmm_in[20];     /*  96  xmm6..xmm15 */
    u64 gpr_out[8];     /* 256 */
    u64 xmm_out[20];    /* 320 */
    u64 rsp_before;     /* 480 */
    u64 rsp_after;      /* 488 */
} RngAbiFrame;
void rng_abi_call(void *fn, RngAbiFrame *f);
void rng_abi_bad(u32 *out4n, const u32 *ctr4n, const u32 key[2], u32 n);   /* negative control */

__asm__(
    ".text\n"
    ".intel_syntax noprefix\n"
    ".globl rng_abi_call\n"
    ".p2align 4\n"
    "rng_abi_call:\n"
    "    push rbx\n    push rbp\n    push rsi\n    push rdi\n"
    "    push r12\n    push r13\n    push r14\n    push r15\n"
    "    sub rsp, 216\n"                 /* 32 shadow, 16 frame pointer slot, 160 xmm, 8 alignment */
    "    movups [rsp+48], xmm6\n    movups [rsp+64], xmm7\n    movups [rsp+80], xmm8\n"
    "    movups [rsp+96], xmm9\n    movups [rsp+112], xmm10\n    movups [rsp+128], xmm11\n"
    "    movups [rsp+144], xmm12\n    movups [rsp+160], xmm13\n    movups [rsp+176], xmm14\n"
    "    movups [rsp+192], xmm15\n"
    "    mov [rsp+32], rdx\n"
    "    mov rax, rcx\n"
    "    mov r10, rdx\n"
    "    mov rbx, [r10+32]\n    mov rbp, [r10+40]\n    mov rsi, [r10+48]\n    mov rdi, [r10+56]\n"
    "    mov r12, [r10+64]\n    mov r13, [r10+72]\n    mov r14, [r10+80]\n    mov r15, [r10+88]\n"
    "    movups xmm6, [r10+96]\n    movups xmm7, [r10+112]\n    movups xmm8, [r10+128]\n"
    "    movups xmm9, [r10+144]\n    movups xmm10, [r10+160]\n    movups xmm11, [r10+176]\n"
    "    movups xmm12, [r10+192]\n    movups xmm13, [r10+208]\n    movups xmm14, [r10+224]\n"
    "    movups xmm15, [r10+240]\n"
    "    mov [r10+480], rsp\n"
    "    mov rcx, [r10]\n    mov rdx, [r10+8]\n    mov r8, [r10+16]\n    mov r9, [r10+24]\n"
    "    call rax\n"
    "    mov r10, [rsp+32]\n"
    "    mov [r10+488], rsp\n"
    "    mov [r10+256], rbx\n    mov [r10+264], rbp\n    mov [r10+272], rsi\n    mov [r10+280], rdi\n"
    "    mov [r10+288], r12\n    mov [r10+296], r13\n    mov [r10+304], r14\n    mov [r10+312], r15\n"
    "    movups [r10+320], xmm6\n    movups [r10+336], xmm7\n    movups [r10+352], xmm8\n"
    "    movups [r10+368], xmm9\n    movups [r10+384], xmm10\n    movups [r10+400], xmm11\n"
    "    movups [r10+416], xmm12\n    movups [r10+432], xmm13\n    movups [r10+448], xmm14\n"
    "    movups [r10+464], xmm15\n"
    "    movups xmm6, [rsp+48]\n    movups xmm7, [rsp+64]\n    movups xmm8, [rsp+80]\n"
    "    movups xmm9, [rsp+96]\n    movups xmm10, [rsp+112]\n    movups xmm11, [rsp+128]\n"
    "    movups xmm12, [rsp+144]\n    movups xmm13, [rsp+160]\n    movups xmm14, [rsp+176]\n"
    "    movups xmm15, [rsp+192]\n"
    "    add rsp, 216\n"
    "    pop r15\n    pop r14\n    pop r13\n    pop r12\n"
    "    pop rdi\n    pop rsi\n    pop rbp\n    pop rbx\n"
    "    ret\n"
    ".globl rng_abi_bad\n"
    ".p2align 4\n"
    "rng_abi_bad:\n"                     /* breaks the ABI on purpose: r12 and xmm9 */
    "    xor r12d, r12d\n"
    "    pxor xmm9, xmm9\n"
    "    ret\n"
    ".att_syntax prefix\n"
);

/* bit mask of registers that came back changed: bits 0..7 the GPRs in frame
 * order, bits 8..17 xmm6..xmm15, bit 18 rsp */
static u32 abi_changed(void *fn, u32 *out, const u32 *ctr, const u32 *key, u32 n)
{
    static RngAbiFrame f;
    _Static_assert(sizeof(RngAbiFrame) == 496, "frame layout is fixed by the assembly above");
    f.arg[0] = (u64)(uintptr_t)out; f.arg[1] = (u64)(uintptr_t)ctr;
    f.arg[2] = (u64)(uintptr_t)key; f.arg[3] = 0xDEADBEEF00000000ull | n;   /* junk above the u32 argument */
    for (int i = 0; i < 8; i++) f.gpr_in[i] = 0x1111111111111101ull * (u64)(i + 1) + 0xF0;
    for (int i = 0; i < 20; i++) f.xmm_in[i] = 0xA0B0C0D0E0F00000ull + (u64)i * 0x0101010101ull;
    memset(f.gpr_out, 0, sizeof f.gpr_out);
    memset(f.xmm_out, 0, sizeof f.xmm_out);
    f.rsp_before = 1; f.rsp_after = 2;
    rng_abi_call(fn, &f);
    u32 mask = 0;
    for (int i = 0; i < 8; i++) if (f.gpr_out[i] != f.gpr_in[i]) mask |= 1u << i;
    for (int i = 0; i < 10; i++)
        if (f.xmm_out[2 * i] != f.xmm_in[2 * i] || f.xmm_out[2 * i + 1] != f.xmm_in[2 * i + 1]) mask |= 1u << (8 + i);
    if (f.rsp_before != f.rsp_after) mask |= 1u << 18;
    return mask;
}

static void test_twin_abi(void)
{
    static const u32 sizes[] = { 0, 1, 2, 7, 8, 9, 15, 16, 17, 40, 1000 };
    static u32 ctr[4 * 1000], out[4 * 1000], ref[4 * 1000];
    const u32 key[2] = { 0x13198A2Eu, 0x03707344u };
    fill_words(ctr, 4 * 1000);
    for (u32 s = 0; s < sizeof sizes / sizeof sizes[0]; s++) {
        u32 n = sizes[s];
        memset(out, 0, sizeof out);
        CHECK_EQ(abi_changed((void *)az_philox_n_avx2, out, ctr, key, n), 0);
        /* the call through the checker also computed the right blocks, and
         * ignored the junk in the upper half of the n register */
        az_philox4x32_n(ref, ctr, key, n);
        CHECK(memcmp(out, ref, 16 * (size_t)n) == 0);
        CHECK(all_bytes((const u8 *)out + 16 * (size_t)n, sizeof out - 16 * (size_t)n, 0));
    }
    /* negative control: the checker does see a broken callee (r12 = bit 4, xmm9 = bit 11) */
    CHECK_EQ(abi_changed((void *)rng_abi_bad, out, ctr, key, 8), (1u << 4) | (1u << 11));
    /* the twin carries unwind records that cover it from its first byte */
    DWORD64 base = 0;
    PRUNTIME_FUNCTION rf = RtlLookupFunctionEntry((DWORD64)(uintptr_t)az_philox_n_avx2, &base, NULL);
    CHECK(rf != NULL);
    if (rf) CHECK_EQ(base + rf->BeginAddress, (DWORD64)(uintptr_t)az_philox_n_avx2);
}

/* ---- 9. dispatch installs the twin ---- */
static void test_install(void)
{
    static u32 ctr[4 * 100], a[4 * 100], b[4 * 100];
    tw.key[0] = 0x85A308D3u; tw.key[1] = 0x243F6A88u;
    az_tick_keys(&tw);
    u32 got = az_kernels_install(&tw, AZ_K_PHILOX_AVX2);
    CHECK(got & AZ_K_PHILOX_AVX2);
    CHECK_EQ(tw.k.asm_mask, got);
    CHECK(tw.k.philox_n == az_philox_n_avx2);
    if (tw.k.philox_n) {
        fill_words(ctr, 4 * 100);
        tw.k.philox_n(a, ctr, tw.key, 100);
        az_philox4x32_n(b, ctr, tw.key, 100);
        CHECK(memcmp(a, b, sizeof a) == 0);
    }
    got = az_kernels_install(&tw, 0);
    CHECK(!(got & AZ_K_PHILOX_AVX2));
    CHECK(tw.k.philox_n != az_philox_n_avx2);
}
#endif /* AZ_HAVE_PHILOX_AVX2 */

int main(void)
{
    test_known_answers();
    test_v40_vectors();
    test_structure();
    test_order_independence();
#ifdef AZ_HAVE_PHILOX_AVX2
    if (az_cpu_features() & AZ_CPU_AVX2) {
        test_twin_small();
        test_twin_fenced();
        test_twin_million();
        test_twin_abi();
        test_install();
    } else {
        printf("note: this CPU has no AVX2; the twin was not run\n");
    }
#else
    printf("note: built without the philox_avx2 twin; only the reference was tested\n");
#endif
    return az_test_done();
}
