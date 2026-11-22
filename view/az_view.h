/* az_view.h - software renderer for Asmodensius-Native (hosted C, floats).
 *
 * The view is an observer. It takes a `const AzWorld *` between ticks, reads
 * the entity and cell columns, and writes only its own pixel buffers
 * (ARCHITECTURE rule D7: nothing outside the simulation may change it).
 *
 * The picture is a function of the world state and the camera only. It does
 * not depend on slot order: every fragment carries the uid of its creature
 * and equal depths are resolved by the lower uid.
 *
 * Coordinates: one unit = one cell edge. The world box is
 * [0, gx] x [0, gy] x [0, gz], z is up.
 */
#ifndef AZ_VIEW_H
#define AZ_VIEW_H

#include "az_abi.h"

typedef struct AzView AzView;

#define AZ_VIEW_LEGEND 8    /* clades listed in the legend                     */
#define AZ_GLYPH_W     7    /* clade glyph: 7 columns, mirror symmetric        */
#define AZ_GLYPH_H     9

/* what the last az_view_render counted and drew */
typedef struct AzViewInfo {
    u32 tick;
    u32 n_live;                       /* slots with AZ_F_ALIVE                  */
    u32 n_eggs;                       /* of those, AZ_F_EGG                     */
    u32 n_clades;                     /* clades with at least one living member */
    u32 n_drawn;                      /* bodies that produced a fragment        */
    u32 n_marched;                    /* of those, drawn with the full silhouette */
    u32 top_n;                        /* legend rows in use (<= AZ_VIEW_LEGEND)  */
    u32 top_clade[AZ_VIEW_LEGEND];    /* clade index, most numerous first       */
    u32 top_count[AZ_VIEW_LEGEND];    /* ties: lower clade index first          */
} AzViewInfo;

AzView *az_view_create(u32 w, u32 h);            /* NULL if w or h is 0 or > 16384 */
void    az_view_destroy(AzView *v);

/* Orbit camera. yaw and pitch in radians (yaw 0 looks along -x, pitch > 0
 * looks down from above); distance in cells; target in cells.
 * target == NULL: the centre of the world box. distance <= 0: the smallest
 * distance at which the whole box is inside the frame. A view whose camera
 * was never set uses yaw 0.6, pitch 0.5, NULL, 0. */
void    az_view_camera(AzView *v, float yaw, float pitch, float distance, const float target[3]);

typedef struct {
    float eye[3], right[3], up[3], forward[3], target[3], focal;
} AzCameraPose;

/* Evaluate the software renderer's camera without drawing a picture. */
bool az_view_camera_pose(AzView *v, const AzWorld *world, AzCameraPose *pose);

void       az_view_render(AzView *v, const AzWorld *world);
const u32 *az_view_pixels(const AzView *v);      /* w * h pixels, 0xAARRGGBB, row 0 on top */
u32        az_view_width(const AzView *v);
u32        az_view_height(const AzView *v);
u64        az_view_hash(const AzView *v);        /* digest of the pixel buffer   */
void       az_view_info(const AzView *v, AzViewInfo *out);

/* Clade glyph from the AZ_B_GLYPH_A / AZ_B_GLYPH_B bytes. rows[r] holds row r
 * (0 = top); bit 6 is the left column, bit 0 the right one. The map from the
 * 16 input bits to the bitmap is injective. */
void az_view_glyph(u8 glyph_a, u8 glyph_b, u8 rows[AZ_GLYPH_H]);

/* Quantised form of the software renderer's octahedral map. */
typedef struct {
    u16 elong, spike_amp, gmax;
    u8 base[48 * 48], spike[48 * 48];
} AzShapeTable;
void az_view_shape_table(const u8 body[16], AzShapeTable *out);

/* Independent copy of the columns used by the software renderer. */
AzWorld *az_view_capture(const AzWorld *world);
void az_view_capture_free(AzWorld *scene);

/* 5x7 overlay font, ASCII 32..126. Row 0 is the top; bit 4 is the left
 * column. Other characters return 0. */
u8 az_view_font_row(u32 ch, u32 row);

/* ---- az_png.c : PNG writer with stored deflate blocks, no libraries ---- */
u32  az_png_crc32(u32 crc, const u8 *data, size_t n);    /* start with crc = 0     */
u32  az_png_adler32(u32 adler, const u8 *data, size_t n);/* start with adler = 1   */
/* Encode into a malloc'd buffer (caller frees). 8-bit RGB when every alpha
 * is 0xFF, otherwise 8-bit RGBA. NULL on a bad size or out of memory. */
u8  *az_png_encode(const u32 *pixels, u32 w, u32 h, size_t *out_len);
bool az_png_write(const char *path, const u32 *pixels, u32 w, u32 h);

#endif /* AZ_VIEW_H */
