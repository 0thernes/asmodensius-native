// test.c - correctness and timing harness for kern.asm
//   gcc -std=c2x -O2 -Wall -Wextra test.c kern.obj -o test.exe
//   test.exe            all tests + timing on one P core and one E core
//   test.exe notime     tests only
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cpuid.h>
#include <x86intrin.h>

// ---------------------------------------------------------------- asm side
uint32_t cpu_features(void);
void     cpuid_raw(uint32_t leaf, uint32_t subleaf, uint32_t out[4]);
typedef void nn_fn(const uint8_t *x, const int8_t *W, const int32_t *b, uint32_t shift, uint8_t *y);
nn_fn nn_mv32x64_vnni_c4x4, nn_mv32x64_vnni_c4x8, nn_mv32x64_vnni_c4x12;
nn_fn nn_mv32x64_avx2_c4, nn_mv32x64_vnni_rm, nn_mv32x64_int8_c4x8;
uint32_t morton3_pdep(uint32_t x, uint32_t y, uint32_t z);
uint32_t morton3_pext_x(uint32_t code, uint32_t axis);
void     morton3_pdep_batch(const uint16_t *x, const uint16_t *y, const uint16_t *z, uint32_t *out, size_t n);
uint32_t abi_check(void (*fn)(void));
void     clobber_all(void), clobber_odd(void), clobber_min(void), clobber_even(void), clobber_xmm(void);
void     clobber_bad(void);
extern uint64_t abi_align_fault;
extern void (*clobber_callback)(void);
uint64_t spin_add8(uint64_t n);

enum { F_OSYMM = 1, F_AVX = 2, F_AVX2 = 4, F_FMA = 8, F_BMI1 = 16, F_BMI2 = 32,
       F_AVXVNNI = 64, F_VNNIINT8 = 128, F_VNNIINT16 = 256, F_AVX512F = 512 };

#define NOUT 32
#define NIN  64

// ---------------------------------------------------------------- scalar references
// Contract: x[c] in 0..127, W s8, |b[r]| <= 2^30, shift 0..31.
static void nn_scalar(const uint8_t *x, const int8_t *Wrm, const int32_t *b, uint32_t shift, uint8_t *y)
{
    for (int r = 0; r < NOUT; r++) {
        int32_t acc = b[r];
        for (int c = 0; c < NIN; c++)
            acc += (int32_t)Wrm[r * NIN + c] * (int32_t)x[c];
        acc >>= shift;                       // arithmetic shift (gcc, clang, msvc on x64)
        y[r] = (uint8_t)(acc < 0 ? 0 : acc > 127 ? 127 : acc);
    }
}

static void pack_c4(const int8_t *Wrm, int8_t *Wc4)
{
    for (int r = 0; r < NOUT; r++)
        for (int c = 0; c < NIN; c++)
            Wc4[((c / 4) * 4 + r / 8) * 32 + (r % 8) * 4 + c % 4] = Wrm[r * NIN + c];
#ifdef MUTATE   // negative control: swap two packed weights; the tests must now fail
    int8_t t = Wc4[1237];  Wc4[1237] = Wc4[1238];  Wc4[1238] = t;
#endif
}

static uint32_t morton3_scalar(uint32_t x, uint32_t y, uint32_t z)
{
    uint32_t m = 0;
    for (int i = 0; i < 10; i++) {
        m |= ((x >> i) & 1u) << (3 * i);
        m |= ((y >> i) & 1u) << (3 * i + 1);
        m |= ((z >> i) & 1u) << (3 * i + 2);
    }
    return m;
}

// ---------------------------------------------------------------- rng (test data only)
static uint64_t rng_s = 0x9E3779B97F4A7C15ull;
static uint64_t rnd(void)
{
    uint64_t z = (rng_s += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

// ---------------------------------------------------------------- kernel table
typedef struct { const char *name; nn_fn *fn; int layout; uint32_t need; } variant;   // layout 0 = c4, 1 = row-major
static const variant variants[] = {
    { "vnni_c4x4",  nn_mv32x64_vnni_c4x4,  0, F_AVX2 | F_AVXVNNI },
    { "vnni_c4x8",  nn_mv32x64_vnni_c4x8,  0, F_AVX2 | F_AVXVNNI },
    { "vnni_c4x12", nn_mv32x64_vnni_c4x12, 0, F_AVX2 | F_AVXVNNI },
    { "avx2_c4",    nn_mv32x64_avx2_c4,    0, F_AVX2 },
    { "vnni_rm",    nn_mv32x64_vnni_rm,    1, F_AVX2 | F_AVXVNNI },
    { "int8_c4x8",  nn_mv32x64_int8_c4x8,  0, F_AVX2 | F_VNNIINT8 },
};
#define NVAR ((int)(sizeof variants / sizeof variants[0]))

static _Alignas(64) int8_t  Wrm[NOUT * NIN], Wc4[NOUT * NIN];
static _Alignas(64) int32_t bias[NOUT];
static _Alignas(64) uint8_t xin[NIN], yref[NOUT], yout[NOUT + 32];

static long long n_cases, n_fail;
static long long hist_out[3];                     // outputs equal 0, strictly inside, equal 127

static void run_case(uint32_t shift, uint32_t feats, const char *what)
{
    pack_c4(Wrm, Wc4);
    nn_scalar(xin, Wrm, bias, shift, yref);
    for (int r = 0; r < NOUT; r++) hist_out[yref[r] == 0 ? 0 : yref[r] == 127 ? 2 : 1]++;
    for (int v = 0; v < NVAR; v++) {
        if ((feats & variants[v].need) != variants[v].need) continue;
        memset(yout, 0xEE, sizeof yout);
        variants[v].fn(xin, variants[v].layout ? Wrm : Wc4, bias, shift, yout);
        int bad = memcmp(yout, yref, NOUT) != 0;
        for (int i = NOUT; i < NOUT + 32; i++) bad |= yout[i] != 0xEE;   // wrote past y[32]?
        if (bad && n_fail++ < 5) {
            printf("  MISMATCH %s in %s shift=%u\n   ref:", variants[v].name, what, shift);
            for (int r = 0; r < NOUT; r++) printf(" %u", yref[r]);
            printf("\n   got:");
            for (int r = 0; r < NOUT; r++) printf(" %u", yout[r]);
            printf("\n");
        }
    }
    n_cases++;
}

static void fill_const(int xv, int wv, int32_t bv)
{
    memset(xin, xv, sizeof xin);
    memset(Wrm, wv, sizeof Wrm);
    for (int r = 0; r < NOUT; r++) bias[r] = bv;
}

static void test_kernels(uint32_t feats)
{
    // ---- edge cases
    fill_const(127, -128, 0);              run_case(0, feats, "x=127 w=-128");       // sum -1040384 -> 0
    fill_const(127, -128, 1040384);        run_case(0, feats, "x=127 w=-128 b=+1040384");   // exactly 0
    for (int r = 0; r < NOUT; r++) bias[r] = 1040384 + r * 4;                        // exact 0,4,..,124
    run_case(0, feats, "x=127 w=-128 ramp");
    for (int r = 0; r < NOUT; r++) bias[r] = 1040384 + r * 5;                        // crosses 127
    run_case(0, feats, "x=127 w=-128 ramp over 127");
    fill_const(127, 127, 0);               run_case(13, feats, "x=127 w=127 >>13"); // 1032256>>13 = 126
    fill_const(127, 127, 0);               run_case(0, feats, "x=127 w=127 >>0");   // clamps to 127
    fill_const(127, 127, -1032256);        run_case(0, feats, "x=127 w=127 b=-sum");
    fill_const(0, -128, 77);               run_case(0, feats, "x=0");
    fill_const(1, -1, 63);                 run_case(0, feats, "x=1 w=-1");          // 63-64 = -1 -> 0
    fill_const(1, 1, -65);                 run_case(0, feats, "x=1 w=1");           // 64-65 = -1 -> 0
    fill_const(127, -128, -(1 << 30));     run_case(31, feats, "b=-2^30 >>31");
    fill_const(127, 127, (1 << 30));       run_case(31, feats, "b=+2^30 >>31");
    fill_const(127, 127, (1 << 30));       run_case(24, feats, "b=+2^30 >>24");
    // alternating extremes inside each 4-byte group and each pair (vpmaddubsw pair sums)
    for (int k = 0; k < 16; k++) {
        for (int c = 0; c < NIN; c++) xin[c] = ((k >> (c & 3)) & 1) ? 127 : 0;
        for (int i = 0; i < NOUT * NIN; i++) Wrm[i] = ((i * 7 + k) & 1) ? -128 : 127;
        for (int r = 0; r < NOUT; r++) bias[r] = (int32_t)(rnd() % 4096) - 2048;
        run_case(k & 7, feats, "alternating");
    }
    // one-hot: a single weight and a single input select every (row, column) once
    for (int r0 = 0; r0 < NOUT; r0++)
        for (int c0 = 0; c0 < NIN; c0++) {
            memset(xin, 0, sizeof xin);  memset(Wrm, 0, sizeof Wrm);
            for (int r = 0; r < NOUT; r++) bias[r] = 0;
            xin[c0] = 1;  Wrm[r0 * NIN + c0] = (int8_t)(1 + (r0 * 64 + c0) % 127);
            // decoy weights that must not leak into other rows or columns
            Wrm[((r0 + 1) % NOUT) * NIN + (c0 + 1) % NIN] = 99;
            run_case(0, feats, "one-hot");
        }
    long long n_edge = n_cases;

    // ---- 100000 random inputs
    //  mode A (even): random bias and shift, output mostly clamps
    //  mode B (odd):  bias chosen from the scalar sum so every output lands in
    //                 0..127; with shift 0 the output byte equals the exact sum
    //                 offset, so any wrong product or carry is visible
    for (int t = 0; t < 100000; t++) {
        int wmode = t % 5;
        for (int i = 0; i < NOUT * NIN; i++) {
            int8_t w = (int8_t)(rnd() & 0xFF);
            if (wmode == 3) w = (rnd() & 1) ? -128 : 127;
            if (wmode == 4) w = (int8_t)((int)(rnd() % 17) - 8);
            Wrm[i] = w;
        }
        int xmode = (t / 5) % 4;
        for (int c = 0; c < NIN; c++) {
            uint8_t v = (uint8_t)(rnd() & 127);
            if (xmode == 1) v = (rnd() & 1) ? 127 : 0;
            if (xmode == 2) v = (rnd() % 8) ? 0 : v;         // sparse input
            if (xmode == 3) v = 127;
            xin[c] = v;
        }
        uint32_t shift;
        if (t & 1) {
            shift = (t % 6 == 1) ? 0 : (uint32_t)(rnd() % 13);
            for (int r = 0; r < NOUT; r++) {
                int32_t s = 0;
                for (int c = 0; c < NIN; c++) s += (int32_t)Wrm[r * NIN + c] * xin[c];
                bias[r] = -s + (int32_t)(rnd() % (128u << shift));
            }
        } else {
            shift = (uint32_t)(rnd() % 20);
            for (int r = 0; r < NOUT; r++) bias[r] = (int32_t)(rnd() % (1u << 21)) - (1 << 20);
        }
        run_case(shift, feats, "random");
    }
    printf("kernels: %lld cases (%lld edge + %lld random) x %d variants vs scalar C: %lld mismatches  %s\n",
           n_cases, n_edge, n_cases - n_edge, NVAR, n_fail, n_fail ? "FAIL" : "PASS");
    printf("  reference output mix: %lld zero, %lld inside 1..126, %lld at 127\n",
           hist_out[0], hist_out[1], hist_out[2]);

    // ---- informational: the contract x <= 127 is what keeps the AVX2 path exact
    fill_const(255, 127, 0);
    for (int r = 0; r < NOUT; r++) bias[r] = -2072640 + 64;      // exact answer 64 everywhere
    pack_c4(Wrm, Wc4);
    nn_scalar(xin, Wrm, bias, 0, yref);
    uint8_t yv[NOUT], ya[NOUT];
    nn_mv32x64_vnni_c4x8(xin, Wc4, bias, 0, yv);
    nn_mv32x64_avx2_c4(xin, Wc4, bias, 0, ya);
    printf("  out-of-contract x=255 w=127: scalar y[0]=%u, vnni y[0]=%u (%s), avx2 y[0]=%u (%s: vpmaddubsw saturates)\n",
           yref[0], yv[0], memcmp(yv, yref, NOUT) ? "differs" : "same", ya[0],
           memcmp(ya, yref, NOUT) ? "differs, expected" : "same");
}

// ---------------------------------------------------------------- Morton
static int test_morton(void)
{
    long long bad = 0, n = 0;
    for (int axis = 0; axis < 3; axis++)
        for (uint32_t v = 0; v < 1024; v++)
            for (int bg = 0; bg < 2; bg++) {                 // other two axes all 0 / all 1
                uint32_t c[3] = { bg ? 1023 : 0, bg ? 1023 : 0, bg ? 1023 : 0 };
                c[axis] = v;
                uint32_t m = morton3_pdep(c[0], c[1], c[2]);
                bad += m != morton3_scalar(c[0], c[1], c[2]);
                for (uint32_t a = 0; a < 3; a++) bad += morton3_pext_x(m, a) != c[a];
                n++;
            }
    for (int t = 0; t < 10000000; t++) {
        uint64_t r = rnd();
        uint32_t x = (uint32_t)r & 1023, y = (uint32_t)(r >> 10) & 1023, z = (uint32_t)(r >> 20) & 1023;
        uint32_t m = morton3_pdep(x, y, z);
        bad += m != morton3_scalar(x, y, z);
        if ((t & 15) == 0) { bad += morton3_pext_x(m, 0) != x; bad += morton3_pext_x(m, 1) != y; bad += morton3_pext_x(m, 2) != z; }
        n++;
    }
    // bits above 10 must be ignored
    bad += morton3_pdep(0xFFFFFC00u, 0xFFFFFC00u, 0xFFFFFC00u) != 0;
    bad += morton3_pdep(0xFFFFFFFFu, 0, 0) != 0x09249249u;
    bad += morton3_pdep(0, 0xFFFFFFFFu, 0) != 0x12492492u;
    bad += morton3_pdep(0, 0, 0xFFFFFFFFu) != 0x24924924u;
    // order property: Morton order of the 8 corners of a 2x2x2 cell
    bad += morton3_pdep(1, 0, 0) != 1; bad += morton3_pdep(0, 1, 0) != 2; bad += morton3_pdep(0, 0, 1) != 4;
    // batch
    enum { NB = 4099 };
    static uint16_t bx[NB], by[NB], bz[NB]; static uint32_t bo[NB + 1];
    for (int i = 0; i < NB; i++) { uint64_t r = rnd(); bx[i] = r & 1023; by[i] = (r >> 10) & 1023; bz[i] = (r >> 20) & 1023; }
    bo[NB] = 0xDEADBEEF;
    morton3_pdep_batch(bx, by, bz, bo, NB);
    for (int i = 0; i < NB; i++) bad += bo[i] != morton3_scalar(bx[i], by[i], bz[i]);
    bad += bo[NB] != 0xDEADBEEF;
    morton3_pdep_batch(bx, by, bz, bo, 0);                    // n = 0 must be a no-op
    n += NB;
    printf("morton3: %lld encodes vs scalar bit loop, pext round trip, batch of %d: %lld mismatches  %s\n",
           n, NB, bad, bad ? "FAIL" : "PASS");
    return bad != 0;
}

// ---------------------------------------------------------------- ABI
static int test_abi(void)
{
    struct { const char *n; void (*f)(void); } t[] = {
        { "clobber_all  (8 gpr, 10 xmm, 32 locals)", clobber_all },
        { "clobber_odd  (3 gpr, 1 xmm, 32 locals)", clobber_odd },
        { "clobber_min  (1 gpr)", clobber_min },
        { "clobber_even (2 gpr, 3 xmm, 48 locals)", clobber_even },
        { "clobber_xmm  (0 gpr, 10 xmm, 32 locals)", clobber_xmm },
    };
    int bad = 0;
    for (int i = 0; i < 5; i++) {
        uint32_t m = abi_check(t[i].f);
        printf("abi: %-42s changed-register mask = 0x%05x  %s\n", t[i].n, m, m ? "FAIL" : "PASS");
        bad |= m != 0;
    }
    // negative control: a function that destroys r12 and xmm9 without saving them
    uint32_t mb = abi_check(clobber_bad);
    printf("abi: clobber_bad  (control: trashes r12, xmm9)     changed-register mask = 0x%05x  %s\n",
           mb, mb == 0x810 ? "PASS (detected, expected 0x00810)" : "FAIL");
    bad |= mb != 0x810;
    printf("abi: stack misaligned inside a PROC body: %llu times  %s\n",
           (unsigned long long)abi_align_fault, abi_align_fault ? "FAIL" : "PASS");
    return bad || abi_align_fault;
}

// ---------------------------------------------------------------- unwind records
// A callback invoked from inside a clobber_* body walks the stack with the
// Windows unwinder (RtlLookupFunctionEntry + RtlVirtualUnwind), which reads
// the .pdata/.xdata records that ENDPROC emitted.  After unwinding out of the
// clobber_* frame the context must hold abi_check's sentinels in every
// callee-saved register, although the live registers were destroyed.
static const uint64_t sent_gpr[8] = {
    0x1111111111111101ull, 0x2222222222222202ull, 0x3333333333333303ull, 0x4444444444444404ull,
    0x5555555555555505ull, 0x6666666666666606ull, 0x7777777777777707ull, 0x8888888888888808ull };
static void (*walk_expect_fn)(void);
static int walk_ran, walk_bad;

static int unwind_step(CONTEXT *ctx, DWORD64 *fn_start)
{
    DWORD64 base = 0;  void *hd = NULL;  DWORD64 ef = 0;
    PRUNTIME_FUNCTION fe = RtlLookupFunctionEntry(ctx->Rip, &base, NULL);
    if (!fe) { *fn_start = 0; return 0; }
    *fn_start = base + fe->BeginAddress;
    RtlVirtualUnwind(UNW_FLAG_NHANDLER, base, ctx->Rip, fe, ctx, &hd, &ef, NULL);
    return 1;
}

__attribute__((noinline)) static void cb_walk(void)
{
    CONTEXT ctx;  DWORD64 fs = 0;
    RtlCaptureContext(&ctx);
    walk_ran++;
    if (!unwind_step(&ctx, &fs)) { walk_bad |= 1; return; }             // out of cb_walk
    if (!unwind_step(&ctx, &fs)) { walk_bad |= 2; return; }             // out of clobber_*
    if (fs != (DWORD64)(uintptr_t)walk_expect_fn) walk_bad |= 4;         // record found for the right function
    const DWORD64 g[8] = { ctx.Rbx, ctx.Rbp, ctx.Rsi, ctx.Rdi, ctx.R12, ctx.R13, ctx.R14, ctx.R15 };
    for (int i = 0; i < 8; i++) if (g[i] != sent_gpr[i]) walk_bad |= 8;
    const M128A *x = &ctx.Xmm6;
    for (int i = 0; i < 10; i++)
        if (x[i].Low != 0xA0A0A0A0A0A0A000ull + (unsigned)i || (uint64_t)x[i].High != 0x0B0B0B0B0B0B0B00ull + (unsigned)i) walk_bad |= 16;
    if (!unwind_step(&ctx, &fs)) { walk_bad |= 32; return; }            // out of abi_check
    if (fs != (DWORD64)(uintptr_t)abi_check) walk_bad |= 64;
    if (!RtlLookupFunctionEntry(ctx.Rip, &fs, NULL)) walk_bad |= 128;    // back in C code
}

static int test_unwind(void)
{
    struct { const char *n; void (*f)(void); } t[] = {
        { "clobber_all", clobber_all }, { "clobber_odd", clobber_odd },
        { "clobber_even", clobber_even }, { "clobber_xmm", clobber_xmm } };
    int bad = 0;
    clobber_callback = cb_walk;
    for (int i = 0; i < 4; i++) {
        walk_expect_fn = t[i].f;  walk_ran = 0;  walk_bad = 0;
        uint32_t m = abi_check(t[i].f);
        int ok = walk_ran == 1 && walk_bad == 0 && m == 0;
        printf("unwind: RtlVirtualUnwind through %-12s -> abi_check: flags 0x%02x, ran %d  %s\n",
               t[i].n, walk_bad, walk_ran, ok ? "PASS (all 8 GPR + 10 XMM recovered)" : "FAIL");
        bad |= !ok;
    }
    clobber_callback = NULL;
    return bad;
}

// ---------------------------------------------------------------- features
__attribute__((target("xsave")))
static int test_features(uint32_t f)
{
    unsigned a = 0, b = 0, c = 0, d = 0, a71 = 0, d71 = 0, b7 = 0, c1 = 0;
    __get_cpuid_count(1, 0, &a, &b, &c1, &d);
    if (__get_cpuid_max(0, 0) >= 7) {
        __get_cpuid_count(7, 0, &a, &b7, &c, &d);
        if (a >= 1) __get_cpuid_count(7, 1, &a71, &b, &c, &d71);
    }
    uint32_t raw[4];  cpuid_raw(7, 1, raw);
    int osymm = ((c1 >> 27) & 1) && ((_xgetbv(0) & 6) == 6);
    uint32_t want = 0;
    if (osymm) want |= F_OSYMM;
    if (osymm && ((c1 >> 28) & 1)) {
        want |= F_AVX;
        if ((c1 >> 12) & 1) want |= F_FMA;
        if ((b7 >> 5) & 1) want |= F_AVX2;
        if ((a71 >> 4) & 1) want |= F_AVXVNNI;
        if ((d71 >> 4) & 1) want |= F_VNNIINT8;
        if ((d71 >> 10) & 1) want |= F_VNNIINT16;
    }
    if ((b7 >> 3) & 1) want |= F_BMI1;
    if ((b7 >> 8) & 1) want |= F_BMI2;
    if ((b7 >> 16) & 1) want |= F_AVX512F;
    char brand[49] = { 0 };
    for (uint32_t i = 0; i < 3; i++) cpuid_raw(0x80000002u + i, 0, (uint32_t *)(brand + 16 * i));
    printf("cpu: %s\n", brand);
    printf("cpu_features() = 0x%03x  OSYMM=%d AVX=%d AVX2=%d FMA=%d BMI1=%d BMI2=%d AVX-VNNI=%d VNNI-INT8=%d VNNI-INT16=%d AVX512F=%d\n",
           f, !!(f & F_OSYMM), !!(f & F_AVX), !!(f & F_AVX2), !!(f & F_FMA), !!(f & F_BMI1), !!(f & F_BMI2),
           !!(f & F_AVXVNNI), !!(f & F_VNNIINT8), !!(f & F_VNNIINT16), !!(f & F_AVX512F));
    int ok = f == want && raw[0] == a71 && raw[3] == d71;
    printf("cpu: asm mask vs gcc <cpuid.h> + _xgetbv: 0x%03x vs 0x%03x; gcc builtin avx2=%d fma=%d bmi2=%d avxvnni=%d  %s\n",
           f, want, !!__builtin_cpu_supports("avx2"), !!__builtin_cpu_supports("fma"),
           !!__builtin_cpu_supports("bmi2"), !!__builtin_cpu_supports("avxvnni"), ok ? "PASS" : "FAIL");
    return !ok;
}

// ---------------------------------------------------------------- timing
static inline uint64_t tsc(void) { _mm_lfence(); uint64_t t = __rdtsc(); _mm_lfence(); return t; }
static double qpc_s(void)
{
    LARGE_INTEGER c, f;  QueryPerformanceCounter(&c);  QueryPerformanceFrequency(&f);
    return (double)c.QuadPart / (double)f.QuadPart;
}
static int cmp_u64(const void *a, const void *b)
{
    uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
    return x < y ? -1 : x > y;
}

#define BATCH 512
#define NBATCH 4000
static uint64_t samples[NBATCH];

typedef struct { double mn, med, p90; } stat;
static stat summarize(double per)
{
    qsort(samples, NBATCH, sizeof samples[0], cmp_u64);
    return (stat){ samples[0] / per, samples[NBATCH / 2] / per, samples[NBATCH * 9 / 10] / per };
}

static _Alignas(64) uint8_t tx[8][NIN], ty[8][NOUT + 32];

static stat time_thr(const variant *v)     // independent calls: throughput
{
    const int8_t *W = v->layout ? Wrm : Wc4;
    for (int b = 0; b < NBATCH; b++) {
        uint64_t t0 = tsc();
        for (int i = 0; i < BATCH; i++) v->fn(tx[i & 7], W, bias, 9, ty[i & 7]);
        samples[b] = tsc() - t0;
    }
    return summarize(BATCH);
}
static stat time_lat(const variant *v)     // each output feeds the next input: latency
{
    const int8_t *W = v->layout ? Wrm : Wc4;
    static _Alignas(64) uint8_t buf[NIN + 32];
    for (int c = 0; c < NIN; c++) buf[c] = (uint8_t)(c * 2);
    for (int b = 0; b < NBATCH; b++) {
        uint64_t t0 = tsc();
        for (int i = 0; i < BATCH; i++) { v->fn(buf, W, bias, 9, buf); v->fn(buf, W, bias, 9, buf + 32); }
        samples[b] = tsc() - t0;
    }
    return summarize(2.0 * BATCH);
}
static stat time_scalar(void)
{
    for (int b = 0; b < NBATCH; b++) {
        uint64_t t0 = tsc();
        for (int i = 0; i < 16; i++) nn_scalar(tx[i & 7], Wrm, bias, 9, ty[i & 7]);
        samples[b] = tsc() - t0;
    }
    return summarize(16);
}

enum { MB = 4096 };
static uint16_t mx[MB], my[MB], mz[MB];
static uint32_t mo[MB];
static stat time_morton_batch(void)
{
    for (int b = 0; b < NBATCH; b++) {
        uint64_t t0 = tsc();
        morton3_pdep_batch(mx, my, mz, mo, MB);
        samples[b] = tsc() - t0;
    }
    return summarize(MB);
}
static stat time_morton_scalar(void)
{
    volatile uint32_t sink = 0;
    for (int b = 0; b < NBATCH; b++) {
        uint64_t t0 = tsc();
        uint32_t s = 0;
        for (int i = 0; i < 256; i++) s += morton3_scalar(mx[i], my[i], mz[i]);
        samples[b] = tsc() - t0;
        sink = sink + s;
    }
    (void)sink;
    return summarize(256);
}

// core cycles per TSC tick right now: best (least disturbed) of 5 short runs of the add chain
static double cyc_per_tick(void)
{
    double best = 0;
    for (int k = 0; k < 5; k++) {
        uint64_t a = tsc();  spin_add8(1000 * 1000);  uint64_t b = tsc();
        double ratio = 8e6 / (double)(b - a);
        if (ratio > best) best = ratio;
    }
    return best;
}

static void timing_on_cpu(int cpu, uint32_t feats)
{
    if (!SetThreadAffinityMask(GetCurrentThread(), (DWORD_PTR)1 << cpu)) { printf("cannot pin to cpu %d\n", cpu); return; }
    Sleep(1);
    uint32_t r[4];  cpuid_raw(0x1A, 0, r);
    unsigned type = r[0] >> 24;
    const char *tn = type == 0x40 ? "P core (Intel Core)" : type == 0x20 ? "E core (Intel Atom)" : "unknown type";

    // clocks: TSC rate from QPC, core clock from the 8-cycle add chain
    double w0 = qpc_s();  uint64_t c0 = tsc();
    spin_add8(40u * 1000 * 1000);                              // 320 M core cycles
    uint64_t c1 = tsc();  double w1 = qpc_s();
    double best_ratio = 0, tsc_hz = (double)(c1 - c0) / (w1 - w0);
    for (int k = 0; k < 7; k++) {                              // best of 7: least disturbed run
        uint64_t a = tsc();  spin_add8(20u * 1000 * 1000);  uint64_t b = tsc();
        double ratio = 160e6 / (double)(b - a);                // core cycles per TSC tick
        if (ratio > best_ratio) best_ratio = ratio;
    }
    printf("\n== timing on logical cpu %d: %s   [noisy, parallel load]\n", cpu, tn);
    printf("   TSC %.1f MHz (vs QPC); core clock ~ %.2f cycles per TSC tick (~%.0f MHz, from an 8-cycle add chain)\n",
           tsc_hz / 1e6, best_ratio, tsc_hz * best_ratio / 1e6);
    printf("   %d batches per row; a batch is %d calls; figures are per call\n", NBATCH, BATCH);
    printf("   the core clock is re-measured before and after every row (column cyc/tick)\n");
    printf("   %-12s %-11s %9s %9s %9s %9s %11s %8s\n", "kernel", "mode", "min tick", "med tick", "p90 tick", "cyc/tick", "min cycles", "min ns");
    for (int v = 0; v < NVAR + 3; v++) {
        if (v < NVAR && (feats & variants[v].need) != variants[v].need) continue;
        for (int m = 0; m < (v < NVAR ? 2 : 1); m++) {
            double q = cyc_per_tick();
            stat s = v == NVAR ? time_scalar() : v == NVAR + 1 ? time_morton_batch() : v == NVAR + 2 ? time_morton_scalar()
                   : m ? time_lat(&variants[v]) : time_thr(&variants[v]);
            double q2 = cyc_per_tick();
            if (q2 > q) q = q2;
            const char *nm = v == NVAR ? "scalar C -O2" : v == NVAR + 1 ? "morton pdep" : v == NVAR + 2 ? "morton C" : variants[v].name;
            printf("   %-12s %-11s %9.2f %9.2f %9.2f %9.3f %11.2f %8.3f%s\n", nm, m ? "latency" : "throughput",
                   s.mn, s.med, s.p90, q, s.mn * q, s.mn / tsc_hz * 1e9,
                   v == NVAR + 1 ? "   per encode, batch of 4096" : v == NVAR + 2 ? "   per encode, C bit loop" : "");
        }
    }
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    uint32_t feats = cpu_features();
    int fail = test_features(feats);
    fail |= test_abi();
    fail |= test_unwind();
    if (!(feats & F_BMI2)) { printf("BMI2 missing: stop\n"); return 2; }
    fail |= test_morton();
    if (!(feats & F_AVX2)) { printf("AVX2 missing: stop\n"); return 2; }
    test_kernels(feats);
    fail |= n_fail != 0;
    printf("RESULT: %s\n", fail ? "FAIL" : "ALL PASS");
    if (argc > 1 && !strcmp(argv[1], "notime")) return fail;

    // timing data
    for (int i = 0; i < NOUT * NIN; i++) Wrm[i] = (int8_t)(rnd() & 0xFF);
    pack_c4(Wrm, Wc4);
    for (int r = 0; r < NOUT; r++) bias[r] = (int32_t)(rnd() % 8192);
    for (int k = 0; k < 8; k++) for (int c = 0; c < NIN; c++) tx[k][c] = (uint8_t)(rnd() & 127);
    for (int i = 0; i < MB; i++) { uint64_t q = rnd(); mx[i] = q & 1023; my[i] = (q >> 10) & 1023; mz[i] = (q >> 20) & 1023; }

    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    SYSTEM_INFO si;  GetSystemInfo(&si);
    int pcpu = -1, ecpu = -1;
    if (argc > 2) { pcpu = atoi(argv[1]); ecpu = atoi(argv[2]); }
    else for (int c = 0; c < (int)si.dwNumberOfProcessors && c < 64; c++) {
        if (!SetThreadAffinityMask(GetCurrentThread(), (DWORD_PTR)1 << c)) continue;
        Sleep(0);
        uint32_t r[4];  cpuid_raw(0x1A, 0, r);
        if ((r[0] >> 24) == 0x40 && pcpu < 0) pcpu = c;
        if ((r[0] >> 24) == 0x20 && ecpu < 0) ecpu = c;
    }
    if (pcpu >= 0) timing_on_cpu(pcpu, feats);
    if (ecpu >= 0) timing_on_cpu(ecpu, feats);
    return fail;
}
