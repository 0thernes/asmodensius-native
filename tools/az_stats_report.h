#ifndef AZ_STATS_REPORT_H
#define AZ_STATS_REPORT_H
#include <inttypes.h>
#include <stdio.h>
#include "../core/az_abi.h"

static inline void azt_census_array(FILE *f, const char *name, const u32 *v, u32 n)
{
    fprintf(f, ",\"%s\":[", name);
    for (u32 j = 0; j < n; ++j) fprintf(f, "%s%u", j ? "," : "", v[j]);
    fputc(']', f);
}

/* Used by both hosted reporters after simulation timing is complete. */
static inline void azt_census_fields(FILE *f, const AzStats *s)
{
    static const char *const names[AZ_STATS_DISTRIBUTIONS] = {
        "genome_length", "hidden_neurons", "tiles", "body_parts", "body_matter",
        "memory_words", "colony_size", "lineage_depth"
    };
    fprintf(f, "{\"live\":%u,\"colonies\":%u,\"species\":%u,\"looks\":%u,"
            "\"species_beyond\":%u,\"looks_beyond\":%u,\"diet_unknown\":%u,"
            "\"mean_age_q8\":\"%" PRIu64 "\",\"age_q8_saturated\":%u,\"mean_mass_q8\":%" PRIu64
            ",\"ranks\":[0,5,25,50,75,95,100],\"quantiles\":{",
            s->n_live,s->n_colonies,s->n_species_live,s->n_looks,s->n_species_beyond,s->n_looks_beyond,
            s->n_diet_unknown,s->mean_age_q8,s->age_q8_saturated,s->mean_mass_q8);
    for (u32 dim = 0; dim < AZ_STATS_DISTRIBUTIONS; ++dim) {
        fprintf(f,"%s\"%s\":[",dim ? "," : "",names[dim]);
        for (u32 j = 0; j < AZ_STATS_QUANTILES; ++j)
            fprintf(f,"%s\"%" PRIu64 "\"",j ? "," : "",s->quantile[dim][j]);
        fputc(']', f);
    }
    fputc('}', f);
    azt_census_array(f,"trait_mean",s->trait_mean,AZ_TRAITS);
    azt_census_array(f,"trait_var",s->trait_var,AZ_TRAITS);
    azt_census_array(f,"archetype",s->archetype,32);
    azt_census_array(f,"look",s->look,64);
    azt_census_array(f,"diet",s->diet,4);
}

static inline void azt_census_json(FILE *f, const AzStats *s)
{
    azt_census_fields(f, s);
    fputc('}', f);
}
#endif
