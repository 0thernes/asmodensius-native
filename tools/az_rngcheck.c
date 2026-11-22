/* az_rngcheck.c - a sanity check of the simulation's random numbers
 * (Philox4x32-10, core/az_rng.c and its AVX2 twin).
 *
 * THIS IS A SANITY CHECK, NOT A STATISTICAL BATTERY. It looks at 2^20 blocks
 * with three simple statistics and catches gross mistakes: a wrong constant,
 * a missing round, a stuck or biased bit, a twin that disagrees with the
 * reference. It cannot certify a generator; that is the job of TestU01
 * BigCrush or PractRand, which Philox4x32-10 passed in the paper by Salmon
 * et al. (SC11) and which are not run here.
 *
 *   az_rngcheck [seed] [--control]
 *     seed       64-bit number (decimal or 0x...), default 1; the Philox key
 *     --control  run the same statistics on a deliberately weak 2-round
 *                variant, to show that they can fail
 *
 * Inputs are the simulation's own shape, which is the hard case for a
 * counter-based generator because the counters barely differ:
 *     ctr = { id = 0..2^20-1, tick = 1000, stream = AZ_RS_NOISE, index = 0 }
 *
 *   monobit     fraction of one bits among all 2^27 output bits, and the
 *               worst of the 128 output bit positions
 *   byte chi2   chi-square of the 2^24 output bytes over 256 values, 255 df
 *   avalanche   flip each of the 192 input bits (128 counter + 64 key) in
 *               every block: each of the 128 output bits should flip in half
 *               of the blocks (192 x 128 cells, 2^20 samples per cell).
 *               Flipping one of the low 20 id bits maps the id range onto
 *               itself, so for those 20 input bits every pair of counters
 *               is seen twice: 2^19 independent samples, each counted
 *               twice. Their standard deviation is taken as sqrt(2) times
 *               larger, which is exact.
 *
 * Exit code 0 when every line says ok (with --control: when at least one
 * line says FLAG, as it must).
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../core/az_abi.h"
#include "../kernels/az_kernels.h"

enum { LOG2_BLOCKS = 20, N_BLOCKS = 1 << LOG2_BLOCKS, BATCH = 4096, VC_PLANES = LOG2_BLOCKS + 1 };

typedef void (*GenFn)(u32 *out4n, const u32 *ctr4n, const u32 key[2], u32 n);

/* the control: Philox4x32 cut down to 2 rounds (never used by the simulation) */
static void weak_philox_n(u32 *out4n, const u32 *ctr4n, const u32 key[2], u32 n)
{
    for (u32 i = 0; i < n; i++) {
        u32 c0 = ctr4n[4 * i], c1 = ctr4n[4 * i + 1], c2 = ctr4n[4 * i + 2], c3 = ctr4n[4 * i + 3];
        u32 k0 = key[0], k1 = key[1];
        for (int r = 0; r < 2; r++) {
            u64 p0 = (u64)0xD2511F53u * c0, p1 = (u64)0xCD9E8D57u * c2;
            u32 n0 = (u32)(p1 >> 32) ^ c1 ^ k0, n2 = (u32)(p0 >> 32) ^ c3 ^ k1;
            c0 = n0; c1 = (u32)p1; c2 = n2; c3 = (u32)p0;
            k0 += 0x9E3779B9u; k1 += 0xBB67AE85u;
        }
        out4n[4 * i] = c0; out4n[4 * i + 1] = c1; out4n[4 * i + 2] = c2; out4n[4 * i + 3] = c3;
    }
}

/* 128 counters, one per output bit, kept as bit planes (plane k holds bit k
 * of every counter), so adding a 128-bit vector costs a few word operations */
typedef struct { u64 p[VC_PLANES][2]; } VCount;

static inline void vc_add(VCount *v, u64 x0, u64 x1)
{
    for (int k = 0; k < VC_PLANES && (x0 | x1); k++) {
        u64 c0 = v->p[k][0] & x0, c1 = v->p[k][1] & x1;
        v->p[k][0] ^= x0; v->p[k][1] ^= x1;
        x0 = c0; x1 = c1;
    }
}

static u32 vc_get(const VCount *v, int bit)
{
    u32 c = 0;
    for (int k = 0; k < VC_PLANES; k++) c |= (u32)((v->p[k][bit >> 6] >> (bit & 63)) & 1) << k;
    return c;
}

static double now_s(void)
{
    LARGE_INTEGER f, t;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&t);
    return (double)t.QuadPart / (double)f.QuadPart;
}

static int n_flag = 0;
static const char *verdict(double z, double limit)
{
    if (fabs(z) < limit) return "ok";
    n_flag++;
    return "FLAG";
}

static u32 ctr[4 * BATCH], ctr2[4 * BATCH], out[4 * BATCH], out2[4 * BATCH];
static VCount ones, aval[192];
static u64 hist[256];

int main(int argc, char **argv)
{
    u64 seed = 1;
    bool control = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--control") == 0) control = true;
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("usage: az_rngcheck [seed] [--control]\n"
                   "  a sanity check of Philox4x32-10 over 2^20 blocks: monobit, byte chi-square, avalanche.\n"
                   "  Not a statistical battery. --control runs a weak 2-round variant that must be flagged.\n");
            return 0;
        } else {
            char *end;
            seed = strtoull(argv[i], &end, 0);
            if (*end) { printf("az_rngcheck: bad argument '%s' (try --help)\n", argv[i]); return 2; }
        }
    }
    const u32 key[2] = { (u32)seed, (u32)(seed >> 32) };
    GenFn ref = control ? weak_philox_n : az_philox4x32_n;
    GenFn gen = ref;
    bool have_twin = false;
#ifdef AZ_HAVE_PHILOX_AVX2
    have_twin = !control && (az_cpu_features() & AZ_CPU_AVX2) != 0;
#endif

    printf("az_rngcheck: %s\n", control ? "CONTROL, Philox4x32 cut to 2 rounds (expected to be flagged)"
                                        : "Philox4x32-10 (core/az_rng.c)");
    printf("A sanity check, not a statistical battery: it catches gross mistakes and certifies nothing.\n");
    printf("inputs: %d blocks, ctr = {id 0..%d, tick 1000, stream %d, index 0}, key = {0x%08x, 0x%08x}\n\n",
           N_BLOCKS, N_BLOCKS - 1, (int)AZ_RS_NOISE, key[0], key[1]);

    /* pass 1: reference output; monobit and byte histogram; twin comparison and timing */
    u64 twin_diff = 0;
    double t_ref = 0, t_twin = 0;
    for (u32 b = 0; b < N_BLOCKS; b += BATCH) {
        for (u32 i = 0; i < BATCH; i++) {
            ctr[4 * i] = b + i; ctr[4 * i + 1] = 1000; ctr[4 * i + 2] = AZ_RS_NOISE; ctr[4 * i + 3] = 0;
        }
        double t0 = now_s();
        ref(out, ctr, key, BATCH);
        t_ref += now_s() - t0;
#ifdef AZ_HAVE_PHILOX_AVX2
        if (have_twin) {
            t0 = now_s();
            az_philox_n_avx2(out2, ctr, key, BATCH);
            t_twin += now_s() - t0;
            for (u32 i = 0; i < BATCH; i++) twin_diff += memcmp(&out[4 * i], &out2[4 * i], 16) != 0;
        }
#endif
        for (u32 i = 0; i < BATCH; i++)
            vc_add(&ones, out[4 * i] | (u64)out[4 * i + 1] << 32, out[4 * i + 2] | (u64)out[4 * i + 3] << 32);
        const u8 *bytes = (const u8 *)out;
        for (u32 i = 0; i < 16 * BATCH; i++) hist[bytes[i]]++;
    }
#ifdef AZ_HAVE_PHILOX_AVX2
    if (have_twin && twin_diff == 0) gen = az_philox_n_avx2;   /* identical, so use the faster one below */
#endif
    if (have_twin) {
        printf("AVX2 twin vs C reference: %llu of %d blocks differ  %s\n", (unsigned long long)twin_diff, N_BLOCKS,
               twin_diff ? "FLAG" : "ok");
        if (twin_diff) n_flag++;
        printf("time per block (noisy, parallel load; one run, batches of %d): C %.2f ns, twin %.2f ns\n\n",
               BATCH, 1e9 * t_ref / N_BLOCKS, 1e9 * t_twin / N_BLOCKS);
    } else {
        printf("AVX2 twin: not compared (%s)\n\n", control ? "control run" : "not built, or no AVX2 on this CPU");
    }

    /* pass 2: avalanche. Flip one input bit in every block and count, per
     * output bit, in how many blocks it changed. */
    for (u32 b = 0; b < N_BLOCKS; b += BATCH) {
        for (u32 i = 0; i < BATCH; i++) {
            ctr[4 * i] = b + i; ctr[4 * i + 1] = 1000; ctr[4 * i + 2] = AZ_RS_NOISE; ctr[4 * i + 3] = 0;
        }
        gen(out, ctr, key, BATCH);
        for (u32 bit = 0; bit < 192; bit++) {
            if (bit < 128) {
                memcpy(ctr2, ctr, sizeof ctr);
                const u32 word = bit >> 5, mask = 1u << (bit & 31);
                for (u32 i = 0; i < BATCH; i++) ctr2[4 * i + word] ^= mask;
                gen(out2, ctr2, key, BATCH);
            } else {
                u32 key2[2] = { key[0], key[1] };
                key2[(bit - 128) >> 5] ^= 1u << (bit & 31);
                gen(out2, ctr, key2, BATCH);
            }
            VCount *v = &aval[bit];
            for (u32 i = 0; i < BATCH; i++)
                vc_add(v, (out[4 * i] ^ out2[4 * i]) | (u64)(out[4 * i + 1] ^ out2[4 * i + 1]) << 32,
                          (out[4 * i + 2] ^ out2[4 * i + 2]) | (u64)(out[4 * i + 3] ^ out2[4 * i + 3]) << 32);
        }
    }

    /* ---- the table ---- */
    const double n = (double)N_BLOCKS;
    printf("%-34s %-22s %-18s %12s  %s\n", "statistic", "observed", "expected", "z", "verdict");

    /* monobit */
    u64 total_ones = 0;
    double worst_bit_z = 0, worst_bit_frac = 0.5;
    int worst_bit = 0;
    for (int j = 0; j < 128; j++) {
        u32 c = vc_get(&ones, j);
        total_ones += c;
        double z = ((double)c - n / 2) / sqrt(n / 4);
        if (fabs(z) > fabs(worst_bit_z)) { worst_bit_z = z; worst_bit = j; worst_bit_frac = (double)c / n; }
    }
    char obs[64], exp_[64];
    double z = ((double)total_ones - 64 * n) / sqrt(32 * n);
    snprintf(obs, sizeof obs, "%.8f", (double)total_ones / (128 * n));
    printf("%-34s %-22s %-18s %+12.2f  %s\n", "monobit fraction, all bits", obs, "0.5", z, verdict(z, 5.0));
    snprintf(obs, sizeof obs, "%.6f (bit %d)", worst_bit_frac, worst_bit);
    printf("%-34s %-22s %-18s %+12.2f  %s\n", "monobit, worst of 128 positions", obs, "0.5, |z| near 2.7",
           worst_bit_z, verdict(worst_bit_z, 5.5));

    /* byte chi-square */
    double chi2 = 0;
    const double e = 16 * n / 256;
    for (int v = 0; v < 256; v++) chi2 += ((double)hist[v] - e) * ((double)hist[v] - e) / e;
    z = (chi2 - 255) / sqrt(2.0 * 255);
    snprintf(obs, sizeof obs, "%.2f", chi2);
    printf("%-34s %-22s %-18s %+12.2f  %s\n", "byte chi-square, 255 df", obs, "255 +- 22.6", z, verdict(z, 5.0));

    /* avalanche */
    double cell_min = 1, cell_max = 0, cell_worst = 0, in_min = 1, in_max = 0, in_worst = 0, grand = 0;
    int in_min_bit = 0, in_max_bit = 0;
    for (int bit = 0; bit < 192; bit++) {
        /* low id bits: the flip maps ids 0..2^20-1 onto themselves, every pair is counted twice */
        const double sd_scale = bit < LOG2_BLOCKS ? sqrt(2.0) : 1.0;
        u64 sum = 0;
        for (int j = 0; j < 128; j++) {
            u32 c = vc_get(&aval[bit], j);
            sum += c;
            double f = (double)c / n, zc = ((double)c - n / 2) / (sd_scale * sqrt(n / 4));
            if (f < cell_min) cell_min = f;
            if (f > cell_max) cell_max = f;
            if (fabs(zc) > fabs(cell_worst)) cell_worst = zc;
        }
        double f = (double)sum / (128 * n), zi = ((double)sum - 64 * n) / (sd_scale * sqrt(32 * n));
        grand += f / 192;
        if (f < in_min) { in_min = f; in_min_bit = bit; }
        if (f > in_max) { in_max = f; in_max_bit = bit; }
        if (fabs(zi) > fabs(in_worst)) in_worst = zi;
    }
    /* variance of the grand mean: 172 input bits with factor 1, 20 with factor 2 */
    z = (grand - 0.5) / (0.5 / sqrt(192.0 * 128 * n) * sqrt((192.0 + LOG2_BLOCKS) / 192.0));
    snprintf(obs, sizeof obs, "%.8f", grand);
    printf("%-34s %-22s %-18s %+12.2f  %s\n", "avalanche, mean flip fraction", obs, "0.5", z, verdict(z, 5.0));
    snprintf(obs, sizeof obs, "%.5f..%.5f", in_min, in_max);
    printf("%-34s %-22s %-18s %+12.2f  %s\n", "avalanche, per input bit (192)", obs, "0.5, |z| near 2.8",
           in_worst, verdict(in_worst, 5.5));
    snprintf(obs, sizeof obs, "%.5f..%.5f", cell_min, cell_max);
    snprintf(exp_, sizeof exp_, "0.5, |z| near 4.1");
    printf("%-34s %-22s %-18s %+12.2f  %s\n", "avalanche, per cell (192 x 128)", obs, exp_,
           cell_worst, verdict(cell_worst, 6.5));
    printf("\n  input bits: 0..127 = counter words {id, tick, stream, index}, 128..191 = key; per input bit the\n"
           "  lowest flip fraction is at bit %d and the highest at bit %d. z is the distance from the expected\n"
           "  value in standard deviations; the 'worst of many' lines show the largest of 128, 192 or 24576\n"
           "  such values, where a few standard deviations are normal. The z of the per-input-bit and\n"
           "  mean-flip lines assumes the 128 output bits flip independently. Input bits 0..%d (low id bits)\n"
           "  see every counter pair twice; their z uses the matching sqrt(2) larger standard deviation.\n",
           in_min_bit, in_max_bit, LOG2_BLOCKS - 1);

    if (control) {
        printf("\ncontrol: %d line(s) flagged; %s\n", n_flag,
               n_flag ? "the statistics do detect a weak generator" : "NOTHING flagged, the check has no power");
        return n_flag ? 0 : 1;
    }
    printf("\nresult: %d line(s) flagged. %s\n", n_flag,
           n_flag ? "Something is grossly wrong, or this seed is a rare outlier: run another seed."
                  : "No gross defect seen. This is a sanity check, not a statistical battery.");
    return n_flag ? 1 : 0;
}
