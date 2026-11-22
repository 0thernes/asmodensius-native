/* az_brainbench.c - nanoseconds per brain step for the C reference and the
 * AVX2 and AVX-VNNI twins, on one P core and one E core, while cycling
 * through 1, 16, 256 and 4096 distinct clades (the weight tiles) to show the
 * cache footprint.
 *
 *   az_brainbench [--pcpu 0] [--ecpu 2] [--steps 16384] [--batches 40]
 *
 * On the Core Ultra 9 275HX the P cores are logical processors 0, 1, 10..13,
 * 22, 23; every other logical processor is an E core. The tool prints what
 * CPUID leaf 0x1A says about the core it actually ran on.
 *
 * Method: the thread is pinned with SetThreadAffinityMask. 64 entity records
 * (sens, hid, gain, bias, motor; 7 KB in total, so they stay in L1) are
 * stepped round-robin; step i uses clade perm[i], a fixed pseudo-random
 * sequence over the n clades, because in the simulation entities are visited
 * in cell order and their clades come in no particular order. One batch is
 * `steps` steps timed with QueryPerformanceCounter; the first batch warms
 * the caches and is not counted; the table gives the minimum and the median
 * over the remaining batches. Timings are NOISY when other work shares the
 * machine: the minimum is the least-disturbed batch.
 */
#include <stdio.h>
#include "az_dense.h"
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "../core/az_abi.h"
#include "../kernels/az_kernels.h"
#include "az_tools.h"
#include "az_statsutil.h"

typedef void (*BrainFn)(const AzDenseNet *c, const u8 *sens, u8 *hid,
                        const u8 *gain, const i8 *bias, i8 *motor);

#define N_ENT 64
typedef struct Ent {
    u8 sens[AZ_DENSE_SENS];
    u8 hid[AZ_DENSE_HID];
    u8 gain[AZ_DENSE_MODG];
    i8 bias[AZ_DENSE_MODG];
    i8 motor[AZ_DENSE_MOT];
} Ent;

static u64 s_state = 0xB5297A4Dull;
static u64 s_rnd(void)
{
    s_state += 0x9E3779B97F4A7C15ull;
    return az_mix64(s_state);
}

static void ent_reset(Ent *e)
{
    s_state = 0xB5297A4Dull;                 /* the same inputs for every kernel */
    for (u32 i = 0; i < N_ENT; i++) {
        for (u32 j = 0; j < AZ_DENSE_SENS; j++) e[i].sens[j] = (u8)(s_rnd() & 127);
        memset(e[i].hid, 0, AZ_DENSE_HID);
        memset(e[i].gain, 64, AZ_DENSE_MODG);
        memset(e[i].bias, 0, AZ_DENSE_MODG);
        memset(e[i].motor, 0, AZ_DENSE_MOT);
    }
}

#define BB_NETS 4096u    /* dense networks in the largest working set */

/* returns a checksum of every hidden and motor byte after the run */
static u64 bench(BrainFn fn, const AzDenseNet *clades, const u32 *perm, u32 steps, u32 batches,
                 Ent *ent, double *min_ns, double *med_ns)
{
    LARGE_INTEGER f, t0, t1;
    QueryPerformanceFrequency(&f);
    double *ns = (double *)malloc(sizeof(double) * batches);
    ent_reset(ent);
    for (u32 b = 0; b <= batches; b++) {
        QueryPerformanceCounter(&t0);
        for (u32 i = 0; i < steps; i++) {
            Ent *e = &ent[i & (N_ENT - 1)];
            fn(&clades[perm[i]], e->sens, e->hid, e->gain, e->bias, e->motor);
        }
        QueryPerformanceCounter(&t1);
        if (b > 0) ns[b - 1] = (double)(t1.QuadPart - t0.QuadPart) * 1e9 / (double)f.QuadPart / (double)steps;
    }
    /* az_statsutil.h, type 7 */
    qsort(ns, batches, sizeof(double), azs_cmp_double);
    *min_ns = azs_quantile_sorted(ns, (int)batches, 0.0);
    *med_ns = azs_quantile_sorted(ns, (int)batches, 0.5);
    free(ns);
    u64 sum = 0;
    for (u32 i = 0; i < N_ENT; i++) {
        for (u32 j = 0; j < AZ_DENSE_HID; j++) sum = az_mix64(sum + ent[i].hid[j]);
        for (u32 j = 0; j < AZ_DENSE_MOT; j++) sum = az_mix64(sum + (u8)ent[i].motor[j]);
    }
    return sum;
}

int main(int argc, char **argv)
{
    /* strict options (az_tools.h): anything else is refused with exit code 2 */
    static const char *const with_value[] = { "--pcpu", "--ecpu", "--steps", "--batches", NULL };
    static const char *const flags[] = { "--help", "-h", NULL };
    bool help = false, help_h = false;
    u32 pcpu = 0, ecpu = 2, steps = 16384, batches = 40;
    if (!azt_known(argc, argv, 1, with_value, flags) ||
        !azt_flag(argc, argv, 1, "--help", &help) || !azt_flag(argc, argv, 1, "-h", &help_h))
        return 2;
    if (help || help_h) {
        printf("usage: az_brainbench [--pcpu 0] [--ecpu 2] [--steps 16384] [--batches 40]\n"
               "  --pcpu, --ecpu   logical processors 0 to 63    --steps 64 to 16777216    --batches 3 to 100000\n");
        return 0;
    }
    if (!azt_u32(argc, argv, 1, "--pcpu", 0, 63, &pcpu) ||          /* one bit of the affinity mask */
        !azt_u32(argc, argv, 1, "--ecpu", 0, 63, &ecpu) ||
        !azt_u32(argc, argv, 1, "--steps", N_ENT, 16777216, &steps) ||
        !azt_u32(argc, argv, 1, "--batches", 3, 100000, &batches))
        return 2;

    /* 4096 dense networks developed from random neural programs by the real
     * development code (az_dense_develop). This tool measures the dense batch
     * kernel, the one the GPU twin runs; the engine's own per-creature brain
     * step is measured in place by az_bench (stage "perceive"). */
    AzDenseNet *clades = (AzDenseNet *)_aligned_malloc((size_t)BB_NETS * sizeof(AzDenseNet), 64);
    Ent *ent = (Ent *)_aligned_malloc(sizeof(Ent) * N_ENT, 64);
    u32 *perm = (u32 *)malloc(sizeof(u32) * steps);
    if (!clades || !ent || !perm) { printf("out of memory\n"); return 2; }
    memset(clades, 0, (size_t)BB_NETS * sizeof(AzDenseNet));
    {
        u64 x = 42;                                  /* splitmix64: the nets are the same every run */
        for (u32 i = 0; i < BB_NETS; i++) {
            AzDenseNet *k = &clades[i];
            u8 *raw = (u8 *)k->ntype;
            for (u32 j = 0; j < sizeof k->ntype + sizeof k->theta; j++) {
                x += 0x9E3779B97F4A7C15ull;
                raw[j] = (u8)(az_mix64(x) >> 24);
            }
            for (u32 j = 0; j < AZ_DENSE_NEURONS; j++) k->ntype[j] &= 7;
            for (u32 j = 0; j < 64; j++) k->theta[j] = (i8)((i32)((u8)k->theta[j] % 97u) - 48);
            for (u32 j = 0; j < 8; j++) { x += 0x9E3779B97F4A7C15ull; k->tbias[j] = (i8)((i32)(az_mix64(x) & 31) - 16); }
            x += 0x9E3779B97F4A7C15ull;
            k->salt = (u16)az_mix64(x);
            k->var = (u8)(16 + ((az_mix64(x) >> 16) & 63));
            k->shift1 = 7; k->shift2 = 6; k->alive = 1;
            az_dense_develop(k);
        }
    }

    static const struct { BrainFn fn; const char *name; u32 need; } kern[3] = {
        { az_dense_step_c, "C reference", 0 },
#ifdef AZ_HAVE_BRAIN_VNNI
        { az_dense_step_avx2, "AVX2 twin", AZ_TWIN_NEED_AVX2 },               /* the installer's rule (v15) */
        { az_dense_step_vnni, "AVX-VNNI twin", AZ_TWIN_NEED_BRAIN_VNNI },
#else
        { NULL, "AVX2 twin", 0 },
        { NULL, "AVX-VNNI twin", 0 },
#endif
    };
    static const u32 sizes[4] = { 1, 16, 256, 4096 };

    printf("az_brainbench: %u steps per batch, %u batches, 64 entities; one clade tile = %u bytes (%u read per step)\n",
           steps, batches, (u32)sizeof(AzDenseNet), (u32)(AZ_W1_LEN + AZ_W2_LEN + 4 * (AZ_DENSE_HID + AZ_DENSE_MOT) + 2));
    printf("ALL TIMINGS: noisy, parallel load (other programs share this machine)\n");
    printf("median ns = median of the %u counted batches: linear interpolation between order statistics at\n"
           "h = (n - 1) q (Hyndman-Fan type 7, the R default); min ns = the fastest batch\n", batches);

    const u32 cpus[2] = { pcpu, ecpu };
    const char *want[2] = { "P core", "E core" };
    for (u32 ci = 0; ci < 2; ci++) {
        DWORD_PTR prev = SetThreadAffinityMask(GetCurrentThread(), (DWORD_PTR)1 << cpus[ci]);
        if (!prev) { printf("\ncould not pin to logical cpu %u (error %lu); skipped\n", cpus[ci], GetLastError()); continue; }
        Sleep(1);                                   /* let the scheduler move the thread */
        u32 feat = az_cpu_features();
        printf("\nlogical cpu %u, asked for a %s; CPUID leaf 0x1A says: %s\n", cpus[ci], want[ci],
               (feat & AZ_CPU_HYBRID_P) ? "performance core" : "not a performance core (E core or not hybrid)");
        printf("  %-14s %7s %10s %10s %10s   %s\n", "kernel", "clades", "tiles", "min ns", "median ns", "checksum");
        for (u32 si = 0; si < 4; si++) {
            u32 n = sizes[si];
            s_state = 0x1357 + n;
            for (u32 i = 0; i < steps; i++) perm[i] = (u32)(((s_rnd() >> 32) * (u64)n) >> 32);
            u64 ref = 0;
            for (u32 k = 0; k < 3; k++) {
                if (!kern[k].fn || (feat & kern[k].need) != kern[k].need) {
                    printf("  %-14s %7u %10s %10s %10s   not available\n", kern[k].name, n, "-", "-", "-");
                    continue;
                }
                double mn, md;
                u64 sum = bench(kern[k].fn, clades, perm, steps, batches, ent, &mn, &md);
                if (k == 0) ref = sum;
                char tiles[32];
                snprintf(tiles, sizeof tiles, "%.1f KB", (double)n * sizeof(AzDenseNet) / 1024.0);
                printf("  %-14s %7u %10s %10.2f %10.2f   %016" PRIx64 "%s\n", kern[k].name, n, tiles, mn, md, sum,
                       sum == ref ? "" : "  MISMATCH with the C reference");
            }
        }
    }
    printf("\nnoisy, parallel load: compare rows, do not quote single numbers as exact.\n");
    _aligned_free(clades); _aligned_free(ent); free(perm);
    return 0;
}
