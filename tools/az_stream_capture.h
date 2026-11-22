/* Extraction runs between ticks. All pointers read here belong to this world only. */
#include "az_stream.h"
#include <math.h>

static void stream_put32(u8 *p, u32 x) { memcpy(p, &x, sizeof x); }
static void stream_put64(u8 *p, u64 x) { memcpy(p, &x, sizeof x); }

static u8 *stream_static(const AzWorld *w, u32 *size)
{
    u32 cols = w->gx * w->gy;
    *size = 32 + 4u * w->vent_count + w->n_cells + 4 * cols;
    u8 *p = calloc(1, *size);
    if (!p) return NULL;
    u32 head[] = {w->gx, w->gy, w->gz, w->n_cells, w->vent_count,
                  w->sun_period[0], w->sun_period[1], w->cfg.field_stride};
    memcpy(p, head, 32);
    memcpy(p + 32, w->vent_cell, 4u * w->vent_count);
    u8 *rock = p + 32 + 4u * w->vent_count;
    memcpy(rock, w->c_solid, w->n_cells);
    for (u32 col = 0; col < cols; col++) {
        u32 z = w->gz;
        while (z && rock[col + (z - 1) * cols] != 255) z--;
        u32 height = z * 256u;
        if (z < w->gz) height += rock[col + z * cols];
        stream_put32(rock + w->n_cells + 4u * col, height);
    }
    return p;
}

static u8 *stream_fields(const AzWorld *w, u32 *size)
{
    u32 cols = w->gx * w->gy;
    *size = 32 + 4 * cols + 4 * w->n_cells;
    u8 *p = calloc(1, *size);
    if (!p) return NULL;
    stream_put64(p, w->tick);
    u32 head[] = {w->gx, w->gy, w->gz, 3};
    memcpy(p + 8, head, 16);
    u8 *cells = p + 32 + 4 * cols;
    for (u32 i = 0; i < w->n_cells; i++) {
        const u32 *sp = w->c_sp + (size_t)i * AZ_NSPECIES;
        if ((w->c_phase[i] & 3) == AZ_PH_LIQUID && sp[AZ_SP_SOLV])
            stream_put32(p + 32 + 4 * (i % cols), (i / cols + 1) * 256u);
        u32 org = sp[AZ_SP_ORG_Y];
        cells[4 * i] = (u8)w->c_phase[i];
        cells[4 * i + 1] = w->c_light[i];
        cells[4 * i + 2] = (u8)az_clamp_i32(w->c_temp[i] >> 7, 0, 255);
        cells[4 * i + 3] = org ? (u8)az_min_u32(255, 16u * (32u - (u32)__builtin_clz(org))) : 0;
    }
    return p;
}

typedef struct {
    u8 *snapshot;
    AzStreamRecord *scratch;
    u32 *buckets;
    u32 *uid_index;
    size_t index_count;
} StreamCapture;

static void stream_sort(StreamCapture *b, AzStreamRecord *a, u32 n, u64 changes)
{
    AzStreamRecord *src = a, *dst = b->scratch;
    for (u32 shift = 0; shift < 64; shift += 16) {
        if (!((changes >> shift) & 65535u)) continue;
        memset(b->buckets, 0, 65536 * sizeof(u32));
        for (u32 i = 0; i < n; i++) b->buckets[(src[i].uid >> shift) & 65535u]++;
        u32 off = 0;
        for (u32 i = 0; i < 65536; i++) {
            u32 len = b->buckets[i];
            b->buckets[i] = off;
            off += len;
        }
        for (u32 i = 0; i < n; i++) dst[b->buckets[(src[i].uid >> shift) & 65535u]++] = src[i];
        AzStreamRecord *tmp = src; src = dst; dst = tmp;
    }
    if (src != a) memcpy(a, src, (size_t)n * sizeof *a);
}

static u32 stream_creatures(const AzWorld *w, StreamCapture *b)
{
    AzStreamHeader *h = (AzStreamHeader *)b->snapshot;
    AzStreamRecord *records = (AzStreamRecord *)(b->snapshot + sizeof *h);
    /* Fit an inverse order map to the live UID span. Map entries are small
     * and the final gather writes entire cache lines in output order. */
    u64 low = UINT64_MAX, high = 0, previous = 0;
    bool ordered = true;
    u32 live = 0;
    for (u32 i = 0; i < w->n_slots; i++) {
        if (!(w->flags[i] & AZ_F_ALIVE)) continue;
        u64 uid = w->uid[i];
        if (uid < low) low = uid;
        if (uid > high) high = uid;
        if (live && previous >= uid) ordered = false;
        previous = uid;
        live++;
    }
    bool dense = !ordered && high - low < (u64)live * 2 && high - low < b->index_count;
    size_t span = dense ? (size_t)(high - low + 1) : 0;
    if (dense) memset(b->uid_index, 0, span * sizeof *b->uid_index);
    AzStreamRecord *linear = dense ? b->scratch : records;
    u32 n = 0;
    u64 changes = 0;
    for (u32 i = 0; i < w->n_slots; i++) {
        if (!(w->flags[i] & AZ_F_ALIVE)) continue;
        u64 uid = w->uid[i];
        u32 index = n++;
        if (dense) {
            b->uid_index[uid - low] = index + 1;
        } else if (!ordered) {
            changes |= low ^ uid;
        }
        AzStreamRecord *r = &linear[index];
        r->uid = uid;
        r->x = (u16)(w->pos_x[i] >> 16); r->y = (u16)(w->pos_y[i] >> 16); r->z = (u16)(w->pos_z[i] >> 16);
        r->yaw = 0; r->pitch = 0;
        if (w->rot_b[i] || w->rot_c[i]) {
            const i16 rot[] = {w->rot_w[i], w->rot_a[i], w->rot_b[i], w->rot_c[i]};
            const i32 forward[] = {16384, 0, 0};
            i32 axis[3];
            az_rotor_apply(axis, rot, forward);
            r->yaw = (u8)lroundf(atan2f((float)axis[1], (float)axis[0]) * 40.74366543f);
            r->pitch = (i8)lroundf(atan2f((float)axis[2], hypotf((float)axis[0], (float)axis[1])) * 80.85071109f);
        }
        r->size = (u8)az_radius_code(w->mass[i]);
        r->hue = (u8)(w->tag[i] >> 8); r->pattern = (u8)w->tag[i];
        r->flags = w->flags[i]; r->clade = w->clade[i];
    }
    if (dense) {
        u32 count = 0;
        for (size_t j = 0; j < span; j++) {
            if (j + 24 < span) {
                u32 next = b->uid_index[j + 24];
                if (next) __builtin_prefetch(&linear[next - 1], 0, 1);
            }
            u32 index = b->uid_index[j];
            if (index) records[count++] = linear[index - 1];
        }
    } else if (!ordered) {
        stream_sort(b, records, n, changes);
    }
    memset(h, 0, sizeof *h);
    h->tick = w->tick; h->count = n; h->stride = sizeof(AzStreamRecord);
    h->grid[0] = w->cfg.gx_bits; h->grid[1] = w->cfg.gy_bits; h->grid[2] = w->cfg.gz_bits;
    h->mode = w->cfg.mode == AZ_MODE_WILD; h->clades = w->n_clades;
    memcpy(h->light, w->c_lvec + (size_t)(w->n_cells - w->gx * w->gy) * 4, 4);
    return n;
}

static void stream_inspect(const AzWorld *w, u64 uid, char *text, size_t size)
{
    for (u32 i = 0; i < w->n_slots; i++) {
        if (!(w->flags[i] & AZ_F_ALIVE) || w->uid[i] != uid) continue;
        u32 a, b, nb, np;
        (void)az_genes(w, i, 0, &a); (void)az_genes(w, i, 1, &b);
        const AzBrain *brain = az_brain(w, i, &nb);
        (void)az_parts(w, i, &np);
        snprintf(text, size, "{\"event\":\"inspect\",\"uid\":\"%016" PRIx64 "\",\"found\":true,"
                 "\"tick\":%" PRIu64 ",\"slot_valid_this_tick\":%u,\"genes\":[%u,%u],"
                 "\"neurons\":%u,\"tiles\":%u,\"parts\":%u,\"units\":%u,\"energy\":%d,"
                 "\"mass\":%u,\"health\":%u,\"clade\":%u,\"lineage\":%u,\"generation\":%u,"
                 "\"parent_a\":\"%016" PRIx64 "\",\"parent_b\":\"%016" PRIx64 "\","
                 "\"age\":%" PRIu64 ",\"colony\":%u,\"flags\":%u,\"pos\":[%u,%u,%u]}",
                 uid, w->tick, i, a, b, nb >= sizeof *brain ? brain->n_hb * 8u : 0u,
                 nb >= sizeof *brain ? brain->n_t1 + brain->n_t2 : 0u, np, az_blk(w, i)->units,
                 w->energy[i], w->mass[i], w->health[i], w->clade[i], w->lineage[i], w->generation[i],
                 w->parent_a[i], w->parent_b[i], w->tick - w->birth_tick[i], w->colony[i], w->flags[i],
                 w->pos_x[i] >> 16, w->pos_y[i] >> 16, w->pos_z[i] >> 16);
        return;
    }
    snprintf(text, size, "{\"event\":\"inspect\",\"uid\":\"%016" PRIx64 "\",\"found\":false,\"tick\":%" PRIu64 "}", uid, w->tick);
}
