/* az_gpubench.c - time brain-step batches on the GPU and on one CPU thread.
 *
 *   az_gpubench [reps]          default 9 repetitions per measurement
 *
 * For n = 100,000 and n = 1,000,000 creatures it reports, as min / median
 * over the repetitions:
 *   cpu  1 thread   : az_dense_step_c in a loop (the C reference, -O2, one thread)
 *   gpu  kernel     : launch + synchronise, all buffers already on the device
 *   gpu  round trip : upload sens, hid, gain, bias, clade; kernel; download hid,
 *                     motor (what a CPU tick would pay if only the brain ran on
 *                     the GPU), from pageable and from page-locked host memory
 * and checks every GPU result against the CPU bytes. When the build was
 * configured with -DAZ_GPU_BENCH_TWIN=ON it also times the AVX-VNNI assembly
 * twin on one thread, which is the CPU kernel the simulation really runs.
 *
 * EVERY TIMING IS NOISY: other processes share the CPU and the GPU while
 * this runs. The numbers are measurements of this run, not a specification.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "az_gpu.h"
#include "az_dense.h"

static double now_us(void)
{
    LARGE_INTEGER f, c;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart * 1e6 / (double)f.QuadPart;
}
static int cmp_double(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}
typedef struct Stat { double min, med; } Stat;
static Stat stat_of(double *v, int n)
{
    Stat s;
    qsort(v, (size_t)n, sizeof v[0], cmp_double);
    s.min = v[0];
    s.med = v[n / 2];
    return s;
}

static u64 td_ctr = 0x13198A2E03707344ull;
static u64 td_u64(void) { return az_mix64(td_ctr += 0x9E3779B97F4A7C15ull); }
static void td_fill(void *dst, size_t n)
{
    u8 *d = (u8 *)dst;
    size_t i = 0;
    for (; i + 8 <= n; i += 8) { u64 v = td_u64(); memcpy(d + i, &v, 8); }
    if (i < n) { u64 v = td_u64(); memcpy(d + i, &v, n - i); }
}
static void *xalloc(size_t n)
{
    void *p = malloc(n ? n : 1);
    if (!p) { printf("out of host memory (%zu bytes)\n", n); exit(1); }
    return p;
}
#define OK(call) do { int s_ = (call); if (s_) { printf("FAIL %s: %s: %s\n", #call, az_gpu_status_name(s_), az_gpu_error(g)); exit(1); } } while (0)

#ifdef AZ_GPU_BENCH_VNNI   /* kernels/az_brain_vnni.asm, linked when -DAZ_GPU_BENCH_TWIN=ON */
void az_dense_step_vnni(const AzDenseNet *c, const u8 *sens, u8 *hid,
                        const u8 *gain, const i8 *bias, i8 *motor);
#endif

enum { NCLADES = 64, MAXREPS = 101 };

typedef struct Host {            /* one set of host arrays */
    u8 *sens, *hid, *gain; i8 *bias; u16 *clade; i8 *motor;
} Host;

static void round_trip(AzGpu *g, const AzGpuBrain *b, const Host *h, u32 n)
{
    OK(az_gpu_upload(g, b->sens, 0, h->sens, (u64)n * AZ_DENSE_SENS));
    OK(az_gpu_upload(g, b->hid, 0, h->hid, (u64)n * AZ_DENSE_HID));
    OK(az_gpu_upload(g, b->gain, 0, h->gain, (u64)n * AZ_DENSE_MODG));
    OK(az_gpu_upload(g, b->bias, 0, h->bias, (u64)n * AZ_DENSE_MODG));
    OK(az_gpu_upload(g, b->clade, 0, h->clade, (u64)n * sizeof(u16)));
    OK(az_gpu_brain_step(g, b, n));
    OK(az_gpu_download(g, h->hid, b->hid, 0, (u64)n * AZ_DENSE_HID));
    OK(az_gpu_download(g, h->motor, b->motor, 0, (u64)n * AZ_DENSE_MOT));
}

static void bench_n(AzGpu *g, u32 n, int reps)
{
    static double t[MAXREPS];
    AzDenseNet *clades = xalloc(NCLADES * sizeof(AzDenseNet));
    memset(clades, 0, NCLADES * sizeof(AzDenseNet));
    for (u32 c = 0; c < NCLADES; c++) {
        AzDenseNet *k = &clades[c];
        td_fill(k->ntype, sizeof k->ntype);
        for (u32 i = 0; i < AZ_DENSE_NEURONS; i++) k->ntype[i] &= 7;
        for (u32 i = 0; i < 64; i++) k->theta[i] = (i8)((i32)(td_u64() % 97u) - 48);
        for (u32 i = 0; i < 8; i++) k->tbias[i] = (i8)((i32)(td_u64() & 31) - 16);
        k->salt = (u16)td_u64();
        k->var = (u8)(16 + (td_u64() & 63));
        k->shift1 = 7; k->shift2 = 6; k->alive = 1;
        az_dense_develop(k);
    }
    /* inputs: sens and hid in 0..127, as the simulation produces them */
    Host in, ref, pg;
    in.sens = xalloc((size_t)n * AZ_DENSE_SENS); in.hid = xalloc((size_t)n * AZ_DENSE_HID);
    in.gain = xalloc((size_t)n * AZ_DENSE_MODG); in.bias = xalloc((size_t)n * AZ_DENSE_MODG);
    in.clade = xalloc((size_t)n * sizeof(u16)); in.motor = NULL;
    td_fill(in.sens, (size_t)n * AZ_DENSE_SENS);
    td_fill(in.hid, (size_t)n * AZ_DENSE_HID);
    td_fill(in.bias, (size_t)n * AZ_DENSE_MODG);
    for (size_t i = 0; i < (size_t)n * AZ_DENSE_SENS; i++) in.sens[i] &= 127;
    for (size_t i = 0; i < (size_t)n * AZ_DENSE_HID; i++) in.hid[i] &= 127;
    for (size_t i = 0; i < (size_t)n * AZ_DENSE_MODG; i++) in.gain[i] = (u8)(48 + (td_u64() & 31));
    for (u32 i = 0; i < n; i++) in.clade[i] = (u16)(td_u64() % NCLADES);

    /* CPU, one thread. Each repetition starts from the same hidden state. */
    ref.hid = xalloc((size_t)n * AZ_DENSE_HID); ref.motor = xalloc((size_t)n * AZ_DENSE_MOT);
    for (int r = 0; r < reps; r++) {
        memcpy(ref.hid, in.hid, (size_t)n * AZ_DENSE_HID);
        double t0 = now_us();
        for (u32 i = 0; i < n; i++)
            az_dense_step_c(&clades[in.clade[i]], &in.sens[(size_t)i * AZ_DENSE_SENS], &ref.hid[(size_t)i * AZ_DENSE_HID],
                            &in.gain[(size_t)i * AZ_DENSE_MODG], &in.bias[(size_t)i * AZ_DENSE_MODG],
                            &ref.motor[(size_t)i * AZ_DENSE_MOT]);
        t[r] = now_us() - t0;
    }
    Stat cpu = stat_of(t, reps);

    int ok = 1;
#ifdef AZ_GPU_BENCH_VNNI
    /* optional second CPU baseline: the AVX-VNNI assembly twin, one thread */
    Stat vnni = { 0, 0 };
    int have_vnni = __builtin_cpu_supports("avx2") && __builtin_cpu_supports("avxvnni");
    if (have_vnni) {
        u8 *vh = xalloc((size_t)n * AZ_DENSE_HID);
        i8 *vm = xalloc((size_t)n * AZ_DENSE_MOT);
        for (int r = 0; r < reps; r++) {
            memcpy(vh, in.hid, (size_t)n * AZ_DENSE_HID);
            double t0 = now_us();
            for (u32 i = 0; i < n; i++)
                az_dense_step_vnni(&clades[in.clade[i]], &in.sens[(size_t)i * AZ_DENSE_SENS], &vh[(size_t)i * AZ_DENSE_HID],
                                   &in.gain[(size_t)i * AZ_DENSE_MODG], &in.bias[(size_t)i * AZ_DENSE_MODG],
                                   &vm[(size_t)i * AZ_DENSE_MOT]);
            t[r] = now_us() - t0;
        }
        vnni = stat_of(t, reps);
        ok &= memcmp(vh, ref.hid, (size_t)n * AZ_DENSE_HID) == 0 && memcmp(vm, ref.motor, (size_t)n * AZ_DENSE_MOT) == 0;
        free(vh); free(vm);
    }
#endif

    /* device buffers */
    AzGpuBrain b;
    memset(&b, 0, sizeof b);
    b.n_clades = NCLADES;
    OK(az_gpu_alloc(g, &b.sens, (u64)n * AZ_DENSE_SENS));
    OK(az_gpu_alloc(g, &b.hid, (u64)n * AZ_DENSE_HID));
    OK(az_gpu_alloc(g, &b.gain, (u64)n * AZ_DENSE_MODG));
    OK(az_gpu_alloc(g, &b.bias, (u64)n * AZ_DENSE_MODG));
    OK(az_gpu_alloc(g, &b.clade, (u64)n * sizeof(u16)));
    OK(az_gpu_alloc(g, &b.motor, (u64)n * AZ_DENSE_MOT));
    OK(az_gpu_alloc(g, &b.clades, (u64)NCLADES * sizeof(AzDenseNet)));
    OK(az_gpu_upload(g, b.clades, 0, clades, (u64)NCLADES * sizeof(AzDenseNet)));

    /* round trip from pageable memory (plain malloc) */
    pg.sens = in.sens; pg.gain = in.gain; pg.bias = in.bias; pg.clade = in.clade;
    pg.hid = xalloc((size_t)n * AZ_DENSE_HID); pg.motor = xalloc((size_t)n * AZ_DENSE_MOT);
    for (int r = -1; r < reps; r++) {                       /* r = -1 warms up */
        memcpy(pg.hid, in.hid, (size_t)n * AZ_DENSE_HID);
        double t0 = now_us();
        round_trip(g, &b, &pg, n);
        if (r >= 0) t[r] = now_us() - t0;
        ok &= memcmp(pg.hid, ref.hid, (size_t)n * AZ_DENSE_HID) == 0 && memcmp(pg.motor, ref.motor, (size_t)n * AZ_DENSE_MOT) == 0;
    }
    Stat trip = stat_of(t, reps);

    /* kernel only: the buffers stay on the device; hid is restored before each run */
    for (int r = -1; r < reps; r++) {
        OK(az_gpu_upload(g, b.hid, 0, in.hid, (u64)n * AZ_DENSE_HID));
        OK(az_gpu_sync(g));
        double t0 = now_us();
        OK(az_gpu_brain_step(g, &b, n));
        OK(az_gpu_sync(g));
        if (r >= 0) t[r] = now_us() - t0;
    }
    Stat ker = stat_of(t, reps);
    OK(az_gpu_download(g, pg.hid, b.hid, 0, (u64)n * AZ_DENSE_HID));
    OK(az_gpu_download(g, pg.motor, b.motor, 0, (u64)n * AZ_DENSE_MOT));
    ok &= memcmp(pg.hid, ref.hid, (size_t)n * AZ_DENSE_HID) == 0 && memcmp(pg.motor, ref.motor, (size_t)n * AZ_DENSE_MOT) == 0;

    /* round trip from page-locked memory allocated by the driver */
    Host pin;
    pin.sens = az_gpu_host_alloc(g, (u64)n * AZ_DENSE_SENS); pin.hid = az_gpu_host_alloc(g, (u64)n * AZ_DENSE_HID);
    pin.gain = az_gpu_host_alloc(g, (u64)n * AZ_DENSE_MODG); pin.bias = az_gpu_host_alloc(g, (u64)n * AZ_DENSE_MODG);
    pin.clade = az_gpu_host_alloc(g, (u64)n * sizeof(u16)); pin.motor = az_gpu_host_alloc(g, (u64)n * AZ_DENSE_MOT);
    Stat trip_pin = { 0, 0 };
    int have_pin = pin.sens && pin.hid && pin.gain && pin.bias && pin.clade && pin.motor;
    if (have_pin) {
        memcpy(pin.sens, in.sens, (size_t)n * AZ_DENSE_SENS); memcpy(pin.gain, in.gain, (size_t)n * AZ_DENSE_MODG);
        memcpy(pin.bias, in.bias, (size_t)n * AZ_DENSE_MODG); memcpy(pin.clade, in.clade, (size_t)n * sizeof(u16));
        for (int r = -1; r < reps; r++) {
            memcpy(pin.hid, in.hid, (size_t)n * AZ_DENSE_HID);
            double t0 = now_us();
            round_trip(g, &b, &pin, n);
            if (r >= 0) t[r] = now_us() - t0;
            ok &= memcmp(pin.hid, ref.hid, (size_t)n * AZ_DENSE_HID) == 0 && memcmp(pin.motor, ref.motor, (size_t)n * AZ_DENSE_MOT) == 0;
        }
        trip_pin = stat_of(t, reps);
    }

    u64 up = (u64)n * (AZ_DENSE_SENS + AZ_DENSE_HID + 2 * AZ_DENSE_MODG + sizeof(u16)), down = (u64)n * (AZ_DENSE_HID + AZ_DENSE_MOT);
    printf("\nn = %u creatures, %u clades, %d repetitions; copies per round trip: %.1f MB up, %.1f MB down\n",
           n, (u32)NCLADES, reps, (double)up / 1e6, (double)down / 1e6);
    printf("  %-34s %10s %10s %12s %10s\n", "(noisy, parallel load)", "min ms", "median ms", "ns/creature", "x cpu");
#define ROW(name, st) printf("  %-34s %10.3f %10.3f %12.1f %10.2f\n", name, (st).min / 1e3, (st).med / 1e3, \
                             (st).med * 1e3 / n, cpu.med / (st).med)
    ROW("cpu  1 thread, C reference", cpu);
#ifdef AZ_GPU_BENCH_VNNI
    if (have_vnni) ROW("cpu  1 thread, AVX-VNNI twin", vnni);
    else printf("  cpu  1 thread, AVX-VNNI twin: this CPU has no AVX-VNNI\n");
#endif
    ROW("gpu  kernel only (launch + sync)", ker);
    ROW("gpu  round trip, pageable memory", trip);
    if (have_pin) ROW("gpu  round trip, page-locked memory", trip_pin);
    else printf("  gpu  round trip, page-locked memory: not measured (%s)\n", az_gpu_error(g));
#undef ROW
    printf("  gpu result == cpu result, every repetition: %s\n", ok ? "yes" : "NO");
    if (!ok) exit(1);

    az_gpu_host_free(g, pin.sens); az_gpu_host_free(g, pin.hid); az_gpu_host_free(g, pin.gain);
    az_gpu_host_free(g, pin.bias); az_gpu_host_free(g, pin.clade); az_gpu_host_free(g, pin.motor);
    az_gpu_free(g, &b.sens); az_gpu_free(g, &b.hid); az_gpu_free(g, &b.gain); az_gpu_free(g, &b.bias);
    az_gpu_free(g, &b.clade); az_gpu_free(g, &b.motor); az_gpu_free(g, &b.clades);
    free(clades); free(in.sens); free(in.hid); free(in.gain); free(in.bias); free(in.clade);
    free(ref.hid); free(ref.motor); free(pg.hid); free(pg.motor);
}

int main(int argc, char **argv)
{
    int reps = argc > 1 ? atoi(argv[1]) : 9;
    if (reps < 1) reps = 1;
    if (reps > MAXREPS) reps = MAXREPS;

    AzGpu *g = NULL;
    AzGpuInfo info;
    int s = az_gpu_open(&g, NULL, &info);
    if (s != AZ_GPU_OK) {
        printf("az_gpubench: no GPU measurement possible: %s (%s)\n", az_gpu_status_name(s), info.msg);
        return s == AZ_GPU_NO_DRIVER || s == AZ_GPU_NO_DEVICE || s == AZ_GPU_NO_PTX ? 0 : 1;
    }
    printf("az_gpubench: %s\n", info.msg);
    printf("ALL TIMINGS: noisy, parallel load (other processes share this CPU and GPU).\n");

    /* fixed cost of one launch */
    static double t[2000];
    for (int i = 0; i < 200; i++) { OK(az_gpu_empty(g)); OK(az_gpu_sync(g)); }
    for (int i = 0; i < 2000; i++) {
        double t0 = now_us();
        OK(az_gpu_empty(g));
        OK(az_gpu_sync(g));
        t[i] = now_us() - t0;
    }
    Stat e = stat_of(t, 2000);
    printf("empty kernel, launch + sync, 2000 runs: min %.1f us, median %.1f us (noisy, parallel load)\n", e.min, e.med);

    bench_n(g, 100000, reps);
    bench_n(g, 1000000, reps);
    az_gpu_close(g);
    return 0;
}
