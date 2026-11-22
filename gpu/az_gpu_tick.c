/* az_gpu_tick.c - live tick GPU brain-step batch engine (TR-04 / TR-18).
 *
 * Bridges the freestanding AzHost.brain_batch hook in core/az_tick.c to the
 * pure-integer CUDA PTX kernel az_gpu_k_tile_brain (compiled from core/az_brain.c)
 * over nvcuda.dll with:
 *   - VRAM-resident active creature store (incremental tail sync on births;
 *     full resync only on first tick, compaction, or in-place block edits);
 *   - page-locked CUDA host staging (cuMemAllocHost_v2) and async DMA + launch
 *     on a dedicated CUDA stream (cuMemcpyHtoDAsync_v2 / cuMemcpyDtoHAsync_v2);
 *   - parallel CPU pack/unpack across all world workers via host->parallel_for
 *     using a private split array so w->splits stays intact for Phase 1c;
 *   - exact unmasked h2 + masked hid + masked motor writeback for bit-identical
 *     az_state_hash parity with CPU under every lesion and brain size.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "az_gpu.h"

struct AzGpuTick {
    AzGpu     *g;
    AzGpuInfo  info;
    u32        cap;

    /* host scratch shared with core/az_tick.c (Phase 1a -> 1c) */
    AzPercept *percepts;
    u8        *fin;

    /* per-tick parallel staging metadata */
    u16       *nhb;
    u32        splits[AZ_MAX_PARTS + 1];

    /* page-locked host staging buffers */
    u32       *h_brain_off8;
    u32       *h_mind_off8;
    u32       *h_hid_off8;
    u8        *h_sens_in;
    u8        *h_gain_in;
    i8        *h_bias_in;
    i8        *h_motor_out;
    u8        *h_hid_in;
    u8        *h_hid_out;
    u64        hid_cap;

    /* device VRAM buffers */
    AzGpuTileBrain tb;

    /* VRAM store synchronization state */
    const u8  *synced_ptr;
    u32        synced_top;
    u64        synced_gen;
    void      *reg_store_ptr;
    u64        reg_store_bytes;

    /* stats */
    LARGE_INTEGER qpf;
    AzGpuTickStats stats;
};

static u64 qpc_ns(const AzGpuTick *gt)
{
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    return (u64)(((double)c.QuadPart * 1e9) / (double)gt->qpf.QuadPart);
}

static void run_par(AzGpuTick *gt, AzWorld *w, AzJobFn fn, u32 n)
{
    if (!n) return;
    if (w->host && w->host->parallel_for && w->n_workers > 1 && n >= 1024u) {
        u32 parts = w->n_workers * 4u;
        if (w->host->max_parts && parts > w->host->max_parts) parts = w->host->max_parts;
        if (parts > AZ_MAX_PARTS) parts = AZ_MAX_PARTS;
        if (parts > n) parts = n;
        for (u32 k = 0; k <= parts; k++) gt->splits[k] = (u32)(((u64)n * k) / parts);
        w->host->parallel_for(w->host->ctx, w, fn, gt->splits, parts);
    } else {
        fn(w, 0, n, 0);
    }
}

/* Pass 1 (parallel): locate each active creature's AzBrain inside the active
 * half of w->store and record its hidden block count. */
static void job_gpu_measure(AzWorld *w, u32 b, u32 e, u32 worker)
{
    (void)worker;
    AzGpuTick *gt = (AzGpuTick *)w->host->gpu_ctx;
    const u8 *active_store = w->store + ((size_t)w->store_base << 6);
    for (u32 i = b; i < e; i++) {
        const u32 slot = w->order[i];
        if (w->flags[slot] & (AZ_F_EGG | AZ_F_DORMANT)) {
            gt->h_brain_off8[i] = 0xFFFFFFFFu;
            gt->nhb[i] = 0u;
            continue;
        }
        u32 bb = 0, mb = 0;
        const AzBrain *br = az_brain(w, slot, &bb);
        (void)az_seg(w, slot, AZ_SEG_MIND, &mb);
        if (bb >= sizeof(AzBrain) && mb >= AZ_SENS + 16u * br->n_hb) {
            const size_t off = (size_t)((const u8 *)(const void *)br - active_store);
            if ((off & 7u) == 0u && (off >> 3) < 0xFFFFFFFFull) {
                gt->h_brain_off8[i] = (u32)(off >> 3);
                gt->nhb[i] = br->n_hb;
                continue;
            }
        }
        gt->h_brain_off8[i] = 0xFFFFFFFFu;
        gt->nhb[i] = 0u;
    }
}

/* Pass 2 (parallel): copy per-tick mutable inputs (sens, current hid, live
 * gain/bias after Tier 1) into page-locked host staging buffers. */
static void job_gpu_pack(AzWorld *w, u32 b, u32 e, u32 worker)
{
    (void)worker;
    AzGpuTick *gt = (AzGpuTick *)w->host->gpu_ctx;
    for (u32 i = b; i < e; i++) {
        if (gt->h_brain_off8[i] == 0xFFFFFFFFu) continue;
        const u32 slot = w->order[i];
        u32 bb = 0, mb = 0;
        const AzBrain *br = az_brain(w, slot, &bb);
        const u8 *mind = (const u8 *)az_seg(w, slot, AZ_SEG_MIND, &mb);
        const u32 nh = 8u * (u32)gt->nhb[i];
        memcpy(&gt->h_sens_in[(size_t)i * AZ_SENS], mind, AZ_SENS);
        if (nh) memcpy(&gt->h_hid_in[(size_t)gt->h_hid_off8[i] << 3], mind + AZ_SENS, nh);
        memcpy(&gt->h_gain_in[(size_t)i * 8u], br->gain, 8u);
        memcpy(&gt->h_bias_in[(size_t)i * 8u], br->bias, 8u);
    }
}

/* Pass 3 (parallel): write raw unmasked h2 to mind[AZ_SENS+nh..+2*nh), masked
 * hid to mind[AZ_SENS..+nh), and masked motor to w->motor[slot*AZ_MOT..]. */
static void job_gpu_unpack(AzWorld *w, u32 b, u32 e, u32 worker)
{
    (void)worker;
    AzGpuTick *gt = (AzGpuTick *)w->host->gpu_ctx;
    const u32 lesion = w->cfg.lesion;
    const i8 motor_mask = (lesion & AZ_LESION_BRAIN) ? 0 : -1;
    const u8 hid_mask = (lesion & AZ_LESION_RECURRENCE) ? 0 : 0xFFu;
    for (u32 i = b; i < e; i++) {
        if (gt->h_brain_off8[i] == 0xFFFFFFFFu) continue;
        const u32 slot = w->order[i];
        u32 mb = 0;
        u8 *mind = (u8 *)az_seg(w, slot, AZ_SEG_MIND, &mb);
        const u32 nh = 8u * (u32)gt->nhb[i];
        if (nh) {
            u8 *hid = mind + AZ_SENS;
            const u8 *h2 = &gt->h_hid_out[(size_t)gt->h_hid_off8[i] << 3];
            memcpy(hid + nh, h2, nh);
            if (hid_mask == 0xFFu) memcpy(hid, h2, nh);
            else memset(hid, 0, nh);
        }
        i8 *motor = &w->motor[(size_t)slot * AZ_MOT];
        if (motor_mask == -1) memcpy(motor, &gt->h_motor_out[(size_t)i * AZ_MOT], AZ_MOT);
        else memset(motor, 0, AZ_MOT);
    }
}

static bool ensure_vram_buf(AzGpuTick *gt, AzGpuBuf *buf, u64 need, u64 headroom_pct)
{
    if (need < 64u) need = 64u;
    if (buf->bytes >= need) return true;
    u64 want = need + (need * headroom_pct) / 100u;
    if (want < need) want = need;
    want = (want + 4095u) & ~4095ull;
    u64 vfree = 0, vtot = 0;
    if (az_gpu_mem_info(gt->g, &vfree, &vtot) == AZ_GPU_OK) {
        u64 avail = vfree + buf->bytes;
        /* Keep at least 256 MiB free for Windows WDDM / desktop compositor. */
        if (avail <= (256ull << 20) || want > avail - (256ull << 20)) {
            if (avail > (256ull << 20) && need <= avail - (256ull << 20)) want = need;
            else return false;
        }
    }
    if (buf->dptr) az_gpu_free(gt->g, buf);
    return az_gpu_alloc(gt->g, buf, want) == AZ_GPU_OK;
}

static bool ensure_hid_buffers(AzGpuTick *gt, u64 need_hid)
{
    if (need_hid < 64u) need_hid = 64u;
    if (gt->hid_cap >= need_hid && gt->tb.hid_in.bytes >= need_hid && gt->tb.hid_out.bytes >= need_hid)
        return true;
    u64 want = need_hid + (need_hid / 4u);
    want = (want + 4095u) & ~4095ull;
    if (!ensure_vram_buf(gt, &gt->tb.hid_in, want, 0)) return false;
    if (!ensure_vram_buf(gt, &gt->tb.hid_out, want, 0)) return false;
    void *ni = az_gpu_host_alloc(gt->g, want);
    void *no = ni ? az_gpu_host_alloc(gt->g, want) : NULL;
    if (!ni || !no) {
        if (ni) az_gpu_host_free(gt->g, ni);
        if (no) az_gpu_host_free(gt->g, no);
        return false;
    }
    if (gt->h_hid_in) az_gpu_host_free(gt->g, gt->h_hid_in);
    if (gt->h_hid_out) az_gpu_host_free(gt->g, gt->h_hid_out);
    gt->h_hid_in = (u8 *)ni;
    gt->h_hid_out = (u8 *)no;
    gt->hid_cap = want;
    return true;
}

static bool gpu_brain_batch(void *ctx, AzWorld *w, u32 n_sorted)
{
    AzGpuTick *gt = (AzGpuTick *)ctx;
    if (!gt || !gt->g || n_sorted > gt->cap) {
        if (gt) gt->stats.ticks_fallback++;
        return false;
    }
    if (n_sorted == 0) {
        gt->stats.ticks_gpu++;
        return true;
    }

    const u64 t0 = qpc_ns(gt);
    /* 1. Parallel measure + sequential u16 prefix sum */
    run_par(gt, w, job_gpu_measure, n_sorted);

    u64 mind_off8 = 0, hid_off8 = 0, n_valid = 0;
    for (u32 i = 0; i < n_sorted; i++) {
        if (gt->h_brain_off8[i] == 0xFFFFFFFFu) {
            gt->h_mind_off8[i] = 0u;
            gt->h_hid_off8[i] = 0u;
            continue;
        }
        const u32 nhb = (u32)gt->nhb[i];
        const u64 m_units8 = (u64)(AZ_SENS / 8u) + 2ull * nhb;
        const u64 h_units8 = (u64)nhb;
        if (mind_off8 + m_units8 > 0xFFFFFFFFull || hid_off8 + h_units8 > 0xFFFFFFFFull) {
            gt->stats.ticks_fallback++;
            return false;
        }
        gt->h_mind_off8[i] = (u32)mind_off8;
        gt->h_hid_off8[i] = (u32)hid_off8;
        mind_off8 += m_units8;
        hid_off8 += h_units8;
        n_valid++;
    }
    if (n_valid == 0) {
        gt->stats.ticks_gpu++;
        return true;
    }

    const u64 total_mind = mind_off8 << 3;
    const u64 total_hid = hid_off8 << 3;
    const u64 need_store = (u64)w->store_top << 6;

    if (!ensure_hid_buffers(gt, total_hid) ||
        !ensure_vram_buf(gt, &gt->tb.mind_scratch, total_mind, 25u)) {
        gt->stats.ticks_fallback++;
        return false;
    }
    if (need_store > gt->tb.store.bytes) {
        if (!ensure_vram_buf(gt, &gt->tb.store, need_store, 25u)) {
            gt->stats.ticks_fallback++;
            return false;
        }
        gt->synced_ptr = NULL;
        gt->synced_top = 0u;
    }

    /* 2. Parallel pack of live per-tick inputs into page-locked host staging */
    run_par(gt, w, job_gpu_pack, n_sorted);
    const u64 t1 = qpc_ns(gt);
    gt->stats.pack_ns += t1 - t0;

    /* 3. Incremental VRAM store sync + async H2D transfers */
    const u8 *active_store = w->store + ((size_t)w->store_base << 6);
    if (w->store && !gt->reg_store_ptr && w->store_units > 0) {
        /* Best-effort CUDA host pin on the VirtualLock'd store arena for full PCIe DMA bandwidth */
        const u64 reg_bytes = (u64)w->store_units * 128ull;
        if (az_gpu_host_register(gt->g, w->store, reg_bytes) == AZ_GPU_OK) {
            gt->reg_store_ptr = w->store;
            gt->reg_store_bytes = reg_bytes;
        }
    }
    int rc = AZ_GPU_OK;
    if (need_store > 0) {
        if (active_store != gt->synced_ptr || w->store_in_place_gen != gt->synced_gen ||
            w->store_top < gt->synced_top) {
            rc = az_gpu_upload_async(gt->g, gt->tb.store, 0, active_store, need_store);
            if (!rc) {
                gt->synced_ptr = active_store;
                gt->synced_gen = w->store_in_place_gen;
                gt->synced_top = w->store_top;
                w->store_sync_top = w->store_top;
                w->store_dirty_n = 0u;
                gt->stats.store_bytes_tx += need_store;
            }
        } else {
            /* Upload only the handful of blocks whose AZ_SEG_BRAIN shifted in place */
            for (u32 d = 0; !rc && d < w->store_dirty_n; d++) {
                const u32 h = w->store_dirty_h[d];
                const u32 u = w->store_dirty_u[d];
                if (h >= gt->synced_top || u == 0) continue;
                const u64 off = (u64)h << 6;
                u64 len = (u64)u << 6;
                if (off + len > need_store) len = need_store - off;
                rc = az_gpu_upload_async(gt->g, gt->tb.store, off, active_store + off, len);
                if (!rc) gt->stats.store_bytes_tx += len;
            }
            if (!rc) w->store_dirty_n = 0u;
            /* Upload any newborn tail appended since last tick */
            if (!rc && w->store_top > gt->synced_top) {
                const u64 off = (u64)gt->synced_top << 6;
                const u64 delta = need_store - off;
                rc = az_gpu_upload_async(gt->g, gt->tb.store, off, active_store + off, delta);
                if (!rc) {
                    gt->synced_top = w->store_top;
                    w->store_sync_top = w->store_top;
                    gt->stats.store_bytes_tx += delta;
                }
            }
        }
    }

    if (!rc) rc = az_gpu_upload_async(gt->g, gt->tb.brain_off8, 0, gt->h_brain_off8, (u64)n_sorted * sizeof(u32));
    if (!rc) rc = az_gpu_upload_async(gt->g, gt->tb.mind_off8, 0, gt->h_mind_off8, (u64)n_sorted * sizeof(u32));
    if (!rc) rc = az_gpu_upload_async(gt->g, gt->tb.hid_off8, 0, gt->h_hid_off8, (u64)n_sorted * sizeof(u32));
    if (!rc) rc = az_gpu_upload_async(gt->g, gt->tb.sens_in, 0, gt->h_sens_in, (u64)n_sorted * AZ_SENS);
    if (!rc && total_hid) rc = az_gpu_upload_async(gt->g, gt->tb.hid_in, 0, gt->h_hid_in, total_hid);
    if (!rc) rc = az_gpu_upload_async(gt->g, gt->tb.gain_in, 0, gt->h_gain_in, (u64)n_sorted * 8u);
    if (!rc) rc = az_gpu_upload_async(gt->g, gt->tb.bias_in, 0, gt->h_bias_in, (u64)n_sorted * 8u);
    const u64 t2 = qpc_ns(gt);
    gt->stats.h2d_ns += t2 - t1;

    /* 4. Launch real freestanding core/az_brain.c PTX kernel on GPU + async D2H */
    if (!rc) rc = az_gpu_tile_brain(gt->g, &gt->tb, n_sorted, true);
    if (!rc && total_hid) rc = az_gpu_download_async(gt->g, gt->h_hid_out, gt->tb.hid_out, 0, total_hid);
    if (!rc) rc = az_gpu_download_async(gt->g, gt->h_motor_out, gt->tb.motor_out, 0, (u64)n_sorted * AZ_MOT);
    if (!rc) rc = az_gpu_stream_sync(gt->g);
    const u64 t3 = qpc_ns(gt);
    gt->stats.kernel_ns += t3 - t2;

    if (rc) {
        gt->synced_ptr = NULL;
        gt->synced_top = 0u;
        w->store_sync_top = 0u;
        gt->stats.ticks_fallback++;
        return false;
    }

    /* 5. Parallel unpack into creature minds and motors */
    run_par(gt, w, job_gpu_unpack, n_sorted);
    const u64 t4 = qpc_ns(gt);
    gt->stats.unpack_ns += t4 - t3;

    gt->stats.ticks_gpu++;
    gt->stats.brains_eval += n_valid;
    gt->stats.vram_store_bytes = gt->tb.store.bytes;
    gt->stats.vram_scratch_bytes = gt->tb.brain_off8.bytes + gt->tb.mind_off8.bytes +
                                   gt->tb.hid_off8.bytes + gt->tb.sens_in.bytes +
                                   gt->tb.hid_in.bytes + gt->tb.gain_in.bytes +
                                   gt->tb.bias_in.bytes + gt->tb.mind_scratch.bytes +
                                   gt->tb.hid_out.bytes + gt->tb.motor_out.bytes;
    return true;
}

int az_gpu_tick_create(AzGpuTick **out, u32 cap, AzGpuInfo *info)
{
    if (!out || cap == 0) return AZ_GPU_BAD_ARG;
    *out = NULL;
    AzGpuTick *gt = (AzGpuTick *)calloc(1, sizeof *gt);
    if (!gt) return AZ_GPU_ERROR;
    QueryPerformanceFrequency(&gt->qpf);
    gt->cap = cap;

    int rc = az_gpu_open(&gt->g, NULL, &gt->info);
    if (info) *info = gt->info;
    if (rc != AZ_GPU_OK) {
        free(gt);
        return rc;
    }

    gt->percepts = (AzPercept *)calloc(cap, sizeof(AzPercept));
    gt->fin = (u8 *)calloc(cap, sizeof(u8));
    gt->nhb = (u16 *)calloc(cap, sizeof(u16));

    gt->h_brain_off8 = (u32 *)az_gpu_host_alloc(gt->g, (u64)cap * sizeof(u32));
    gt->h_mind_off8 = (u32 *)az_gpu_host_alloc(gt->g, (u64)cap * sizeof(u32));
    gt->h_hid_off8 = (u32 *)az_gpu_host_alloc(gt->g, (u64)cap * sizeof(u32));
    gt->h_sens_in = (u8 *)az_gpu_host_alloc(gt->g, (u64)cap * AZ_SENS);
    gt->h_gain_in = (u8 *)az_gpu_host_alloc(gt->g, (u64)cap * 8u);
    gt->h_bias_in = (i8 *)az_gpu_host_alloc(gt->g, (u64)cap * 8u);
    gt->h_motor_out = (i8 *)az_gpu_host_alloc(gt->g, (u64)cap * AZ_MOT);

    bool ok = gt->percepts && gt->fin && gt->nhb &&
              gt->h_brain_off8 && gt->h_mind_off8 && gt->h_hid_off8 &&
              gt->h_sens_in && gt->h_gain_in && gt->h_bias_in && gt->h_motor_out;
    if (ok) ok = az_gpu_alloc(gt->g, &gt->tb.brain_off8, (u64)cap * sizeof(u32)) == AZ_GPU_OK;
    if (ok) ok = az_gpu_alloc(gt->g, &gt->tb.mind_off8, (u64)cap * sizeof(u32)) == AZ_GPU_OK;
    if (ok) ok = az_gpu_alloc(gt->g, &gt->tb.hid_off8, (u64)cap * sizeof(u32)) == AZ_GPU_OK;
    if (ok) ok = az_gpu_alloc(gt->g, &gt->tb.sens_in, (u64)cap * AZ_SENS) == AZ_GPU_OK;
    if (ok) ok = az_gpu_alloc(gt->g, &gt->tb.gain_in, (u64)cap * 8u) == AZ_GPU_OK;
    if (ok) ok = az_gpu_alloc(gt->g, &gt->tb.bias_in, (u64)cap * 8u) == AZ_GPU_OK;
    if (ok) ok = az_gpu_alloc(gt->g, &gt->tb.motor_out, (u64)cap * AZ_MOT) == AZ_GPU_OK;
    if (ok) ok = ensure_hid_buffers(gt, (u64)cap * 32u);
    if (ok) ok = ensure_vram_buf(gt, &gt->tb.mind_scratch, (u64)cap * (AZ_SENS + 64u), 0);

    if (!ok) {
        if (info) snprintf(info->msg, sizeof info->msg, "az_gpu_tick_create: allocation failed (%s)", az_gpu_error(gt->g));
        az_gpu_tick_destroy(gt);
        return AZ_GPU_ERROR;
    }

    *out = gt;
    return AZ_GPU_OK;
}

void az_gpu_tick_attach(AzGpuTick *gt, AzHost *host)
{
    if (!gt || !host) return;
    host->brain_batch = gpu_brain_batch;
    host->gpu_ctx = gt;
    host->gpu_percepts = gt->percepts;
    host->gpu_fin = gt->fin;
    host->gpu_cap = gt->cap;
}

void az_gpu_tick_detach(AzGpuTick *gt, AzHost *host)
{
    (void)gt;
    if (!host) return;
    host->brain_batch = NULL;
    host->gpu_ctx = NULL;
    host->gpu_percepts = NULL;
    host->gpu_fin = NULL;
    host->gpu_cap = 0;
}

void az_gpu_tick_stats(const AzGpuTick *gt, AzGpuTickStats *out)
{
    if (!out) return;
    if (!gt) { memset(out, 0, sizeof *out); return; }
    *out = gt->stats;
}

void az_gpu_tick_destroy(AzGpuTick *gt)
{
    if (!gt) return;
    if (gt->g) {
        if (gt->reg_store_ptr) {
            az_gpu_host_unregister(gt->g, gt->reg_store_ptr);
            gt->reg_store_ptr = NULL;
        }
        az_gpu_free(gt->g, &gt->tb.store);
        az_gpu_free(gt->g, &gt->tb.brain_off8);
        az_gpu_free(gt->g, &gt->tb.mind_off8);
        az_gpu_free(gt->g, &gt->tb.hid_off8);
        az_gpu_free(gt->g, &gt->tb.sens_in);
        az_gpu_free(gt->g, &gt->tb.hid_in);
        az_gpu_free(gt->g, &gt->tb.gain_in);
        az_gpu_free(gt->g, &gt->tb.bias_in);
        az_gpu_free(gt->g, &gt->tb.mind_scratch);
        az_gpu_free(gt->g, &gt->tb.hid_out);
        az_gpu_free(gt->g, &gt->tb.motor_out);
        az_gpu_host_free(gt->g, gt->h_brain_off8);
        az_gpu_host_free(gt->g, gt->h_mind_off8);
        az_gpu_host_free(gt->g, gt->h_hid_off8);
        az_gpu_host_free(gt->g, gt->h_sens_in);
        az_gpu_host_free(gt->g, gt->h_gain_in);
        az_gpu_host_free(gt->g, gt->h_bias_in);
        az_gpu_host_free(gt->g, gt->h_motor_out);
        az_gpu_host_free(gt->g, gt->h_hid_in);
        az_gpu_host_free(gt->g, gt->h_hid_out);
        az_gpu_close(gt->g);
    }
    free(gt->percepts);
    free(gt->fin);
    free(gt->nhb);
    free(gt);
}
