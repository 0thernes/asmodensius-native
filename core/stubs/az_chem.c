/* az_chem.c (reference stub) - no chemistry but the decay of the radiotoxin.
 *
 * The shared redox cycle says: toxin decay, 1 TOX_Z -> 1 WST_W + 2 eu heat.
 * az_metab.c lets creatures make and release the toxin, and the stub world
 * has nothing else that removes it, so the stub does this one reaction: a
 * stepped cell loses one sixteenth of its toxin, rounded up. Phases, the
 * medium, slow oxidation and every flux between cells wait for the real
 * module. */
#include "../az_abi.h"

void az_chem_cell(AzWorld *w, u32 c, u32 worker)
{
    const u32 tox = w->c_sp[(size_t)c * AZ_NSPECIES + AZ_SP_TOX_Z];
    if (tox) {
        const u32 d = tox / 16u + (tox % 16u != 0);
        az_cell_post(w, worker, c, AZ_SP_TOX_Z, -(i32)d);
        az_cell_post(w, worker, c, AZ_SP_WST_W, (i32)d);
        az_cell_post_heat(w, worker, c, (i32)(d * (u32)(az_eps_species[AZ_SP_TOX_Z] - az_eps_species[AZ_SP_WST_W])));
    }
}

void az_chem_derive(AzWorld *w, u32 c)
{
    (void)w; (void)c;
}
