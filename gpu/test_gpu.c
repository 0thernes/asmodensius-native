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

/* ---- 4. TR-18: real sparse tiled brain (core/az_brain.c) & kinematics ---- */
static u64 pad8_u64(u64 x) { return (x + 7u) & ~7ull; }

static void build_random_tile_brain(u8 *dst, u32 n_hb, u32 n_t1, u32 n_t2)
{
    AzBrain *b = (AzBrain *)(void *)dst;
    memset(b, 0, sizeof *b);
    u32 at = (u32)sizeof(AzBrain);
    b->n_hb = (u16)n_hb; b->n_mb = (u16)(AZ_MOT / 8u);
    b->n_t1 = n_t1; b->n_t2 = n_t2;
    b->n_sg = (u8)(AZ_SENS / 4u); b->pad = 0;
    b->off_b1 = at;  at += 32u * n_hb;
    b->off_b2 = at;  at += 4u * AZ_MOT;
    b->off_c1 = at;  at += (u32)pad8_u64(2ull * n_hb);
    b->off_c2 = at;  at += (u32)pad8_u64(2ull * b->n_mb);
    b->off_in1 = at; at += (u32)pad8_u64(2ull * n_t1);
    b->off_in2 = at; at += (u32)pad8_u64(2ull * n_t2);
    b->off_w1 = at;  at += 32u * n_t1;
    b->off_w2 = at;
    b->shift1 = (u8)(td_u64() & 15u);
    b->shift2 = (u8)(td_u64() & 15u);
    for (u32 k = 0; k < 8; k++) {
        b->gain[k] = (u8)(1u + (td_u64() % 255u));
        b->bias[k] = (i8)(td_u64() & 255u);
    }
    i32 *b1 = (i32 *)(void *)(dst + b->off_b1);
    i32 *b2 = (i32 *)(void *)(dst + b->off_b2);
    u16 *c1 = (u16 *)(void *)(dst + b->off_c1);
    u16 *c2 = (u16 *)(void *)(dst + b->off_c2);
    u16 *in1 = (u16 *)(void *)(dst + b->off_in1);
    u16 *in2 = (u16 *)(void *)(dst + b->off_in2);
    i8 *w1 = (i8 *)(void *)(dst + b->off_w1);
    i8 *w2 = (i8 *)(void *)(dst + b->off_w2);
    for (u32 i = 0; i < 8u * n_hb; i++) b1[i] = (i32)(td_u64() % 2097153u) - 1048576;
    for (u32 i = 0; i < AZ_MOT; i++) b2[i] = (i32)(td_u64() % 2097153u) - 1048576;
    u32 rem1 = n_t1, max_g1 = (AZ_SENS / 4u) + 2u * n_hb;
    for (u32 hb = 0; hb < n_hb; hb++) {
        u32 cnt = (hb + 1u == n_hb) ? rem1 : (rem1 / (n_hb - hb));
        if (cnt > max_g1) cnt = max_g1;
        c1[hb] = (u16)cnt;
        rem1 -= cnt;
    }
    u32 pos = 0;
    for (u32 hb = 0; hb < n_hb; hb++) {
        for (u32 t = 0; t < c1[hb]; t++) in1[pos++] = (u16)(t % max_g1);
    }
    b->n_t1 = pos;
    u32 rem2 = n_t2, max_g2 = n_hb ? 2u * n_hb : 1u;
    for (u32 mb = 0; mb < b->n_mb; mb++) {
        u32 cnt = n_hb ? ((mb + 1u == b->n_mb) ? rem2 : (rem2 / (b->n_mb - mb))) : 0u;
        if (cnt > max_g2) cnt = max_g2;
        c2[mb] = (u16)cnt;
        rem2 -= cnt;
    }
    pos = 0;
    for (u32 mb = 0; mb < b->n_mb; mb++) {
        for (u32 t = 0; t < c2[mb]; t++) in2[pos++] = (u16)(t % max_g2);
    }
    b->n_t2 = pos;
    td_fill(w1, 32u * n_t1);
    td_fill(w2, 32u * n_t2);
}

static void test_tile_brain(AzGpu *g)
{
    /* 128 distinct variable-size sparse AzBrain templates (0..6 hidden blocks)
     * instantiated across 1,000,000 creatures (including 0-hidden and skipped
     * slots) and verified bit-for-bit against core/az_brain.c az_brain_step_mod_c. */
    const u32 n_tpl = 128, n = 1000000;
    u32 tpl_off[128], tpl_nhb[128];
    u64 store_bytes = 64;
    for (u32 t = 0; t < n_tpl; t++) {
        u32 nhb = (t == 0) ? 0u : (1u + (t % 6u));
        u32 nt1 = nhb ? (nhb * (3u + (t % 5u))) : 0u;
        u32 nt2 = nhb ? ((AZ_MOT / 8u) * (1u + (t % (2u * nhb)))) : 0u;
        u64 bbytes = pad8_u64(az_brain_bytes(nhb, nt1, nt2));
        tpl_off[t] = (u32)store_bytes;
        tpl_nhb[t] = nhb;
        store_bytes += bbytes;
    }
    u8 *store = xalloc((size_t)store_bytes);
    memset(store, 0, (size_t)store_bytes);
    for (u32 t = 0; t < n_tpl; t++) {
        u32 nhb = tpl_nhb[t];
        u32 nt1 = nhb ? (nhb * (3u + (t % 5u))) : 0u;
        u32 nt2 = nhb ? ((AZ_MOT / 8u) * (1u + (t % (2u * nhb)))) : 0u;
        build_random_tile_brain(store + tpl_off[t], nhb, nt1, nt2);
    }

    u32 *bo8 = xalloc((size_t)n * sizeof(u32));
    u32 *mo8 = xalloc((size_t)n * sizeof(u32));
    u32 *ho8 = xalloc((size_t)n * sizeof(u32));
    u8 *sens = xalloc((size_t)n * AZ_SENS);
    u8 *gain = xalloc((size_t)n * 8u);
    i8 *bias = xalloc((size_t)n * 8u);
    td_fill(sens, (size_t)n * AZ_SENS);
    for (size_t i = 0; i < (size_t)n * AZ_SENS; i++) sens[i] &= 127u;
    td_fill(gain, (size_t)n * 8u);
    td_fill(bias, (size_t)n * 8u);

    u64 m_u8 = 0, h_u8 = 0;
    for (u32 i = 0; i < n; i++) {
        if ((i & 63u) == 63u) {
            bo8[i] = 0xFFFFFFFFu; mo8[i] = 0u; ho8[i] = 0u;
            continue;
        }
        u32 t = i % n_tpl;
        bo8[i] = tpl_off[t] >> 3;
        mo8[i] = (u32)m_u8;
        ho8[i] = (u32)h_u8;
        m_u8 += (AZ_SENS / 8u) + 2u * tpl_nhb[t];
        h_u8 += tpl_nhb[t];
    }
    const u64 total_mind = m_u8 << 3;
    const u64 total_hid = h_u8 << 3;
    u8 *hid_in = xalloc((size_t)total_hid);
    u8 *hid_cpu = xalloc((size_t)total_hid);
    u8 *hid_gpu = xalloc((size_t)total_hid);
    i8 *mot_cpu = xalloc((size_t)n * AZ_MOT);
    i8 *mot_gpu = xalloc((size_t)n * AZ_MOT);
    td_fill(hid_in, (size_t)total_hid);
    for (size_t i = 0; i < (size_t)total_hid; i++) hid_in[i] &= 127u;
    memset(hid_cpu, 0, (size_t)total_hid);
    memset(hid_gpu, 0xA5, (size_t)total_hid);
    memset(mot_cpu, 0, (size_t)n * AZ_MOT);
    memset(mot_gpu, 0, (size_t)n * AZ_MOT);

    /* CPU reference via literal core/az_brain.c az_brain_step_mod_c */
    u8 mind_tmp[AZ_SENS + 16 * 8];
    for (u32 i = 0; i < n; i++) {
        if (bo8[i] == 0xFFFFFFFFu) continue;
        const AzBrain *br = (const AzBrain *)(const void *)(store + ((size_t)bo8[i] << 3));
        const u32 nh = 8u * (u32)br->n_hb;
        memcpy(mind_tmp, &sens[(size_t)i * AZ_SENS], AZ_SENS);
        if (nh) memcpy(mind_tmp + AZ_SENS, &hid_in[(size_t)ho8[i] << 3], nh);
        az_brain_step_mod_c(br, &gain[(size_t)i * 8u], &bias[(size_t)i * 8u], mind_tmp, &mot_cpu[(size_t)i * AZ_MOT]);
        if (nh) memcpy(&hid_cpu[(size_t)ho8[i] << 3], mind_tmp + AZ_SENS + nh, nh);
    }

    AzGpuTileBrain tb;
    memset(&tb, 0, sizeof tb);
    CHECK_EQ(az_gpu_alloc(g, &tb.store, store_bytes), AZ_GPU_OK);
    CHECK_EQ(az_gpu_alloc(g, &tb.brain_off8, (u64)n * sizeof(u32)), AZ_GPU_OK);
    CHECK_EQ(az_gpu_alloc(g, &tb.mind_off8, (u64)n * sizeof(u32)), AZ_GPU_OK);
    CHECK_EQ(az_gpu_alloc(g, &tb.hid_off8, (u64)n * sizeof(u32)), AZ_GPU_OK);
    CHECK_EQ(az_gpu_alloc(g, &tb.sens_in, (u64)n * AZ_SENS), AZ_GPU_OK);
    CHECK_EQ(az_gpu_alloc(g, &tb.hid_in, total_hid), AZ_GPU_OK);
    CHECK_EQ(az_gpu_alloc(g, &tb.gain_in, (u64)n * 8u), AZ_GPU_OK);
    CHECK_EQ(az_gpu_alloc(g, &tb.bias_in, (u64)n * 8u), AZ_GPU_OK);
    CHECK_EQ(az_gpu_alloc(g, &tb.mind_scratch, total_mind), AZ_GPU_OK);
    CHECK_EQ(az_gpu_alloc(g, &tb.hid_out, total_hid), AZ_GPU_OK);
    CHECK_EQ(az_gpu_alloc(g, &tb.motor_out, (u64)n * AZ_MOT), AZ_GPU_OK);

    CHECK_EQ(az_gpu_upload(g, tb.store, 0, store, store_bytes), AZ_GPU_OK);
    CHECK_EQ(az_gpu_upload(g, tb.brain_off8, 0, bo8, (u64)n * sizeof(u32)), AZ_GPU_OK);
    CHECK_EQ(az_gpu_upload(g, tb.mind_off8, 0, mo8, (u64)n * sizeof(u32)), AZ_GPU_OK);
    CHECK_EQ(az_gpu_upload(g, tb.hid_off8, 0, ho8, (u64)n * sizeof(u32)), AZ_GPU_OK);
    CHECK_EQ(az_gpu_upload(g, tb.sens_in, 0, sens, (u64)n * AZ_SENS), AZ_GPU_OK);
    CHECK_EQ(az_gpu_upload(g, tb.hid_in, 0, hid_in, total_hid), AZ_GPU_OK);
    CHECK_EQ(az_gpu_upload(g, tb.gain_in, 0, gain, (u64)n * 8u), AZ_GPU_OK);
    CHECK_EQ(az_gpu_upload(g, tb.bias_in, 0, bias, (u64)n * 8u), AZ_GPU_OK);
    CHECK_EQ(az_gpu_upload(g, tb.motor_out, 0, mot_gpu, (u64)n * AZ_MOT), AZ_GPU_OK);
    CHECK_EQ(az_gpu_tile_brain(g, &tb, n, false), AZ_GPU_OK);
    CHECK_EQ(az_gpu_download(g, hid_gpu, tb.hid_out, 0, total_hid), AZ_GPU_OK);
    CHECK_EQ(az_gpu_download(g, mot_gpu, tb.motor_out, 0, (u64)n * AZ_MOT), AZ_GPU_OK);

    u32 d_hid = 0, d_mot = 0;
    for (u32 i = 0; i < n; i++) {
        if (bo8[i] == 0xFFFFFFFFu) {
            d_mot += memcmp(&mot_gpu[(size_t)i * AZ_MOT], &mot_cpu[(size_t)i * AZ_MOT], AZ_MOT) != 0;
            continue;
        }
        const AzBrain *br = (const AzBrain *)(const void *)(store + ((size_t)bo8[i] << 3));
        const u32 nh = 8u * (u32)br->n_hb;
        if (nh) d_hid += memcmp(&hid_gpu[(size_t)ho8[i] << 3], &hid_cpu[(size_t)ho8[i] << 3], nh) != 0;
        d_mot += memcmp(&mot_gpu[(size_t)i * AZ_MOT], &mot_cpu[(size_t)i * AZ_MOT], AZ_MOT) != 0;
    }
    CHECK_EQ(d_hid, 0);
    CHECK_EQ(d_mot, 0);
    printf("tile brain 1M  : %u variable-size sparse AzBrains (0..48 hid, 128 templates), gpu vs cpu diffs: hid=%u motor=%u\n",
           n, d_hid, d_mot);

    az_gpu_free(g, &tb.store); az_gpu_free(g, &tb.brain_off8); az_gpu_free(g, &tb.mind_off8);
    az_gpu_free(g, &tb.hid_off8); az_gpu_free(g, &tb.sens_in); az_gpu_free(g, &tb.hid_in);
    az_gpu_free(g, &tb.gain_in); az_gpu_free(g, &tb.bias_in); az_gpu_free(g, &tb.mind_scratch);
    az_gpu_free(g, &tb.hid_out); az_gpu_free(g, &tb.motor_out);
    free(store); free(bo8); free(mo8); free(ho8); free(sens); free(gain); free(bias);
    free(hid_in); free(hid_cpu); free(hid_gpu); free(mot_cpu); free(mot_gpu);
}

static void test_kin_axis(AzGpu *g)
{
    const u32 n = 1000000;
    u32 *pos0 = xalloc((size_t)n * sizeof(u32));
    u32 *pos_c = xalloc((size_t)n * sizeof(u32));
    u32 *pos_g = xalloc((size_t)n * sizeof(u32));
    i16 *vel0 = xalloc((size_t)n * sizeof(i16));
    i16 *vel_c = xalloc((size_t)n * sizeof(i16));
    i16 *vel_g = xalloc((size_t)n * sizeof(i16));
    i16 *dv = xalloc((size_t)n * sizeof(i16));
    u8 *drag = xalloc(n);
    td_fill(pos0, (size_t)n * sizeof(u32));
    td_fill(vel0, (size_t)n * sizeof(i16));
    td_fill(dv, (size_t)n * sizeof(i16));
    td_fill(drag, n);
    /* extremes in first 6 lanes: clamping, wrap-around, full drag, zero drag */
    vel0[0] = 32767; dv[0] = 32767; drag[0] = 0; pos0[0] = 0xFFFFFFFFu;
    vel0[1] = -32768; dv[1] = -32768; drag[1] = 0; pos0[1] = 0u;
    vel0[2] = 16000; dv[2] = 0; drag[2] = 255; pos0[2] = 12345u;
    vel0[3] = -16000; dv[3] = 0; drag[3] = 255; pos0[3] = 0x80000000u;

    u32 shifts[3] = { 16u, 22u, 26u };
    u32 total_diff = 0;
    for (u32 k = 0; k < 3; k++) {
        memcpy(pos_c, pos0, (size_t)n * sizeof(u32));
        memcpy(pos_g, pos0, (size_t)n * sizeof(u32));
        memcpy(vel_c, vel0, (size_t)n * sizeof(i16));
        memcpy(vel_g, vel0, (size_t)n * sizeof(i16));
        az_kin_axis_c(pos_c, vel_c, dv, drag, n, shifts[k]);
        CHECK_EQ(az_gpu_kin_axis_host(g, pos_g, vel_g, dv, drag, n, shifts[k]), AZ_GPU_OK);
        total_diff += diff_records(pos_g, pos_c, n, sizeof(u32));
        total_diff += diff_records(vel_g, vel_c, n, sizeof(i16));
    }
    CHECK_EQ(total_diff, 0);
    printf("kin axis 1M    : %u entities x 3 shifts (16,22,26), gpu vs cpu differing lanes = %u\n", n, total_diff);
    free(pos0); free(pos_c); free(pos_g); free(vel0); free(vel_c); free(vel_g); free(dv); free(drag);
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

    test_tile_brain(g);
    test_kin_axis(g);

    CHECK_EQ(az_gpu_empty(g), AZ_GPU_OK);
    CHECK_EQ(az_gpu_sync(g), AZ_GPU_OK);
    az_gpu_close(g);
    return az_test_done();
}
