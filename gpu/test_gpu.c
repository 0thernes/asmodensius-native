/* test_gpu.c - the GPU path is bit-identical to the CPU reference.
 *
 * Exit code 0 with a clear SKIPPED line when the machine has no NVIDIA
 * driver, no CUDA device, or the build carries no device code. Everything
 * that can be tested without a GPU (the clean failure paths) is still run.
 *
 * CPU references: az_philox4x32_n and az_dense_step_c from az_core, i.e. the
 * same source files the device code was compiled from.
 */
#include "az_test.h"
#include "az_gpu.h"
#include "az_dense.h"

/* ---- test data: a counter through the splitmix64 finaliser -------------- */
static u64 td_ctr = 0x243F6A8885A308D3ull;
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
    if (!p) { printf("FAIL: out of host memory (%zu bytes)\n", n); exit(1); }
    return p;
}
/* number of differing records of `rec` bytes */
static u32 diff_records(const void *a, const void *b, u32 n, u32 rec)
{
    const u8 *x = (const u8 *)a, *y = (const u8 *)b;
    u32 d = 0;
    for (u32 i = 0; i < n; i++) d += memcmp(x + (size_t)i * rec, y + (size_t)i * rec, rec) != 0;
    return d;
}

/* ---- 1. failure paths that need no GPU ----------------------------------- */
static void test_no_driver_paths(void)
{
    AzGpu *g = (AzGpu *)(uintptr_t)1;
    AzGpuInfo info;
    AzGpuOptions o;
    memset(&o, 0, sizeof o);

    o.dll_path = "Z:\\no-such-folder\\nvcuda.dll";       /* a machine without the driver */
    int s = az_gpu_open(&g, &o, &info);
    CHECK_EQ(s, AZ_GPU_NO_DRIVER);
    CHECK(g == NULL);
    CHECK_EQ(info.status, AZ_GPU_NO_DRIVER);
    CHECK(strstr(info.msg, "no NVIDIA driver") != NULL);
    printf("no-driver path : status=%d \"%s\"\n", s, info.msg);

    g = (AzGpu *)(uintptr_t)1;
    o.dll_path = "kernel32.dll";                         /* a DLL that is not a CUDA driver */
    s = az_gpu_open(&g, &o, &info);
    CHECK_EQ(s, AZ_GPU_NO_DRIVER);
    CHECK(g == NULL);
    CHECK(strstr(info.msg, "lacks the symbol") != NULL);
    printf("wrong-dll path : status=%d \"%s\"\n", s, info.msg);

    CHECK_EQ(az_gpu_open(NULL, NULL, &info), AZ_GPU_BAD_ARG);
    az_gpu_close(NULL);                                  /* must be harmless */
    CHECK(strcmp(az_gpu_status_name(AZ_GPU_NO_DRIVER), "no NVIDIA driver") == 0);
}

/* ---- 2. Philox ------------------------------------------------------------ */
static void test_philox(AzGpu *g)
{
    /* known answers of Philox4x32-10 (Random123 kat_vectors) pin the CPU reference */
    static const u32 kat[3][10] = {
        { 0, 0, 0, 0, 0, 0, 0x6627e8d5u, 0xe169c58du, 0xbc57ac4cu, 0x9b00dbd8u },
        { 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu,
          0x408f276du, 0x41c83b0eu, 0xa20bc7c6u, 0x6d5451fdu },
        { 0x243f6a88u, 0x85a308d3u, 0x13198a2eu, 0x03707344u, 0xa4093822u, 0x299f31d0u,
          0xd16cfe09u, 0x94fdccebu, 0x5001e420u, 0x24126ea1u } };
    for (u32 k = 0; k < 3; k++) {
        u32 cpu[4], gpu[4] = { 0, 0, 0, 0 };
        az_philox4x32(cpu, &kat[k][0], &kat[k][4]);
        CHECK(memcmp(cpu, &kat[k][6], 16) == 0);
        CHECK_EQ(az_gpu_philox_host(g, gpu, &kat[k][0], &kat[k][4], 1), AZ_GPU_OK);
        CHECK(memcmp(gpu, &kat[k][6], 16) == 0);
    }

    const u32 n = 1000000;
    u32 *ctr = xalloc((size_t)n * 16), *cpu = xalloc((size_t)n * 16);
    u32 *g1 = xalloc((size_t)n * 16), *g2 = xalloc((size_t)n * 16);
    td_fill(ctr, (size_t)n * 16);
    /* structured counters as the simulation uses them: (id, tick, stream, idx) */
    for (u32 i = 0; i < n; i += 3) { ctr[4 * i] = i; ctr[4 * i + 1] = 77; ctr[4 * i + 2] = AZ_RS_NOISE; ctr[4 * i + 3] = 0; }
    u32 key[2] = { 0x12345678u, 0x9ABCDEF0u };
    az_philox4x32_n(cpu, ctr, key, n);
    memset(g1, 0xA5, (size_t)n * 16);
    memset(g2, 0x5A, (size_t)n * 16);
    CHECK_EQ(az_gpu_philox_host(g, g1, ctr, key, n), AZ_GPU_OK);
    CHECK_EQ(az_gpu_set_block(g, 1024), AZ_GPU_OK);      /* another block size, same answer */
    CHECK_EQ(az_gpu_philox_host(g, g2, ctr, key, n), AZ_GPU_OK);
    CHECK_EQ(az_gpu_set_block(g, 256), AZ_GPU_OK);
    u32 d_cpu = diff_records(g1, cpu, n, 16), d_run = diff_records(g1, g2, n, 16);
    CHECK_EQ(d_cpu, 0);
    CHECK_EQ(d_run, 0);
    printf("philox         : %u blocks, gpu vs cpu differing blocks = %u, run 1 vs run 2 = %u\n", n, d_cpu, d_run);

    /* a different key must change the stream (the comparison can fail) */
    u32 key2[2] = { key[0] ^ 1u, key[1] };
    CHECK_EQ(az_gpu_philox_host(g, g2, ctr, key2, 4096), AZ_GPU_OK);
    CHECK(diff_records(g2, cpu, 4096, 16) == 4096);
    free(ctr); free(cpu); free(g1); free(g2);
}

/* ---- 3. the brain step ------------------------------------------------------ */

/* Clade table for the tests. Three kinds:
 *   [0, n_dev)        developed from a random neural program (az_dense_develop)
 *   [n_dev, n_con)    raw random networks at the edge of the brain contract:
 *                     any int8 weight, |b1|, |b2| <= 2^20, shifts 0..15
 *   [n_con, nc)       beyond the contract: any i32 bias, shifts 0..31
 * The first six raw clades are hand-made extremes. */
static void make_clades(AzDenseNet *cl, u32 nc, u32 n_dev, u32 n_con)
{
    memset(cl, 0, (size_t)nc * sizeof *cl);
    for (u32 c = 0; c < nc; c++) {
        AzDenseNet *k = &cl[c];
        k->alive = 1;
        if (c < n_dev) {
            td_fill(k->ntype, sizeof k->ntype);
            for (u32 i = 0; i < AZ_DENSE_NEURONS; i++) k->ntype[i] &= 7;
            td_fill(k->theta, sizeof k->theta);
            for (u32 i = 0; i < 8; i++) k->tbias[i] = (i8)((i32)(td_u64() & 31) - 16);
            k->salt = (u16)td_u64();
            k->var = (u8)td_u64();
            k->shift1 = (u8)(1 + td_u64() % 15);
            k->shift2 = (u8)(1 + td_u64() % 15);
            az_dense_develop(k);
        } else {
            bool wild = c >= n_con;
            td_fill(k->w1, sizeof k->w1);
            td_fill(k->w2, sizeof k->w2);
            for (u32 h = 0; h < AZ_DENSE_HID; h++)
                k->b1[h] = wild ? (i32)td_u64() : (i32)(td_u64() % 2097153u) - 1048576;
            for (u32 m = 0; m < AZ_DENSE_MOT; m++)
                k->b2[m] = wild ? (i32)td_u64() : (i32)(td_u64() % 2097153u) - 1048576;
            k->shift1 = (u8)(td_u64() % (wild ? 32 : 16));
            k->shift2 = (u8)(td_u64() % (wild ? 32 : 16));
        }
    }
    /* extremes, inside the contract */
    AzDenseNet *e = &cl[n_dev];
    memset(e[0].w1, 0, sizeof e[0].w1); memset(e[0].w2, 0, sizeof e[0].w2);            /* all zero */
    memset(e[0].b1, 0, sizeof e[0].b1); memset(e[0].b2, 0, sizeof e[0].b2);
    memset(e[1].w1, 127, sizeof e[1].w1); memset(e[1].w2, 127, sizeof e[1].w2);        /* all max */
    memset(e[2].w1, -128, sizeof e[2].w1); memset(e[2].w2, -128, sizeof e[2].w2);      /* all min */
    for (u32 h = 0; h < AZ_DENSE_HID; h++) { e[1].b1[h] = 1048576; e[2].b1[h] = -1048576; }
    for (u32 m = 0; m < AZ_DENSE_MOT; m++) { e[1].b2[m] = 1048576; e[2].b2[m] = -1048576; }
    e[1].shift1 = 0; e[1].shift2 = 0; e[2].shift1 = 0; e[2].shift2 = 0;
    e[3].shift1 = 15; e[3].shift2 = 15;
    e[4].shift1 = 0; e[4].shift2 = 15;
    e[5].shift1 = 15; e[5].shift2 = 0;
}

typedef struct BrainData {
    u32 n, nc, n_dev, n_con;
    AzDenseNet *clades;
    u8 *sens, *hid0, *gain;
    i8 *bias;
    u16 *clade;
    u8 *wild;            /* 1 = this creature is outside the brain contract */
    u8 *hid_cpu;         /* CPU reference after one step */
    i8 *mot_cpu;
} BrainData;

static void brain_data_make(BrainData *d, u32 n)
{
    memset(d, 0, sizeof *d);
    d->n = n; d->nc = 320; d->n_dev = 200; d->n_con = 300;
    d->clades = xalloc((size_t)d->nc * sizeof(AzDenseNet));
    make_clades(d->clades, d->nc, d->n_dev, d->n_con);
    d->sens = xalloc((size_t)n * AZ_DENSE_SENS); d->hid0 = xalloc((size_t)n * AZ_DENSE_HID);
    d->gain = xalloc((size_t)n * AZ_DENSE_MODG); d->bias = xalloc((size_t)n * AZ_DENSE_MODG);
    d->clade = xalloc((size_t)n * sizeof(u16)); d->wild = xalloc(n);
    d->hid_cpu = xalloc((size_t)n * AZ_DENSE_HID); d->mot_cpu = xalloc((size_t)n * AZ_DENSE_MOT);
    td_fill(d->sens, (size_t)n * AZ_DENSE_SENS);
    td_fill(d->hid0, (size_t)n * AZ_DENSE_HID);
    td_fill(d->gain, (size_t)n * AZ_DENSE_MODG);
    td_fill(d->bias, (size_t)n * AZ_DENSE_MODG);
    for (u32 i = 0; i < n; i++) {
        bool wild = (i & 7) == 7;
        d->wild[i] = wild;
        d->clade[i] = (u16)(td_u64() % (wild ? d->nc : d->n_con));
        if (!wild) {
            for (u32 j = 0; j < AZ_DENSE_SENS; j++) d->sens[(size_t)i * AZ_DENSE_SENS + j] &= 127;
            for (u32 j = 0; j < AZ_DENSE_HID; j++) d->hid0[(size_t)i * AZ_DENSE_HID + j] &= 127;
        }
    }
    /* hand-made extremes on the first creatures: inputs all 0 / all 127,
     * gain 0 / 255, bias -128 / 127, on each of the six extreme clades */
    for (u32 i = 0; i < 48 && i < n; i++) {
        d->wild[i] = 0;
        d->clade[i] = (u16)(d->n_dev + i % 6);
        memset(&d->sens[(size_t)i * AZ_DENSE_SENS], (i & 8) ? 127 : 0, AZ_DENSE_SENS);
        memset(&d->hid0[(size_t)i * AZ_DENSE_HID], (i & 16) ? 127 : 0, AZ_DENSE_HID);
        memset(&d->gain[(size_t)i * AZ_DENSE_MODG], (i & 32) ? 255 : (i % 3 ? 64 : 0), AZ_DENSE_MODG);
        memset(&d->bias[(size_t)i * AZ_DENSE_MODG], (i % 5 == 0) ? -128 : (i % 5 == 1 ? 127 : 0), AZ_DENSE_MODG);
    }
    memcpy(d->hid_cpu, d->hid0, (size_t)n * AZ_DENSE_HID);
    for (u32 i = 0; i < n; i++)
        az_dense_step_c(&d->clades[d->clade[i]], &d->sens[(size_t)i * AZ_DENSE_SENS],
                        &d->hid_cpu[(size_t)i * AZ_DENSE_HID], &d->gain[(size_t)i * AZ_DENSE_MODG],
                        &d->bias[(size_t)i * AZ_DENSE_MODG], &d->mot_cpu[(size_t)i * AZ_DENSE_MOT]);
}

static void brain_data_free(BrainData *d)
{
    free(d->clades); free(d->sens); free(d->hid0); free(d->gain); free(d->bias);
    free(d->clade); free(d->wild); free(d->hid_cpu); free(d->mot_cpu);
}

static u64 fnv1a64(const void *p, size_t n, u64 h)
{
    const u8 *b = (const u8 *)p;
    for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 0x100000001b3ull; }
    return h;
}

/* 1,000,000 random brain steps: GPU == CPU, and two GPU runs are identical */
static void test_brain_million(AzGpu *g, const BrainData *d)
{
    const u32 n = d->n;
    u8 *h1 = xalloc((size_t)n * AZ_DENSE_HID), *h2 = xalloc((size_t)n * AZ_DENSE_HID);
    i8 *m1 = xalloc((size_t)n * AZ_DENSE_MOT), *m2 = xalloc((size_t)n * AZ_DENSE_MOT);
    memcpy(h1, d->hid0, (size_t)n * AZ_DENSE_HID);
    memcpy(h2, d->hid0, (size_t)n * AZ_DENSE_HID);
    memset(m1, 0x55, (size_t)n * AZ_DENSE_MOT);
    memset(m2, 0x33, (size_t)n * AZ_DENSE_MOT);
    CHECK_EQ(az_gpu_brain_step_host(g, d->clades, d->nc, d->sens, h1, d->gain, d->bias, d->clade, m1, n), AZ_GPU_OK);
    CHECK_EQ(az_gpu_set_block(g, 1024), AZ_GPU_OK);
    CHECK_EQ(az_gpu_brain_step_host(g, d->clades, d->nc, d->sens, h2, d->gain, d->bias, d->clade, m2, n), AZ_GPU_OK);
    CHECK_EQ(az_gpu_set_block(g, 256), AZ_GPU_OK);

    u32 bad_con = 0, bad_wild = 0, n_wild = 0, changed = 0, sat = 0;
    for (u32 i = 0; i < n; i++) {
        bool same = memcmp(&h1[(size_t)i * AZ_DENSE_HID], &d->hid_cpu[(size_t)i * AZ_DENSE_HID], AZ_DENSE_HID) == 0 &&
                    memcmp(&m1[(size_t)i * AZ_DENSE_MOT], &d->mot_cpu[(size_t)i * AZ_DENSE_MOT], AZ_DENSE_MOT) == 0;
        n_wild += d->wild[i];
        if (!same) { if (d->wild[i]) bad_wild++; else bad_con++; }
        changed += memcmp(&h1[(size_t)i * AZ_DENSE_HID], &d->hid0[(size_t)i * AZ_DENSE_HID], AZ_DENSE_HID) != 0;
        for (u32 k = 0; k < AZ_DENSE_MOT; k++) { i8 v = m1[(size_t)i * AZ_DENSE_MOT + k]; sat += (v == 127 || v == -127); }
    }
    CHECK_EQ(bad_con, 0);
    CHECK_EQ(bad_wild, 0);
    CHECK(memcmp(h1, d->hid_cpu, (size_t)n * AZ_DENSE_HID) == 0);
    CHECK(memcmp(m1, d->mot_cpu, (size_t)n * AZ_DENSE_MOT) == 0);
    CHECK(memcmp(h1, h2, (size_t)n * AZ_DENSE_HID) == 0);          /* run 1 == run 2 */
    CHECK(memcmp(m1, m2, (size_t)n * AZ_DENSE_MOT) == 0);
    /* the data is not degenerate: the step changed most hidden states and the
     * motors are not all pinned at the clamp */
    CHECK(changed > n / 2);
    CHECK(sat < n * (AZ_DENSE_MOT - 1));
    u64 dg = fnv1a64(m1, (size_t)n * AZ_DENSE_MOT, fnv1a64(h1, (size_t)n * AZ_DENSE_HID, 0xcbf29ce484222325ull));
    u64 dc = fnv1a64(d->mot_cpu, (size_t)n * AZ_DENSE_MOT, fnv1a64(d->hid_cpu, (size_t)n * AZ_DENSE_HID, 0xcbf29ce484222325ull));
    CHECK(dg == dc);
    printf("brain          : %u steps over %u clades; differing creatures gpu vs cpu: %u of %u in contract, "
           "%u of %u beyond it; run 1 vs run 2 identical: %s\n", n, d->nc, bad_con, n - n_wild, bad_wild, n_wild,
           (memcmp(h1, h2, (size_t)n * AZ_DENSE_HID) == 0 && memcmp(m1, m2, (size_t)n * AZ_DENSE_MOT) == 0) ? "yes" : "NO");
    printf("brain digest   : cpu %016llx gpu %016llx (hid changed in %u creatures, %u of %u motor values at the clamp)\n",
           (unsigned long long)dc, (unsigned long long)dg, changed, sat, n * AZ_DENSE_MOT);
    free(h1); free(h2); free(m1); free(m2);
}

/* device buffers of n creatures plus `pad` guard creatures, filled from d */
static int brain_bufs_make(AzGpu *g, AzGpuBrain *b, const BrainData *d, u32 n, u32 pad)
{
    memset(b, 0, sizeof *b);
    b->n_clades = d->nc;
    u32 t = n + pad;
    int s = az_gpu_alloc(g, &b->sens, (u64)t * AZ_DENSE_SENS);
    if (!s) s = az_gpu_alloc(g, &b->hid, (u64)t * AZ_DENSE_HID);
    if (!s) s = az_gpu_alloc(g, &b->gain, (u64)t * AZ_DENSE_MODG);
    if (!s) s = az_gpu_alloc(g, &b->bias, (u64)t * AZ_DENSE_MODG);
    if (!s) s = az_gpu_alloc(g, &b->clade, (u64)t * sizeof(u16));
    if (!s) s = az_gpu_alloc(g, &b->motor, (u64)t * AZ_DENSE_MOT);
    if (!s) s = az_gpu_alloc(g, &b->clades, (u64)d->nc * sizeof(AzDenseNet));
    if (!s) s = az_gpu_upload(g, b->sens, 0, d->sens, (u64)t * AZ_DENSE_SENS);
    if (!s) s = az_gpu_upload(g, b->hid, 0, d->hid0, (u64)t * AZ_DENSE_HID);
    if (!s) s = az_gpu_upload(g, b->gain, 0, d->gain, (u64)t * AZ_DENSE_MODG);
    if (!s) s = az_gpu_upload(g, b->bias, 0, d->bias, (u64)t * AZ_DENSE_MODG);
    if (!s) s = az_gpu_upload(g, b->clade, 0, d->clade, (u64)t * sizeof(u16));
    if (!s) s = az_gpu_upload(g, b->clades, 0, d->clades, (u64)d->nc * sizeof(AzDenseNet));
    return s;
}
static void brain_bufs_free(AzGpu *g, AzGpuBrain *b)
{
    az_gpu_free(g, &b->sens); az_gpu_free(g, &b->hid); az_gpu_free(g, &b->gain);
    az_gpu_free(g, &b->bias); az_gpu_free(g, &b->clade); az_gpu_free(g, &b->motor);
    az_gpu_free(g, &b->clades);
}

/* batch sizes around the block size: exactly n creatures are computed and
 * nothing past n is written */
static void test_brain_sizes(AzGpu *g, const BrainData *d)
{
    static const u32 sizes[] = { 1, 2, 255, 256, 257, 1000, 65537 };
    const u32 pad = 64;
    for (u32 k = 0; k < AZ_LEN(sizes); k++) {
        u32 n = sizes[k], t = n + pad;
        AzGpuBrain b;
        CHECK_EQ(brain_bufs_make(g, &b, d, n, pad), AZ_GPU_OK);
        i8 *mot = xalloc((size_t)t * AZ_DENSE_MOT);
        u8 *hid = xalloc((size_t)t * AZ_DENSE_HID);
        memset(mot, 0x6E, (size_t)t * AZ_DENSE_MOT);
        CHECK_EQ(az_gpu_upload(g, b.motor, 0, mot, (u64)t * AZ_DENSE_MOT), AZ_GPU_OK);
        CHECK_EQ(az_gpu_brain_step(g, &b, n), AZ_GPU_OK);
        CHECK_EQ(az_gpu_download(g, mot, b.motor, 0, (u64)t * AZ_DENSE_MOT), AZ_GPU_OK);
        CHECK_EQ(az_gpu_download(g, hid, b.hid, 0, (u64)t * AZ_DENSE_HID), AZ_GPU_OK);
        CHECK(memcmp(mot, d->mot_cpu, (size_t)n * AZ_DENSE_MOT) == 0);
        CHECK(memcmp(hid, d->hid_cpu, (size_t)n * AZ_DENSE_HID) == 0);
        u32 touched = 0;
        for (u32 i = n * AZ_DENSE_MOT; i < t * AZ_DENSE_MOT; i++) touched += mot[i] != 0x6E;
        CHECK_EQ(touched, 0);                                                /* guard motors untouched */
        CHECK(memcmp(&hid[(size_t)n * AZ_DENSE_HID], &d->hid0[(size_t)n * AZ_DENSE_HID], (size_t)pad * AZ_DENSE_HID) == 0);
        free(mot); free(hid);
        brain_bufs_free(g, &b);
    }
}

/* the hidden state stays on the device for several steps (the recurrent path) */
static void test_brain_recurrent(AzGpu *g, const BrainData *d)
{
    const u32 n = 100000, steps = 8;
    AzGpuBrain b;
    CHECK_EQ(brain_bufs_make(g, &b, d, n, 0), AZ_GPU_OK);
    u8 *hc = xalloc((size_t)n * AZ_DENSE_HID), *hg = xalloc((size_t)n * AZ_DENSE_HID);
    i8 *mc = xalloc((size_t)n * AZ_DENSE_MOT), *mg = xalloc((size_t)n * AZ_DENSE_MOT);
    memcpy(hc, d->hid0, (size_t)n * AZ_DENSE_HID);
    for (u32 s = 0; s < steps; s++) {
        CHECK_EQ(az_gpu_brain_step(g, &b, n), AZ_GPU_OK);
        for (u32 i = 0; i < n; i++)
            az_dense_step_c(&d->clades[d->clade[i]], &d->sens[(size_t)i * AZ_DENSE_SENS], &hc[(size_t)i * AZ_DENSE_HID],
                            &d->gain[(size_t)i * AZ_DENSE_MODG], &d->bias[(size_t)i * AZ_DENSE_MODG], &mc[(size_t)i * AZ_DENSE_MOT]);
    }
    CHECK_EQ(az_gpu_download(g, hg, b.hid, 0, (u64)n * AZ_DENSE_HID), AZ_GPU_OK);
    CHECK_EQ(az_gpu_download(g, mg, b.motor, 0, (u64)n * AZ_DENSE_MOT), AZ_GPU_OK);
    u32 dh = diff_records(hg, hc, n, AZ_DENSE_HID), dm = diff_records(mg, mc, n, AZ_DENSE_MOT);
    CHECK_EQ(dh, 0);
    CHECK_EQ(dm, 0);
    /* the eighth state is not the first one: the recurrence did something */
    u32 moved = diff_records(hc, d->hid_cpu, n, AZ_DENSE_HID);
    CHECK(moved > n / 4);
    printf("brain recurrent: %u creatures x %u steps on device buffers; differing hid = %u, motor = %u "
           "(state after step 8 differs from step 1 in %u creatures)\n", n, steps, dh, dm, moved);
    free(hc); free(hg); free(mc); free(mg);
    brain_bufs_free(g, &b);
}

/* guards: a clade index outside the table, buffers that are too small, and a
 * control showing the comparison is able to fail */
static void test_brain_guards(AzGpu *g, const BrainData *d)
{
    const u32 n = 4096;
    AzGpuBrain b;
    CHECK_EQ(brain_bufs_make(g, &b, d, n, 0), AZ_GPU_OK);
    u8 *hid = xalloc((size_t)n * AZ_DENSE_HID);
    i8 *mot = xalloc((size_t)n * AZ_DENSE_MOT);

    /* out-of-table clade index: zero motors, hidden state kept, neighbours unaffected */
    u16 bad[2] = { (u16)d->nc, 0xFFFF };
    CHECK_EQ(az_gpu_upload(g, b.clade, 5 * sizeof(u16), &bad[0], sizeof(u16)), AZ_GPU_OK);
    CHECK_EQ(az_gpu_upload(g, b.clade, 9 * sizeof(u16), &bad[1], sizeof(u16)), AZ_GPU_OK);
    CHECK_EQ(az_gpu_brain_step(g, &b, n), AZ_GPU_OK);
    CHECK_EQ(az_gpu_download(g, hid, b.hid, 0, (u64)n * AZ_DENSE_HID), AZ_GPU_OK);
    CHECK_EQ(az_gpu_download(g, mot, b.motor, 0, (u64)n * AZ_DENSE_MOT), AZ_GPU_OK);
    u32 wrong = 0;
    for (u32 i = 0; i < n; i++) {
        if (i == 5 || i == 9) {
            for (u32 k = 0; k < AZ_DENSE_MOT; k++) wrong += mot[(size_t)i * AZ_DENSE_MOT + k] != 0;
            wrong += memcmp(&hid[(size_t)i * AZ_DENSE_HID], &d->hid0[(size_t)i * AZ_DENSE_HID], AZ_DENSE_HID) != 0;
        } else {
            wrong += memcmp(&hid[(size_t)i * AZ_DENSE_HID], &d->hid_cpu[(size_t)i * AZ_DENSE_HID], AZ_DENSE_HID) != 0;
            wrong += memcmp(&mot[(size_t)i * AZ_DENSE_MOT], &d->mot_cpu[(size_t)i * AZ_DENSE_MOT], AZ_DENSE_MOT) != 0;
        }
    }
    CHECK_EQ(wrong, 0);

    /* buffers smaller than n are refused before anything is launched */
    CHECK_EQ(az_gpu_brain_step(g, &b, n + 1), AZ_GPU_BAD_ARG);
    AzGpuBrain c = b;
    c.n_clades = d->nc + 1;
    CHECK_EQ(az_gpu_brain_step(g, &c, n), AZ_GPU_BAD_ARG);
    CHECK_EQ(az_gpu_upload(g, b.motor, (u64)n * AZ_DENSE_MOT - 4, mot, 8), AZ_GPU_BAD_ARG);
    CHECK_EQ(az_gpu_download(g, mot, b.motor, 1, (u64)n * AZ_DENSE_MOT), AZ_GPU_BAD_ARG);
    u32 key[2] = { 1, 2 };
    CHECK_EQ(az_gpu_philox(g, b.motor, b.sens, key, n + 1), AZ_GPU_BAD_ARG);   /* motor holds 16 n bytes only */
    brain_bufs_free(g, &b);

    /* control: one changed bias word per clade on the device must be visible */
    AzDenseNet *alt = xalloc((size_t)d->nc * sizeof(AzDenseNet));
    memcpy(alt, d->clades, (size_t)d->nc * sizeof(AzDenseNet));
    for (u32 k = 0; k < d->nc; k++) alt[k].b1[3] += 1 << 16;
    memcpy(hid, d->hid0, (size_t)n * AZ_DENSE_HID);
    CHECK_EQ(az_gpu_brain_step_host(g, alt, d->nc, d->sens, hid, d->gain, d->bias, d->clade, mot, n), AZ_GPU_OK);
    u32 seen = diff_records(hid, d->hid_cpu, n, AZ_DENSE_HID);
    CHECK(seen > n / 8);
    printf("brain guards   : out-of-table clade and short buffers handled; control (one bias word changed "
           "per clade) differs in %u of %u creatures\n", seen, n);
    free(alt); free(hid); free(mot);
}

/* a text that is not PTX must be refused cleanly */
static void test_bad_module(void)
{
    AzGpu *g = (AzGpu *)(uintptr_t)1;
    AzGpuInfo info;
    AzGpuOptions o;
    memset(&o, 0, sizeof o);
    o.ptx_text = "this is not PTX\n";
    int s = az_gpu_open(&g, &o, &info);
    if (s == AZ_GPU_NO_DRIVER || s == AZ_GPU_NO_DEVICE) return;   /* nothing to refuse it */
    CHECK_EQ(s, AZ_GPU_BAD_MODULE);
    CHECK(g == NULL);
    printf("bad-module path: status=%d \"%.110s...\"\n", s, info.msg);
}

int main(void)
{
    test_no_driver_paths();
    test_bad_module();

    /* AZ_GPU_DLL=<path> replaces the driver DLL: pointing it at a file that
     * does not exist rehearses this whole test on a machine without a driver */
    AzGpu *g = NULL;
    AzGpuInfo info;
    AzGpuOptions opt;
    memset(&opt, 0, sizeof opt);
    opt.dll_path = getenv("AZ_GPU_DLL");
    int s = az_gpu_open(&g, &opt, &info);
    if (s == AZ_GPU_NO_DRIVER || s == AZ_GPU_NO_DEVICE || s == AZ_GPU_NO_PTX) {
        printf("test_gpu: SKIPPED, the GPU comparisons did not run: %s (%s)\n",
               az_gpu_status_name(s), info.msg);
        return az_test_done();
    }
    CHECK_EQ(s, AZ_GPU_OK);
    if (s != AZ_GPU_OK) {
        printf("az_gpu_open: %s: %s\n", az_gpu_status_name(s), info.msg);
        return az_test_done();
    }
    printf("gpu            : %s\n", info.msg);

    test_philox(g);

    BrainData d;
    brain_data_make(&d, 1000000);
    test_brain_million(g, &d);
    test_brain_sizes(g, &d);
    test_brain_recurrent(g, &d);
    test_brain_guards(g, &d);
    brain_data_free(&d);

    CHECK_EQ(az_gpu_empty(g), AZ_GPU_OK);
    CHECK_EQ(az_gpu_sync(g), AZ_GPU_OK);
    az_gpu_close(g);
    return az_test_done();
}
