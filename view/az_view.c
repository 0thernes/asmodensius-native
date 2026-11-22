/* az_view.c - software renderer: world box, field-tinted floor, creatures as
 * depth-buffered shaded bodies, text overlay and clade legend.
 *
 * Hosted C with floating point. The world is read through a const pointer
 * and never written (rule D7). Nothing here depends on slot order, time or
 * addresses: the same world and camera always give the same pixels.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "az_view.h"

/* lineages the viewer keeps a shape and a count for; a creature of a later
 * record is drawn with the default shape */
#define VIEW_MAX_CLADES 4096

#define VIEW_FOV_TAN 0.36397f   /* tan(20 deg): 40 degree vertical field of view */
#define VIEW_NEAR    0.05f      /* near plane, cells                             */
#define SHAPE_N      48         /* octahedral shape table is SHAPE_N x SHAPE_N   */
#define OWNER_NONE   0xFFFFFFFFu

typedef struct V3 { float x, y, z; } V3;

/* radial shape of one clade's body plan: radius as a function of direction */
typedef struct Shape {
    u8    key[16];              /* the body bytes it was built from (slots are reused) */
    float elong;                /* stretch along the forward axis                 */
    float spike_amp;            /* species spike amplitude                        */
    float gmax;                 /* bound of base + spike over all directions      */
    float base[SHAPE_N * SHAPE_N];
    float spike[SHAPE_N * SHAPE_N];
} Shape;

struct AzView {
    u32 w, h;
    u32   *px;                  /* 0xAARRGGBB                                     */
    float *zb;                  /* depth along the view axis, cells               */
    u32   *own;                 /* uid of the creature that owns the pixel        */
    /* camera request */
    float yaw, pitch, dist, target[3];
    bool  auto_target;
    /* camera of the current frame */
    V3    cpos, cr, cu, cf;
    float focal, cx, cy;
    float fog_near, fog_far;
    /* per-frame scratch */
    float *floor_rgb; u32 floor_n;
    u32   *count;               /* living members per clade index                 */
    Shape *shape[VIEW_MAX_CLADES];
    AzViewInfo info;
};

/* ---------------------------------------------------------------------- */
/* 5x7 font, ASCII 32..126; bit 4 = left column                            */
/* ---------------------------------------------------------------------- */
static const u8 font5x7[95][7] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* space */
    {0x04,0x04,0x04,0x04,0x04,0x00,0x04}, /* ! */
    {0x0A,0x0A,0x0A,0x00,0x00,0x00,0x00}, /* " */
    {0x0A,0x0A,0x1F,0x0A,0x1F,0x0A,0x0A}, /* # */
    {0x04,0x0F,0x14,0x0E,0x05,0x1E,0x04}, /* $ */
    {0x18,0x19,0x02,0x04,0x08,0x13,0x03}, /* % */
    {0x0C,0x12,0x14,0x08,0x15,0x12,0x0D}, /* & */
    {0x04,0x04,0x08,0x00,0x00,0x00,0x00}, /* ' */
    {0x02,0x04,0x08,0x08,0x08,0x04,0x02}, /* ( */
    {0x08,0x04,0x02,0x02,0x02,0x04,0x08}, /* ) */
    {0x00,0x04,0x15,0x0E,0x15,0x04,0x00}, /* * */
    {0x00,0x04,0x04,0x1F,0x04,0x04,0x00}, /* + */
    {0x00,0x00,0x00,0x00,0x0C,0x04,0x08}, /* , */
    {0x00,0x00,0x00,0x1F,0x00,0x00,0x00}, /* - */
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C}, /* . */
    {0x00,0x01,0x02,0x04,0x08,0x10,0x00}, /* / */
    {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}, /* 0 */
    {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E}, /* 1 */
    {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F}, /* 2 */
    {0x1F,0x02,0x04,0x02,0x01,0x11,0x0E}, /* 3 */
    {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}, /* 4 */
    {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E}, /* 5 */
    {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E}, /* 6 */
    {0x1F,0x01,0x02,0x04,0x08,0x08,0x08}, /* 7 */
    {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}, /* 8 */
    {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C}, /* 9 */
    {0x00,0x0C,0x0C,0x00,0x0C,0x0C,0x00}, /* : */
    {0x00,0x0C,0x0C,0x00,0x0C,0x04,0x08}, /* ; */
    {0x02,0x04,0x08,0x10,0x08,0x04,0x02}, /* < */
    {0x00,0x00,0x1F,0x00,0x1F,0x00,0x00}, /* = */
    {0x08,0x04,0x02,0x01,0x02,0x04,0x08}, /* > */
    {0x0E,0x11,0x01,0x02,0x04,0x00,0x04}, /* ? */
    {0x0E,0x11,0x01,0x0D,0x15,0x15,0x0E}, /* @ */
    {0x0E,0x11,0x11,0x11,0x1F,0x11,0x11}, /* A */
    {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E}, /* B */
    {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E}, /* C */
    {0x1C,0x12,0x11,0x11,0x11,0x12,0x1C}, /* D */
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}, /* E */
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10}, /* F */
    {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F}, /* G */
    {0x11,0x11,0x11,0x1F,0x11,0x11,0x11}, /* H */
    {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E}, /* I */
    {0x07,0x02,0x02,0x02,0x02,0x12,0x0C}, /* J */
    {0x11,0x12,0x14,0x18,0x14,0x12,0x11}, /* K */
    {0x10,0x10,0x10,0x10,0x10,0x10,0x1F}, /* L */
    {0x11,0x1B,0x15,0x15,0x11,0x11,0x11}, /* M */
    {0x11,0x11,0x19,0x15,0x13,0x11,0x11}, /* N */
    {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}, /* O */
    {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10}, /* P */
    {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D}, /* Q */
    {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}, /* R */
    {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E}, /* S */
    {0x1F,0x04,0x04,0x04,0x04,0x04,0x04}, /* T */
    {0x11,0x11,0x11,0x11,0x11,0x11,0x0E}, /* U */
    {0x11,0x11,0x11,0x11,0x11,0x0A,0x04}, /* V */
    {0x11,0x11,0x11,0x15,0x15,0x15,0x0A}, /* W */
    {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11}, /* X */
    {0x11,0x11,0x11,0x0A,0x04,0x04,0x04}, /* Y */
    {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F}, /* Z */
    {0x0E,0x08,0x08,0x08,0x08,0x08,0x0E}, /* [ */
    {0x00,0x10,0x08,0x04,0x02,0x01,0x00}, /* \ */
    {0x0E,0x02,0x02,0x02,0x02,0x02,0x0E}, /* ] */
    {0x04,0x0A,0x11,0x00,0x00,0x00,0x00}, /* ^ */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x1F}, /* _ */
    {0x08,0x04,0x02,0x00,0x00,0x00,0x00}, /* ` */
    {0x00,0x00,0x0E,0x01,0x0F,0x11,0x0F}, /* a */
    {0x10,0x10,0x16,0x19,0x11,0x11,0x1E}, /* b */
    {0x00,0x00,0x0E,0x10,0x10,0x11,0x0E}, /* c */
    {0x01,0x01,0x0D,0x13,0x11,0x11,0x0F}, /* d */
    {0x00,0x00,0x0E,0x11,0x1F,0x10,0x0E}, /* e */
    {0x06,0x09,0x08,0x1C,0x08,0x08,0x08}, /* f */
    {0x00,0x0F,0x11,0x11,0x0F,0x01,0x0E}, /* g */
    {0x10,0x10,0x16,0x19,0x11,0x11,0x11}, /* h */
    {0x04,0x00,0x0C,0x04,0x04,0x04,0x0E}, /* i */
    {0x02,0x00,0x06,0x02,0x02,0x12,0x0C}, /* j */
    {0x10,0x10,0x12,0x14,0x18,0x14,0x12}, /* k */
    {0x0C,0x04,0x04,0x04,0x04,0x04,0x0E}, /* l */
    {0x00,0x00,0x1A,0x15,0x15,0x11,0x11}, /* m */
    {0x00,0x00,0x16,0x19,0x11,0x11,0x11}, /* n */
    {0x00,0x00,0x0E,0x11,0x11,0x11,0x0E}, /* o */
    {0x00,0x00,0x1E,0x11,0x1E,0x10,0x10}, /* p */
    {0x00,0x00,0x0D,0x13,0x0F,0x01,0x01}, /* q */
    {0x00,0x00,0x16,0x19,0x10,0x10,0x10}, /* r */
    {0x00,0x00,0x0E,0x10,0x0E,0x01,0x1E}, /* s */
    {0x08,0x08,0x1C,0x08,0x08,0x09,0x06}, /* t */
    {0x00,0x00,0x11,0x11,0x11,0x13,0x0D}, /* u */
    {0x00,0x00,0x11,0x11,0x11,0x0A,0x04}, /* v */
    {0x00,0x00,0x11,0x11,0x15,0x15,0x0A}, /* w */
    {0x00,0x00,0x11,0x0A,0x04,0x0A,0x11}, /* x */
    {0x00,0x00,0x11,0x11,0x0F,0x01,0x0E}, /* y */
    {0x00,0x00,0x1F,0x02,0x04,0x08,0x1F}, /* z */
    {0x02,0x04,0x04,0x08,0x04,0x04,0x02}, /* { */
    {0x04,0x04,0x04,0x04,0x04,0x04,0x04}, /* | */
    {0x08,0x04,0x04,0x02,0x04,0x04,0x08}, /* } */
    {0x00,0x00,0x08,0x15,0x02,0x00,0x00}  /* ~ */
};

u8 az_view_font_row(u32 ch, u32 row)
{
    if (ch < 32 || ch > 126 || row >= 7) return 0;
    return font5x7[ch - 32][row];
}

/* ---------------------------------------------------------------------- */
/* clade glyph                                                             */
/* ---------------------------------------------------------------------- */
/* 16 bits -> 9 rows x 4 free columns (the 4th is the mirror axis) = 36
 * cells. The even cells 0, 2, .. 30 carry a bijective scramble of the 16
 * input bits, so two different inputs never give the same bitmap; the other
 * 20 cells are filled from a second hash to make the shapes less regular. */
void az_view_glyph(u8 glyph_a, u8 glyph_b, u8 rows[AZ_GLYPH_H])
{
    u32 x = ((u32)glyph_a << 8) | glyph_b;
    u32 m = x;                                   /* each step is invertible mod 2^16 */
    m = (m * 0x9E37u + 0x7F4Bu) & 0xFFFFu;       /* odd multiplier */
    m ^= m >> 7;
    m = (m * 0x2C8Du) & 0xFFFFu;
    m ^= m >> 9;
    u32 fill = (u32)(az_mix64(0xA5A5F00Dull + x) >> 20);

    u32 fi = 0;
    for (u32 r = 0; r < AZ_GLYPH_H; r++) {
        u32 half = 0;                            /* bit 3 = leftmost column, bit 0 = centre */
        for (u32 c = 0; c < 4; c++) {
            u32 cell = r * 4 + c, bit;
            if ((cell & 1u) == 0 && cell / 2 < 16) bit = (m >> (cell / 2)) & 1u;
            else bit = (fill >> fi++) & 1u;
            half = (half << 1) | bit;
        }
        /* columns 0 1 2 3 2 1 0 */
        u32 mirror = ((half >> 1) & 1u) << 2 | ((half >> 2) & 1u) << 1 | ((half >> 3) & 1u);
        rows[r] = (u8)((half << 3) | mirror);
    }
}

/* ---------------------------------------------------------------------- */
/* small helpers                                                           */
/* ---------------------------------------------------------------------- */
static inline V3 v3(float x, float y, float z) { V3 a = { x, y, z }; return a; }
static inline V3 v3_add(V3 a, V3 b) { return v3(a.x + b.x, a.y + b.y, a.z + b.z); }
static inline V3 v3_sub(V3 a, V3 b) { return v3(a.x - b.x, a.y - b.y, a.z - b.z); }
static inline V3 v3_scale(V3 a, float s) { return v3(a.x * s, a.y * s, a.z * s); }
static inline float v3_dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline V3 v3_cross(V3 a, V3 b)
{
    return v3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}
static inline V3 v3_norm(V3 a)
{
    float l = sqrtf(v3_dot(a, a));
    return l > 1e-12f ? v3_scale(a, 1.0f / l) : v3(0, 0, 1);
}
static inline float clamp01(float x) { return x < 0 ? 0 : (x > 1 ? 1 : x); }

/* linear colour -> 0xAARRGGBB with gamma 2 */
static inline u32 pack_rgb(float r, float g, float b)
{
    u32 R = (u32)(sqrtf(clamp01(r)) * 255.0f + 0.5f);
    u32 G = (u32)(sqrtf(clamp01(g)) * 255.0f + 0.5f);
    u32 B = (u32)(sqrtf(clamp01(b)) * 255.0f + 0.5f);
    return 0xFF000000u | (R << 16) | (G << 8) | B;
}

/* ---------------------------------------------------------------------- */
/* life cycle, camera                                                      */
/* ---------------------------------------------------------------------- */
AzView *az_view_create(u32 w, u32 h)
{
    if (w == 0 || h == 0 || w > 16384 || h > 16384) return NULL;
    AzView *v = (AzView *)calloc(1, sizeof *v);
    if (!v) return NULL;
    size_t n = (size_t)w * h;
    v->w = w; v->h = h;
    v->px = (u32 *)calloc(n, sizeof(u32));
    v->zb = (float *)calloc(n, sizeof(float));
    v->own = (u32 *)calloc(n, sizeof(u32));
    v->count = (u32 *)calloc(VIEW_MAX_CLADES, sizeof(u32));
    if (!v->px || !v->zb || !v->own || !v->count) { az_view_destroy(v); return NULL; }
    v->yaw = 0.6f; v->pitch = 0.5f; v->dist = 0.0f; v->auto_target = true;
    return v;
}

void az_view_destroy(AzView *v)
{
    if (!v) return;
    for (u32 k = 0; k < VIEW_MAX_CLADES; k++) free(v->shape[k]);
    free(v->floor_rgb); free(v->count);
    free(v->px); free(v->zb); free(v->own);
    free(v);
}

void az_view_camera(AzView *v, float yaw, float pitch, float distance, const float target[3])
{
    if (!v) return;
    v->yaw = yaw;
    v->pitch = pitch < -1.55f ? -1.55f : (pitch > 1.55f ? 1.55f : pitch);
    v->dist = distance;
    v->auto_target = (target == NULL);
    if (target) { v->target[0] = target[0]; v->target[1] = target[1]; v->target[2] = target[2]; }
}

const u32 *az_view_pixels(const AzView *v) { return v ? v->px : NULL; }
u32 az_view_width(const AzView *v) { return v ? v->w : 0; }
u32 az_view_height(const AzView *v) { return v ? v->h : 0; }
void az_view_info(const AzView *v, AzViewInfo *out) { if (v && out) *out = v->info; }

u64 az_view_hash(const AzView *v)
{
    if (!v) return 0;
    u64 h = 0x9E3779B97F4A7C15ull ^ (((u64)v->w << 32) | v->h);
    size_t n = (size_t)v->w * v->h;
    for (size_t i = 0; i < n; i++) h = az_mix64(h ^ v->px[i]) + i;
    return az_mix64(h);
}

/* place the camera on its orbit at distance d */
static void cam_place(AzView *v, V3 tgt, float d)
{
    float cp = cosf(v->pitch), sp = sinf(v->pitch);
    V3 dir = v3(cp * cosf(v->yaw), cp * sinf(v->yaw), sp);   /* target -> camera */
    v->cpos = v3_add(tgt, v3_scale(dir, d));
    v->cf = v3_scale(dir, -1.0f);
    v->cr = v3_norm(v3_cross(v->cf, v3(0, 0, 1)));
    v->cu = v3_cross(v->cr, v->cf);
}

/* are all eight corners of the box inside 94% of the frame? */
static bool cam_fits(const AzView *v, V3 box)
{
    for (u32 k = 0; k < 8; k++) {
        V3 p = v3_sub(v3((k & 1) ? box.x : 0, (k & 2) ? box.y : 0, (k & 4) ? box.z : 0), v->cpos);
        float zc = v3_dot(p, v->cf);
        if (zc <= VIEW_NEAR) return false;
        float sx = v3_dot(p, v->cr) / zc * v->focal, sy = v3_dot(p, v->cu) / zc * v->focal;
        if (fabsf(sx) > 0.47f * (float)v->w || fabsf(sy) > 0.47f * (float)v->h) return false;
    }
    return true;
}

static void cam_setup(AzView *v, const AzWorld *w)
{
    V3 box = v3((float)w->gx, (float)w->gy, (float)w->gz);
    V3 tgt = v->auto_target ? v3_scale(box, 0.5f) : v3(v->target[0], v->target[1], v->target[2]);
    v->focal = 0.5f * (float)v->h / VIEW_FOV_TAN;
    v->cx = 0.5f * (float)v->w; v->cy = 0.5f * (float)v->h;
    float rbox = 0.5f * sqrtf(v3_dot(box, box));
    float d = v->dist;
    if (!(d > 0.0f)) {
        float lo = 0.25f * rbox, hi = 16.0f * rbox;    /* fits at hi for any sane aspect */
        for (u32 it = 0; it < 28; it++) {
            float mid = 0.5f * (lo + hi);
            cam_place(v, tgt, mid);
            if (cam_fits(v, box)) hi = mid; else lo = mid;
        }
        d = hi;
    }
    cam_place(v, tgt, d);
    float dc = sqrtf(v3_dot(v3_sub(v3_scale(box, 0.5f), v->cpos), v3_sub(v3_scale(box, 0.5f), v->cpos)));
    v->fog_near = dc - rbox; v->fog_far = dc + rbox;
}

bool az_view_camera_pose(AzView *v, const AzWorld *world, AzCameraPose *pose)
{
    if (!v || !world || !pose) return false;
    cam_setup(v, world);
    *pose = (AzCameraPose){
        .eye = {v->cpos.x, v->cpos.y, v->cpos.z},
        .right = {v->cr.x, v->cr.y, v->cr.z},
        .up = {v->cu.x, v->cu.y, v->cu.z},
        .forward = {v->cf.x, v->cf.y, v->cf.z},
        .focal = v->focal
    };
    pose->target[0] = v->auto_target ? 0.5f * (float)world->gx : v->target[0];
    pose->target[1] = v->auto_target ? 0.5f * (float)world->gy : v->target[1];
    pose->target[2] = v->auto_target ? 0.5f * (float)world->gz : v->target[2];
    return true;
}

/* ---------------------------------------------------------------------- */
/* background: sky, field-tinted floor, world box                          */
/* ---------------------------------------------------------------------- */
/* One colour per (x, y) column: mean light and total organic matter. */
static bool floor_build(AzView *v, const AzWorld *w)
{
    u32 n = w->gx * w->gy;
    if (v->floor_n != n) {
        float *p = (float *)realloc(v->floor_rgb, (size_t)n * 3 * sizeof(float));
        if (!p) return false;
        v->floor_rgb = p; v->floor_n = n;
    }
    u64 org_max = 0;
    for (u32 c = 0; c < n; c++) {
        u64 org = 0;
        for (u32 z = 0; z < w->gz; z++) org += w->c_sp[(size_t)(c + z * n) * AZ_NSPECIES + AZ_SP_ORG_Y];
        if (org > org_max) org_max = org;
    }
    float org_scale = 1.0f / logf(2.0f + (float)org_max);
    for (u32 c = 0; c < n; c++) {
        u64 org = 0; u32 light = 0;
        for (u32 z = 0; z < w->gz; z++) {
            size_t cell = (size_t)c + (size_t)z * n;
            org += w->c_sp[cell * AZ_NSPECIES + AZ_SP_ORG_Y];
            light += w->c_light[cell];
        }
        float L = (float)light / (255.0f * (float)w->gz);
        float O = logf(1.0f + (float)org) * org_scale;
        float *rgb = &v->floor_rgb[(size_t)c * 3];
        rgb[0] = 0.012f + 0.20f * L + 0.03f * O;
        rgb[1] = 0.016f + 0.17f * L + 0.22f * O;
        rgb[2] = 0.030f + 0.09f * L + 0.05f * O;
    }
    return true;
}

static void background(AzView *v, const AzWorld *w)
{
    bool have_floor = floor_build(v, w);
    float gx = (float)w->gx, gy = (float)w->gy;
    u32 mx = w->gx - 1, my = w->gy - 1;
    float line_every = w->gx > 16 ? 4.0f : 1.0f;
    for (u32 py = 0; py < v->h; py++) {
        float t = (float)py / (float)v->h;
        float sky[3] = { 0.004f + 0.010f * t, 0.006f + 0.016f * t, 0.016f + 0.040f * t };
        float dy = v->cy - ((float)py + 0.5f);
        V3 rowd = v3_add(v3_scale(v->cf, v->focal), v3_scale(v->cu, dy));
        for (u32 pxi = 0; pxi < v->w; pxi++) {
            size_t i = (size_t)py * v->w + pxi;
            float dx = (float)pxi + 0.5f - v->cx;
            V3 d = v3_add(rowd, v3_scale(v->cr, dx));
            float rgb[3] = { sky[0], sky[1], sky[2] };
            float depth = INFINITY;
            float s = (fabsf(d.z) > 1e-9f) ? -v->cpos.z / d.z : -1.0f;
            if (have_floor && s > 0.0f) {
                float fx = v->cpos.x + s * d.x, fy = v->cpos.y + s * d.y;
                if (fx >= 0.0f && fx < gx && fy >= 0.0f && fy < gy) {
                    /* bilinear between column centres; x and y wrap like the world */
                    float ux = fx - 0.5f, uy = fy - 0.5f;
                    float flx = floorf(ux), fly = floorf(uy);
                    float ax = ux - flx, ay = uy - fly;
                    u32 x0 = (u32)(i32)flx & mx, y0 = (u32)(i32)fly & my;
                    u32 x1 = (x0 + 1) & mx, y1 = (y0 + 1) & my;
                    const float *c00 = &v->floor_rgb[(size_t)(y0 * w->gx + x0) * 3];
                    const float *c10 = &v->floor_rgb[(size_t)(y0 * w->gx + x1) * 3];
                    const float *c01 = &v->floor_rgb[(size_t)(y1 * w->gx + x0) * 3];
                    const float *c11 = &v->floor_rgb[(size_t)(y1 * w->gx + x1) * 3];
                    float lx = fx / line_every, ly = fy / line_every;
                    float edge = fminf(lx - floorf(lx), ly - floorf(ly)) * line_every;
                    float dim = edge < 0.06f ? 0.6f : 1.0f;
                    for (u32 k = 0; k < 3; k++)
                        rgb[k] = dim * ((c00[k] * (1 - ax) + c10[k] * ax) * (1 - ay) +
                                        (c01[k] * (1 - ax) + c11[k] * ax) * ay);
                    depth = s * v->focal;
                }
            }
            v->px[i] = pack_rgb(rgb[0], rgb[1], rgb[2]);
            v->zb[i] = depth;
            v->own[i] = OWNER_NONE;
        }
    }
}

/* depth-tested line between two world points */
static void line3(AzView *v, V3 a, V3 b, float r, float g, float bl)
{
    V3 pa = v3_sub(a, v->cpos), pb = v3_sub(b, v->cpos);
    float za = v3_dot(pa, v->cf), zb = v3_dot(pb, v->cf);
    if (za < VIEW_NEAR && zb < VIEW_NEAR) return;
    if (za < VIEW_NEAR) { float t = (VIEW_NEAR - za) / (zb - za); pa = v3_add(pa, v3_scale(v3_sub(pb, pa), t)); za = VIEW_NEAR; }
    if (zb < VIEW_NEAR) { float t = (VIEW_NEAR - zb) / (za - zb); pb = v3_add(pb, v3_scale(v3_sub(pa, pb), t)); zb = VIEW_NEAR; }
    float xa = v->cx + v3_dot(pa, v->cr) / za * v->focal, ya = v->cy - v3_dot(pa, v->cu) / za * v->focal;
    float xb = v->cx + v3_dot(pb, v->cr) / zb * v->focal, yb = v->cy - v3_dot(pb, v->cu) / zb * v->focal;
    float len = fmaxf(fabsf(xb - xa), fabsf(yb - ya));
    if (!(len < 65536.0f)) return;
    u32 steps = (u32)len + 1;
    u32 col = pack_rgb(r, g, bl);
    for (u32 k = 0; k <= steps; k++) {
        float t = (float)k / (float)steps;
        float x = xa + (xb - xa) * t, y = ya + (yb - ya) * t;
        if (x < 0.0f || y < 0.0f || x >= (float)v->w || y >= (float)v->h) continue;
        float z = 1.0f / ((1.0f - t) / za + t / zb);    /* 1/z is linear on screen */
        size_t i = (size_t)(u32)y * v->w + (u32)x;
        if (z <= v->zb[i] * 1.0005f) { v->px[i] = col; v->zb[i] = fminf(z, v->zb[i]); v->own[i] = OWNER_NONE; }
    }
}

static void world_box(AzView *v, const AzWorld *w)
{
    V3 b = v3((float)w->gx, (float)w->gy, (float)w->gz);
    for (u32 k = 0; k < 8; k++) {
        V3 p = v3((k & 1) ? b.x : 0, (k & 2) ? b.y : 0, (k & 4) ? b.z : 0);
        for (u32 ax = 0; ax < 3; ax++) {
            if (k & (1u << ax)) continue;                /* each edge once, from its low corner */
            V3 q = p;
            if (ax == 0) q.x = b.x; else if (ax == 1) q.y = b.y; else q.z = b.z;
            float top = (p.z > 0 || ax == 2) ? 1.0f : 0.7f;
            line3(v, p, q, 0.20f * top, 0.30f * top, 0.50f * top);
        }
    }
}

/* ---------------------------------------------------------------------- */
/* body plans                                                              */
/* ---------------------------------------------------------------------- */
#define PI_F 3.14159265358979f
#define SPIKE_AMP_MAX 0.8f

/* Gielis superformula, period 2 pi * 4 / m in phi */
static float superformula(float phi, float m, float n1, float n2, float n3)
{
    float a = fabsf(cosf(0.25f * m * phi)), b = fabsf(sinf(0.25f * m * phi));
    float s = powf(a, n2) + powf(b, n3);
    return s > 1e-12f ? powf(s, -1.0f / n1) : 1e6f;
}

/* direction of a texel of the octahedral map */
static V3 octa_dir(u32 i, u32 j)
{
    float ox = 2.0f * (float)i / (float)(SHAPE_N - 1) - 1.0f;
    float oy = 2.0f * (float)j / (float)(SHAPE_N - 1) - 1.0f;
    float z = 1.0f - fabsf(ox) - fabsf(oy);
    float x = ox, y = oy;
    if (z < 0.0f) {
        x = (1.0f - fabsf(oy)) * (ox >= 0.0f ? 1.0f : -1.0f);
        y = (1.0f - fabsf(ox)) * (oy >= 0.0f ? 1.0f : -1.0f);
    }
    return v3_norm(v3(x, y, z));
}

/* Body frame: x forward, y left, z up. The plan is mirror symmetric in y. */
static void shape_build(Shape *sh, const u8 body[16])
{
    memcpy(sh->key, body, 16);
    float m  = 1.0f + (float)(body[AZ_B_SHAPE_M] & 7);
    float n1 = 0.8f + (float)body[AZ_B_SHAPE_N1] * (3.2f / 255.0f);
    float n2 = 0.6f + (float)body[AZ_B_SHAPE_N2] * (4.4f / 255.0f);
    float n3 = 0.6f + (float)body[AZ_B_SHAPE_N3] * (4.4f / 255.0f);
    bool around_up = (body[AZ_B_SYMMETRY] & 1u) != 0;   /* lobes around the up axis, else around forward */
    u32 nseg = body[AZ_B_SEGMENTS] % 6u;
    float ks = 2.0f + (float)(body[AZ_B_SPIKES] & 7u);
    sh->elong = 1.0f + (float)body[AZ_B_ELONG] * (1.5f / 255.0f);
    sh->spike_amp = (float)(body[AZ_B_SPIKES] >> 3) * (0.6f / 31.0f);

    /* scale the outline so that bodies of equal mass look equally large */
    float sum2 = 0.0f;
    for (u32 k = 0; k < 256; k++) {
        float r = superformula(PI_F * ((float)k + 0.5f) / 256.0f, m, n1, n2, n3);
        r = fminf(r, 8.0f);
        sum2 += r * r;
    }
    float norm = 1.0f / sqrtf(sum2 / 256.0f);

    sh->gmax = 0.0f;
    for (u32 j = 0; j < SHAPE_N; j++) {
        for (u32 i = 0; i < SHAPE_N; i++) {
            V3 d = octa_dir(i, j);
            float axis = around_up ? d.z : d.x;
            float phi = around_up ? atan2f(fabsf(d.y), d.x) : atan2f(fabsf(d.y), d.z);
            float sf = superformula(phi, m, n1, n2, n3) * norm;
            sf = sf < 0.45f ? 0.45f : (sf > 1.6f ? 1.6f : sf);
            float g = 1.0f + (sf - 1.0f) * (1.0f - axis * axis);     /* round at the poles */
            if (nseg >= 2) g *= 1.0f + 0.10f * cosf((float)nseg * PI_F * d.x);
            g = g < 0.35f ? 0.35f : (g > 1.8f ? 1.8f : g);
            float cp = cosf(ks * atan2f(fabsf(d.y), d.x));
            float ct = cosf(ks * asinf(d.z < -1.0f ? -1.0f : (d.z > 1.0f ? 1.0f : d.z)));
            float s = (cp > 0.0f && ct > 0.0f) ? powf(cp * ct, 8.0f) : 0.0f;
            sh->base[j * SHAPE_N + i] = g;
            sh->spike[j * SHAPE_N + i] = s;
            if (g + SPIKE_AMP_MAX * s > sh->gmax) sh->gmax = g + SPIKE_AMP_MAX * s;
        }
    }
}

void az_view_shape_table(const u8 body[16], AzShapeTable *out)
{
    Shape sh;
    shape_build(&sh, body);
    out->elong = (u16)lroundf(sh.elong * 256.0f);
    out->spike_amp = (u16)lroundf(sh.spike_amp * 256.0f);
    out->gmax = (u16)lroundf(sh.gmax * 256.0f);
    for (u32 i = 0; i < SHAPE_N * SHAPE_N; i++) {
        out->base[i] = (u8)lroundf((sh.base[i] - 0.35f) * (255.0f / 1.45f));
        out->spike[i] = (u8)lroundf(sh.spike[i] * 255.0f);
    }
}

static const Shape *shape_get(AzView *v, u32 clade, const u8 body[16])
{
    Shape *sh = v->shape[clade];
    if (!sh) {
        sh = (Shape *)malloc(sizeof *sh);
        if (!sh) return NULL;
        v->shape[clade] = sh;
        shape_build(sh, body);
    } else if (memcmp(sh->key, body, 16) != 0) {
        shape_build(sh, body);                   /* the clade slot was reused */
    }
    return sh;
}

/* radius factor in direction q (any length): base + amp * spike, bilinear */
static inline float shape_at(const Shape *sh, V3 q, float amp)
{
    float s = fabsf(q.x) + fabsf(q.y) + fabsf(q.z);
    if (s < 1e-20f) return 1.0f;
    float ox = q.x / s, oy = q.y / s;
    if (q.z < 0.0f) {
        float tx = (1.0f - fabsf(oy)) * (ox >= 0.0f ? 1.0f : -1.0f);
        float ty = (1.0f - fabsf(ox)) * (oy >= 0.0f ? 1.0f : -1.0f);
        ox = tx; oy = ty;
    }
    float u = (ox * 0.5f + 0.5f) * (float)(SHAPE_N - 1), w = (oy * 0.5f + 0.5f) * (float)(SHAPE_N - 1);
    i32 iu = (i32)u, iw = (i32)w;
    iu = iu < 0 ? 0 : (iu > SHAPE_N - 2 ? SHAPE_N - 2 : iu);
    iw = iw < 0 ? 0 : (iw > SHAPE_N - 2 ? SHAPE_N - 2 : iw);
    float au = u - (float)iu, aw = w - (float)iw;
    u32 k = (u32)iw * SHAPE_N + (u32)iu;
    float b = (sh->base[k] * (1 - au) + sh->base[k + 1] * au) * (1 - aw) +
              (sh->base[k + SHAPE_N] * (1 - au) + sh->base[k + SHAPE_N + 1] * au) * aw;
    float p = (sh->spike[k] * (1 - au) + sh->spike[k + 1] * au) * (1 - aw) +
              (sh->spike[k + SHAPE_N] * (1 - au) + sh->spike[k + SHAPE_N + 1] * au) * aw;
    return b + amp * p;
}

/* ---------------------------------------------------------------------- */
/* appearance                                                              */
/* ---------------------------------------------------------------------- */
typedef struct Look {
    float rgb[3];        /* albedo, linear                                      */
    float glow;          /* emitted fraction of the albedo                      */
    float sheen;         /* specular strength                                   */
    u32   pat_type;      /* 0 countershade, 1 bands, 2 spots, 3 stripes         */
    float pat_freq, pat_contrast;
    float dim;           /* eggs and cysts are dim                              */
} Look;

static void hsv_rgb(float h, float s, float val, float rgb[3])
{
    float h6 = h * 6.0f;
    u32 sector = (u32)h6 % 6u;
    float f = h6 - floorf(h6);
    float p = val * (1 - s), q = val * (1 - s * f), t = val * (1 - s * (1 - f));
    switch (sector) {
    case 0: rgb[0] = val; rgb[1] = t; rgb[2] = p; break;
    case 1: rgb[0] = q; rgb[1] = val; rgb[2] = p; break;
    case 2: rgb[0] = p; rgb[1] = val; rgb[2] = t; break;
    case 3: rgb[0] = p; rgb[1] = q; rgb[2] = val; break;
    case 4: rgb[0] = t; rgb[1] = p; rgb[2] = val; break;
    default: rgb[0] = val; rgb[1] = p; rgb[2] = q; break;
    }
}

static inline i32 clamp_byte(i32 x) { return x < 0 ? 0 : (x > 255 ? 255 : x); }

/* species baseline (body bytes) shifted by the individual's own traits */
static void look_of(Look *lk, const u8 body[16], const u8 *trait, u32 flags)
{
    i32 hue = ((i32)body[AZ_B_HUE] + (((i32)trait[AZ_T_HUE] - 128) * 3) / 8) & 255;   /* hue wraps */
    i32 sat = clamp_byte((i32)body[AZ_B_SAT] + ((i32)trait[AZ_T_SAT] - 128) / 2);
    i32 glow = clamp_byte((i32)body[AZ_B_GLOW] + ((i32)trait[AZ_T_GLOW] - 128));
    i32 sheen = clamp_byte((i32)body[AZ_B_SHEEN] + ((i32)trait[AZ_T_SHEEN] - 128));
    i32 pat = clamp_byte((i32)body[AZ_B_PATTERN] + ((i32)trait[AZ_T_PATTERN] - 128));
    hsv_rgb((float)hue / 256.0f, 0.30f + 0.70f * (float)sat / 255.0f, 0.80f, lk->rgb);
    float g = (float)glow / 255.0f;
    lk->glow = 0.7f * g * g;
    lk->sheen = 0.9f * (float)sheen / 255.0f;
    lk->pat_type = (u32)pat >> 6;
    lk->pat_freq = 2.0f + (float)(((u32)pat >> 3) & 7u);
    lk->pat_contrast = 0.25f + 0.07f * (float)((u32)pat & 7u);
    lk->dim = (flags & AZ_F_EGG) ? 0.40f : ((flags & AZ_F_DORMANT) ? 0.65f : 1.0f);
}

/* pattern brightness at body direction d (unit), 1 - contrast .. 1 */
static inline float pattern_at(const Look *lk, V3 d)
{
    float k;
    switch (lk->pat_type) {
    case 0:  k = 0.5f + 0.5f * d.z; break;                                      /* darker belly */
    case 1:  k = 0.5f + 2.0f * sinf(lk->pat_freq * PI_F * d.x); break;           /* bands */
    case 2:  k = 0.5f + 6.0f * (0.2f - sinf(lk->pat_freq * PI_F * d.x) *
                                       sinf(lk->pat_freq * PI_F * d.y) *
                                       sinf(lk->pat_freq * PI_F * d.z)); break;  /* spots */
    default: k = 0.5f + 2.0f * sinf(lk->pat_freq * PI_F * d.y); break;           /* stripes */
    }
    return 1.0f - lk->pat_contrast * (1.0f - clamp01(k));
}

static const V3 light_dir = { 0.35634832f, 0.25453451f, 0.89087081f };   /* (0.4, 0.2857, 1) normalised */

/* shade one fragment and write it if it wins the depth test (ties: lower uid) */
static inline bool fragment(AzView *v, size_t i, float depth, u32 uid, const Look *lk, V3 n, float pat)
{
    if (!(depth < v->zb[i] || (depth == v->zb[i] && uid < v->own[i]))) return false;
    float ndl = v3_dot(n, light_dir);
    float diff = 0.22f + 0.78f * (ndl > 0.0f ? ndl : 0.0f);
    V3 hv = v3_norm(v3_sub(light_dir, v->cf));
    float nh = v3_dot(n, hv);
    float spec = 0.0f;
    if (nh > 0.0f) { float s2 = nh * nh, s4 = s2 * s2, s8 = s4 * s4, s16 = s8 * s8; spec = lk->sheen * s16 * s16; }
    float fog = 0.5f * clamp01((depth - v->fog_near) / (v->fog_far - v->fog_near + 1e-6f));
    float lum = lk->dim * pat * (diff + lk->glow);
    float r = lk->rgb[0] * lum + lk->dim * spec, g = lk->rgb[1] * lum + lk->dim * spec, b = lk->rgb[2] * lum + lk->dim * spec;
    r += (0.008f - r) * fog; g += (0.012f - g) * fog; b += (0.030f - b) * fog;
    v->px[i] = pack_rgb(r, g, b);
    v->zb[i] = depth;
    v->own[i] = uid;
    return true;
}

/* small or passive body: a shaded sphere; always at least one pixel */
static bool draw_sphere(AzView *v, float sx, float sy, float zc, float radius, float rpx, u32 uid, const Look *lk)
{
    float pat = 1.0f - lk->pat_contrast * (lk->pat_type == 0 ? 0.25f : 0.5f);    /* mean of the pattern */
    bool any = false;
    if (rpx < 0.75f) {
        if (sx < 0.0f || sy < 0.0f || sx >= (float)v->w || sy >= (float)v->h) return false;
        return fragment(v, (size_t)(u32)sy * v->w + (u32)sx, zc - radius, uid, lk, v3_scale(v->cf, -1.0f), pat);
    }
    i32 x0 = (i32)floorf(sx - rpx), x1 = (i32)ceilf(sx + rpx);
    i32 y0 = (i32)floorf(sy - rpx), y1 = (i32)ceilf(sy + rpx);
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > (i32)v->w - 1) x1 = (i32)v->w - 1;
    if (y1 > (i32)v->h - 1) y1 = (i32)v->h - 1;
    float inv = 1.0f / rpx;
    for (i32 y = y0; y <= y1; y++) {
        for (i32 x = x0; x <= x1; x++) {
            float dx = ((float)x + 0.5f - sx) * inv, dy = (sy - ((float)y + 0.5f)) * inv;
            float d2 = dx * dx + dy * dy;
            if (d2 > 1.0f) continue;
            float nz = sqrtf(1.0f - d2);
            V3 n = v3_add(v3_add(v3_scale(v->cr, dx), v3_scale(v->cu, dy)), v3_scale(v->cf, -nz));
            any |= fragment(v, (size_t)y * v->w + (size_t)x, zc - nz * radius, uid, lk, n, pat);
        }
    }
    return any;
}

/* full silhouette: march the view ray through the radial shape of the clade */
static bool draw_body(AzView *v, const AzWorld *w, u32 s, const Shape *sh, float sx, float sy, float zc,
                      float radius, float scale, const Look *lk)
{
    const u8 *trait = &w->trait[(size_t)s * AZ_TRAITS];
    float amp = sh->spike_amp + ((float)trait[AZ_T_SPIKES] - 128.0f) * (0.2f / 128.0f);
    amp = amp < 0.0f ? 0.0f : (amp > SPIKE_AMP_MAX ? SPIKE_AMP_MAX : amp);
    float e = sh->elong;
    float rq = radius / cbrtf(e);                /* keep the volume when stretching */
    float bq = rq * sh->gmax;                    /* bounding sphere in the unstretched space */
    float bpx = bq * e * scale;

    /* body axes in the world, from the integer rotor exactly as the core rotates */
    i16 rot[4] = { w->rot_w[s], w->rot_a[s], w->rot_b[s], w->rot_c[s] };
    V3 ax[3];
    for (u32 k = 0; k < 3; k++) {
        i32 unit[3] = { 0, 0, 0 }, out[3];
        unit[k] = 16384;
        az_rotor_apply(out, rot, unit);
        ax[k] = v3_norm(v3((float)out[0], (float)out[1], (float)out[2]));
    }
    /* camera axes in the unstretched body space q = (x / e, y, z) */
    V3 crq = v3(v3_dot(v->cr, ax[0]) / e, v3_dot(v->cr, ax[1]), v3_dot(v->cr, ax[2]));
    V3 cuq = v3(v3_dot(v->cu, ax[0]) / e, v3_dot(v->cu, ax[1]), v3_dot(v->cu, ax[2]));
    V3 cfq = v3(v3_dot(v->cf, ax[0]) / e, v3_dot(v->cf, ax[1]), v3_dot(v->cf, ax[2]));
    float qa = v3_dot(cfq, cfq);

    i32 x0 = (i32)floorf(sx - bpx), x1 = (i32)ceilf(sx + bpx);
    i32 y0 = (i32)floorf(sy - bpx), y1 = (i32)ceilf(sy + bpx);
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > (i32)v->w - 1) x1 = (i32)v->w - 1;
    if (y1 > (i32)v->h - 1) y1 = (i32)v->h - 1;
    float inv_scale = 1.0f / scale, eps = 0.03f * rq;
    u32 uid = w->uid[s];
    bool any = false;
    for (i32 y = y0; y <= y1; y++) {
        for (i32 x = x0; x <= x1; x++) {
            float ox = ((float)x + 0.5f - sx) * inv_scale, oy = (sy - ((float)y + 0.5f)) * inv_scale;
            V3 oq = v3_add(v3_scale(crq, ox), v3_scale(cuq, oy));
            float qb = v3_dot(oq, cfq), qc = v3_dot(oq, oq) - bq * bq;
            float disc = qb * qb - qa * qc;
            if (disc <= 0.0f) continue;
            float sq = sqrtf(disc), t0 = (-qb - sq) / qa, t1 = (-qb + sq) / qa;
            size_t i = (size_t)y * v->w + (size_t)x;
            if (zc + t0 > v->zb[i]) continue;                     /* hidden even at its nearest point */
            i32 steps = (i32)((t1 - t0) * scale) + 3;
            steps = steps < 6 ? 6 : (steps > 24 ? 24 : steps);
            float tp = t0, th = 0.0f;
            bool hit = false;
            for (i32 k = 1; k <= steps && !hit; k++) {
                float t = t0 + (t1 - t0) * (float)k / (float)steps;
                V3 q = v3_add(oq, v3_scale(cfq, t));
                if (sqrtf(v3_dot(q, q)) - rq * shape_at(sh, q, amp) <= 0.0f) {
                    float lo = tp, hi = t;                         /* outside at lo, inside at hi */
                    for (u32 b = 0; b < 5; b++) {
                        float mid = 0.5f * (lo + hi);
                        V3 qm = v3_add(oq, v3_scale(cfq, mid));
                        if (sqrtf(v3_dot(qm, qm)) - rq * shape_at(sh, qm, amp) <= 0.0f) hi = mid; else lo = mid;
                    }
                    th = hi; hit = true;
                }
                tp = t;
            }
            if (!hit) continue;
            V3 q = v3_add(oq, v3_scale(cfq, th));
            float f0 = sqrtf(v3_dot(q, q)) - rq * shape_at(sh, q, amp);
            V3 qx = v3(q.x + eps, q.y, q.z), qy = v3(q.x, q.y + eps, q.z), qz = v3(q.x, q.y, q.z + eps);
            float gxq = sqrtf(v3_dot(qx, qx)) - rq * shape_at(sh, qx, amp) - f0;
            float gyq = sqrtf(v3_dot(qy, qy)) - rq * shape_at(sh, qy, amp) - f0;
            float gzq = sqrtf(v3_dot(qz, qz)) - rq * shape_at(sh, qz, amp) - f0;
            /* gradient back to the body frame (x was divided by e), then to the world */
            V3 nb = v3(gxq / e, gyq, gzq);
            V3 n = v3_norm(v3_add(v3_add(v3_scale(ax[0], nb.x), v3_scale(ax[1], nb.y)), v3_scale(ax[2], nb.z)));
            any |= fragment(v, i, zc + th, uid, lk, n, pattern_at(lk, v3_norm(q)));
        }
    }
    return any;
}

/* count the living per clade, pick the legend */
static void census(AzView *v, const AzWorld *w)
{
    memset(v->count, 0, VIEW_MAX_CLADES * sizeof(u32));
    for (u32 s = 0; s < w->n_slots; s++) {
        u32 fl = w->flags[s];
        if (!(fl & AZ_F_ALIVE)) continue;
        v->info.n_live++;
        if (fl & AZ_F_EGG) v->info.n_eggs++;
        u32 c = w->clade[s];
        if (c < VIEW_MAX_CLADES) v->count[c]++;
    }
    bool used[VIEW_MAX_CLADES / 64 * 64] = { false };
    for (u32 c = 0; c < VIEW_MAX_CLADES; c++) if (v->count[c]) v->info.n_clades++;
    for (u32 k = 0; k < AZ_VIEW_LEGEND; k++) {
        u32 best = AZ_NONE, best_n = 0;
        for (u32 c = 0; c < VIEW_MAX_CLADES; c++)
            if (!used[c] && v->count[c] > best_n) { best = c; best_n = v->count[c]; }   /* strict >: lower index wins ties */
        if (best == AZ_NONE) break;
        used[best] = true;
        v->info.top_clade[k] = best; v->info.top_count[k] = best_n;
        v->info.top_n = k + 1;
    }
}

static void bodies(AzView *v, const AzWorld *w)
{
    double ix = ldexp(1.0, -(int)w->shift_x), iy = ldexp(1.0, -(int)w->shift_y), iz = ldexp(1.0, -(int)w->shift_z);
    for (u32 s = 0; s < w->n_slots; s++) {
        u32 fl = w->flags[s];
        if (!(fl & AZ_F_ALIVE)) continue;
        V3 p = v3((float)((double)w->pos_x[s] * ix), (float)((double)w->pos_y[s] * iy), (float)((double)w->pos_z[s] * iz));
        V3 rel = v3_sub(p, v->cpos);
        float zc = v3_dot(rel, v->cf);
        float radius = (float)(az_radius_code(w->mass[s]) + 1u) / 32.0f;
        if (fl & AZ_F_EGG) radius *= 0.5f;                         /* eggs are small */
        if (zc < VIEW_NEAR + 2.0f * radius) continue;
        float scale = v->focal / zc;                              /* pixels per cell at this depth */
        float sx = v->cx + v3_dot(rel, v->cr) * scale, sy = v->cy - v3_dot(rel, v->cu) * scale;
        float rpx = radius * scale;
        if (sx < -4.0f * rpx - 2.0f || sy < -4.0f * rpx - 2.0f ||
            sx > (float)v->w + 4.0f * rpx + 2.0f || sy > (float)v->h + 4.0f * rpx + 2.0f) continue;
        u32 c = w->clade[s];
        const u8 *body = (c < w->n_clades) ? w->clades[c].body : w->clades[0].body;
        Look lk;
        look_of(&lk, body, &w->trait[(size_t)s * AZ_TRAITS], fl);
        const Shape *sh = NULL;
        bool passive = (fl & (AZ_F_EGG | AZ_F_DORMANT)) != 0;      /* eggs and cysts are plain spheres */
        if (!passive && rpx >= 1.6f && c < VIEW_MAX_CLADES) sh = shape_get(v, c, body);
        bool drawn;
        if (sh) {
            drawn = draw_body(v, w, s, sh, sx, sy, zc, radius, scale, &lk);
            if (drawn) v->info.n_marched++;
        } else {
            drawn = draw_sphere(v, sx, sy, zc, radius, rpx, w->uid[s], &lk);
        }
        if (drawn) v->info.n_drawn++;
    }
}

/* ---------------------------------------------------------------------- */
/* overlay                                                                 */
/* ---------------------------------------------------------------------- */
static void blend_rect(AzView *v, i32 x0, i32 y0, i32 x1, i32 y1, u32 rgb, u32 alpha)
{
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > (i32)v->w) x1 = (i32)v->w;
    if (y1 > (i32)v->h) y1 = (i32)v->h;
    for (i32 y = y0; y < y1; y++) {
        for (i32 x = x0; x < x1; x++) {
            u32 *p = &v->px[(size_t)y * v->w + (size_t)x];
            u32 out = 0xFF000000u;
            for (u32 sh = 0; sh < 24; sh += 8) {
                u32 a = (*p >> sh) & 255u, b = (rgb >> sh) & 255u;
                out |= ((a * (255u - alpha) + b * alpha + 127u) / 255u) << sh;
            }
            *p = out;
        }
    }
}

static void fill_rect(AzView *v, i32 x0, i32 y0, i32 x1, i32 y1, u32 rgb)
{
    blend_rect(v, x0, y0, x1, y1, rgb, 255);
}

static void put_text(AzView *v, i32 x, i32 y, const char *s, i32 sc, u32 rgb)
{
    for (; *s; s++, x += 6 * sc) {
        for (u32 r = 0; r < 7; r++) {
            u8 row = az_view_font_row((u8)*s, r);
            for (u32 c = 0; c < 5; c++)
                if (row & (0x10u >> c))
                    fill_rect(v, x + (i32)c * sc, y + (i32)r * sc, x + (i32)(c + 1) * sc, y + (i32)(r + 1) * sc, rgb);
        }
    }
}

static void put_glyph(AzView *v, i32 x, i32 y, const u8 rows[AZ_GLYPH_H], i32 sc, u32 rgb)
{
    for (u32 r = 0; r < AZ_GLYPH_H; r++)
        for (u32 c = 0; c < AZ_GLYPH_W; c++)
            if (rows[r] & (0x40u >> c))
                fill_rect(v, x + (i32)c * sc, y + (i32)r * sc, x + (i32)(c + 1) * sc, y + (i32)(r + 1) * sc, rgb);
}

static void overlay(AzView *v, const AzWorld *w)
{
    i32 sc = v->w >= 960 ? 2 : 1;
    i32 m = 6 * sc, pad = 3 * sc;
    char line[96];
    snprintf(line, sizeof line, "TICK %u  LIVING %u  CLADES %u", v->info.tick, v->info.n_live, v->info.n_clades);
    i32 tw = (i32)strlen(line) * 6 * sc;
    blend_rect(v, m - pad, m - pad, m + tw + pad, m + 7 * sc + pad, 0x000000u, 150);
    put_text(v, m, m, line, sc, 0xE8F0FFu);

    /* legend: glyph in the clade's baseline colour, clade index, living count */
    i32 row_h = (AZ_GLYPH_H + 3) * sc;
    i32 top = m + 7 * sc + 3 * pad;
    i32 lw = (AZ_GLYPH_W + 3) * sc + 16 * 6 * sc;
    if (v->info.top_n)
        blend_rect(v, m - pad, top - pad, m + lw + pad, top + (i32)v->info.top_n * row_h + pad - 3 * sc, 0x000000u, 150);
    for (u32 k = 0; k < v->info.top_n; k++) {
        u32 c = v->info.top_clade[k];
        const u8 *body = (c < w->n_clades) ? w->clades[c].body : w->clades[0].body;
        u8 rows[AZ_GLYPH_H];
        az_view_glyph(body[AZ_B_GLYPH_A], body[AZ_B_GLYPH_B], rows);
        float rgb[3];
        hsv_rgb((float)body[AZ_B_HUE] / 256.0f, 0.30f + 0.70f * (float)body[AZ_B_SAT] / 255.0f, 0.95f, rgb);
        i32 y = top + (i32)k * row_h;
        put_glyph(v, m, y, rows, sc, pack_rgb(rgb[0], rgb[1], rgb[2]) & 0xFFFFFFu);
        snprintf(line, sizeof line, "C%-5u %u", c, v->info.top_count[k]);
        put_text(v, m + (AZ_GLYPH_W + 3) * sc, y + sc, line, sc, 0xE8F0FFu);
    }
}

/* ---------------------------------------------------------------------- */
/* render                                                                  */
/* ---------------------------------------------------------------------- */
void az_view_render(AzView *v, const AzWorld *world)
{
    if (!v || !world) return;
    memset(&v->info, 0, sizeof v->info);
    v->info.tick = world->tick;
    cam_setup(v, world);
    background(v, world);
    world_box(v, world);
    census(v, world);
    bodies(v, world);
    overlay(v, world);
}

/* This list is also the lifetime contract for an off-thread proof render.
 * Start with a zero world so a capture can never refer to engine-owned memory. */
#define AZ_SCENE_COLUMNS(X) \
    X(flags, n_slots) X(uid, n_slots) X(clade, n_slots) X(mass, n_slots) \
    X(pos_x, n_slots) X(pos_y, n_slots) X(pos_z, n_slots) \
    X(rot_w, n_slots) X(rot_a, n_slots) X(rot_b, n_slots) X(rot_c, n_slots) \
    X(trait, n_slots * (size_t)AZ_TRAITS) X(clades, n_clades) \
    X(c_light, n_cells) X(c_sp, n_cells * (size_t)AZ_NSPECIES)

void az_view_capture_free(AzWorld *s)
{
    if (!s) return;
#define RELEASE(field, count) free(s->field);
    AZ_SCENE_COLUMNS(RELEASE)
#undef RELEASE
    free(s);
}

AzWorld *az_view_capture(const AzWorld *w)
{
    AzWorld *s = calloc(1, sizeof *s);
    if (!s) return NULL;
    s->gx = w->gx; s->gy = w->gy; s->gz = w->gz;
    s->shift_x = w->shift_x; s->shift_y = w->shift_y; s->shift_z = w->shift_z;
    s->n_slots = w->n_slots; s->n_clades = w->n_clades; s->n_cells = w->n_cells;
    s->tick = w->tick;
#define COPY(field, count) do { \
        size_t bytes = (w->count) * sizeof(*w->field); \
        if (bytes) { \
            s->field = malloc(bytes); \
            if (!s->field) { az_view_capture_free(s); return NULL; } \
            memcpy(s->field, w->field, bytes); \
        } \
    } while (0);
    AZ_SCENE_COLUMNS(COPY)
#undef COPY
    return s;
}
#undef AZ_SCENE_COLUMNS
