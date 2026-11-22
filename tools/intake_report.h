#ifndef AZ_INTAKE_REPORT_H
#define AZ_INTAKE_REPORT_H
#include "az_abi.h"
#include <stdio.h>

/* A world-scoped hosted measurement. The tail is zeroed, 64-byte aligned
 * and locked for at least the lifetime of the selected world. */
u64 azt_intake_bytes(u32 capacity);
bool azt_intake_attach(const AzWorld *w, bool from_worldgen, void *tail, u64 bytes);
void azt_intake_detach(void);
#ifdef AZ_BUILDER_OBSERVER
bool azt_intake_is_attached(const AzWorld *w);
bool azt_intake_record_builder(const AzWorld *w, u32 slot);
u32 azt_intake_builder_events(const AzWorld *w, u32 slot);
#endif
/* True when all four lifetime occasion counts are exact. Use azt_intake_fill
 * to preserve upper and lower bounds when a complete history mixes sources. */
bool azt_intake_class(const AzWorld *w, u32 slot, u64 occasions[4]);
void azt_intake_json(FILE *out, const AzWorld *w);
void azt_intake_fill(const AzWorld *w, u32 slot, AzClassObservations *observation);
void azt_intake_apply(const AzWorld *w, AzStats *st);
#ifdef AZ_COMMONS_OBSERVER
void azt_commons_json(FILE *out, const AzWorld *w);
#endif
#endif
