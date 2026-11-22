/* az_weather.c (reference stub) - no weather, but a temperature.
 *
 * The cell column `temp` is this module's (az_cells.def): az_temp_of(heat,
 * all matter in the cell). The stub writes it, as the ABI says, and holds
 * every stepped cell at the 8 eu of heat per matter unit the stub world is
 * made with (temperature 8192, the optimum of TOPT 128): a stepped cell
 * gives up or takes in one eighth of the difference, booked as energy_out or
 * energy_in. Without this the heat of everything that lives would pile up in
 * a world that has no sky to radiate into. The real module replaces it with
 * suns, radiation, wind, storms and the gradients they make. */
#include "../az_abi.h"

static u64 cell_matter(const AzWorld *w, u32 c)
{
    const u32 *sp = &w->c_sp[(size_t)c * AZ_NSPECIES];
    u64 m = 0;
    for (u32 k = 0; k < AZ_NSPECIES; k++) m += sp[k];
    return m;
}

void az_weather_cell(AzWorld *w, u32 c, u32 worker)
{
    const u64 base = 8u * (cell_matter(w, c) + 16u);
    const u64 heat = w->c_heat[c];
    if (heat > base) {
        const u32 out = (u32)((heat - base + 7u) >> 3);
        az_cell_post_heat(w, worker, c, -(i32)out);
        az_tally(w, worker)->energy_out += out;
    } else if (heat < base) {
        const u64 d = (base - heat + 7u) >> 3;
        const u32 in = d > 0x3FFFFFFFu ? 0x3FFFFFFFu : (u32)d;
        az_cell_post_heat(w, worker, c, (i32)in);
        az_tally(w, worker)->energy_in += in;
    }
    az_weather_derive(w, c);
}

void az_weather_derive(AzWorld *w, u32 c)
{
    w->c_temp[c] = (i16)az_temp_of(w->c_heat[c], cell_matter(w, c));
}

void az_weather_global(AzWorld *w)
{
    (void)w;
}
