/* az_worldgen.c (reference stub) - a uniform world with random founders.
 * The module owner builds the real alien world: layered phases, terrain,
 * vents, two suns, and the founder species. */
#include "../az_abi.h"

void az_worldgen(AzWorld *w)
{
    w->sun_period[0] = 2000; w->sun_period[1] = 3100;
    az_weather_global(w);                        /* the suns stand somewhere before the cells are derived */
    for (u32 c = w->n_cells; c-- > 0u;) {        /* from the top layer down: a cell's light comes from the cell above */
        u32 *sp = &w->c_sp[(size_t)c * AZ_NSPECIES];
        sp[AZ_SP_SOLV] = 1000;
        sp[AZ_SP_OXI] = 200;
        sp[AZ_SP_ORG_Y] = 400;
        w->c_heat[c] = 8u * (1600u + 16u);       /* 8 eu per matter unit: temperature 8192, the optimum of TOPT 128 */
        w->c_light[c] = 128;
        w->c_lvec[(size_t)c * 4 + 2] = 127;      /* the light comes from above */
        az_chem_derive(w, c);                    /* the derived columns are defined from tick 0 */
        az_weather_derive(w, c);
    }
    w->gravity = 0;
    az_genetic_code_init(w);

    u32 nsp = w->cfg.n_species ? w->cfg.n_species : 1;
    u32 first_clade = w->n_clades;
    for (u32 k = 0; k < nsp; k++) (void)az_clade_found(w, 0x10000u + k, k);

    for (u32 i = 0; i < w->cfg.n_initial; i++) {
        u32 r[4];
        az_rand4(w, w->uid_next, AZ_RS_WORLDGEN, 0, r);
        u32 px = r[0];
        u32 py = r[1];
        u32 pz = (1u << 16) + (u32)(((u64)r[2] * (0xFFFFFFFFull - (2u << 16))) >> 32);
        const u32 cell = az_rock_place(w, &px, &py, &pz);
        if (cell == AZ_NONE) {
            az_emit(w, 0, AZ_EV_REFUSED, AZ_REF_TERRAIN, w->uid_next, 0, 0);
            break;
        }
        u32 s = az_spawn(w);
        if (s == AZ_NONE) break;
        w->pos_x[s] = px; w->pos_y[s] = py; w->pos_z[s] = pz;
        w->cell[s] = cell;
        u32 species = r[3] % nsp;
        u32 cl = first_clade + species;
        if (cl >= w->n_clades) cl = 0;
        w->clade[s] = cl;
        w->clades[cl].refcount++;
        w->energy[s] = 2000;
        w->pool_a[s] = 200;
        w->mass[s] = 200;
        w->health[s] = 40000;
        w->flags[s] = AZ_F_ALIVE;
        w->lineage[s] = species;
        w->birth_tick[s] = 0;
        if (!az_found(w, s, 0x10000u + species)) {     /* no room in the creature store: no founder */
            w->clades[cl].refcount--;
            w->flags[s] = 0;
            w->energy[s] = 0; w->pool_a[s] = 0; w->mass[s] = 0;
            break;
        }
    }
    az_publish_pressure(w);
}
