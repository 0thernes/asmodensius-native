/* Placement at creation and restore boundaries. Only solid 255 is bedrock. */
#include "az_abi.h"

static u32 centre(u32 cell, u32 shift)
{
    return (cell << shift) | (1u << (shift - 1u));
}

static u32 distance(u32 a, u32 b) { return a >= b ? a - b : b - a; }

static u32 wrapped_distance(u32 a, u32 b, u32 width)
{
    u32 d = distance(a, b);
    return d <= width - d ? d : width - d;
}

u32 az_rock_place(const AzWorld *w, u32 *px, u32 *py, u32 *pz)
{
    u32 cell = az_cell_at(w, *px, *py, *pz);
    if (w->c_solid[cell] != 255u) return cell;
    const u32 x = az_cell_x(w, cell), y = az_cell_y(w, cell), z = az_cell_z(w, cell);
    for (u32 d = 1; d < w->gz; d++) {
        if (z + d < w->gz) {
            cell = az_cell_xyz(w, x, y, z + d);
            if (w->c_solid[cell] != 255u) { *pz = centre(z + d, w->shift_z); return cell; }
        }
        if (d <= z) {
            cell = az_cell_xyz(w, x, y, z - d);
            if (w->c_solid[cell] != 255u) { *pz = centre(z - d, w->shift_z); return cell; }
        }
    }
    /* Cell order breaks exact ties in the horizontal and vertical distances. */
    u32 best = AZ_NONE, best_xy = AZ_NONE, best_z = AZ_NONE;
    for (u32 c = 0; c < w->n_cells; c++) {
        if (w->c_solid[c] == 255u) continue;
        const u32 xy = wrapped_distance(x, az_cell_x(w, c), w->gx)
                     + wrapped_distance(y, az_cell_y(w, c), w->gy);
        const u32 dz = distance(z, az_cell_z(w, c));
        if (xy < best_xy || (xy == best_xy && dz < best_z)) {
            best = c; best_xy = xy; best_z = dz;
        }
    }
    if (best != AZ_NONE) {
        *px = centre(az_cell_x(w, best), w->shift_x);
        *py = centre(az_cell_y(w, best), w->shift_y);
        *pz = centre(az_cell_z(w, best), w->shift_z);
    }
    return best;
}

/* A blocked dispersal proposal first falls back to the mother's actual position. */
u32 az_rock_birth_place(const AzWorld *w, u32 mother, u32 *px, u32 *py, u32 *pz)
{
    const u32 cell = az_cell_at(w, *px, *py, *pz);
    if (w->c_solid[cell] != 255u) return cell;
    *px = w->pos_x[mother]; *py = w->pos_y[mother]; *pz = w->pos_z[mother];
    return az_rock_place(w, px, py, pz);
}
