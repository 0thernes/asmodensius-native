/* az_stats.c (reference stub) - head counts only. */
#include "../az_abi.h"

void az_stats_collect(const AzWorld *w, AzStats *out)
{
    az_memset(out, 0, sizeof(*out));
    u64 esum = 0;
    for (u32 s = 0; s < w->n_slots; s++) {
        u8 fl = w->flags[s];
        if (!(fl & AZ_F_ALIVE)) continue;
        out->n_live++;
        if (fl & AZ_F_EGG) out->n_eggs++;
        esum += (u64)(w->energy[s] > 0 ? w->energy[s] : 0);
        if (w->generation[s] > out->max_generation) out->max_generation = w->generation[s];
        out->archetype[w->archetype[s] & 31]++;
        out->mating_mode[w->phen[(size_t)s * AZ_PHEN + AZ_P_MATE] & 7]++;
        for (u32 k = 0; k < AZ_TRAITS; k++) out->trait_mean[k] += w->trait[(size_t)s * AZ_TRAITS + k];
    }
    for (u32 i = 0; i < w->n_clades; i++) if (w->clades[i].alive && w->clades[i].refcount) out->n_clades++;
    for (u32 i = 1; i < w->n_colonies; i++) if (w->colonies[i].alive) out->n_colonies++;
    if (out->n_live) {
        out->mean_energy_q8 = (esum << 8) / out->n_live;
        for (u32 k = 0; k < AZ_TRAITS; k++) out->trait_mean[k] /= out->n_live;
    }
}

u32 az_classify(const AzWorld *w, u32 slot)
{
    (void)w; (void)slot;
    return AZ_ARCH_NONE;
}

u32 az_classify_observed(const AzWorld *w, u32 s, const AzClassObservations *x)
{
    (void)x;
    return az_classify(w, s);
}

u32 az_look_class(const AzWorld *w, u32 slot)
{
    (void)w; (void)slot;
    return 0;
}
