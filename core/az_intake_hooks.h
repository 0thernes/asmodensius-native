#ifndef AZ_INTAKE_HOOKS_H
#define AZ_INTAKE_HOOKS_H
#include "az_abi.h"

/* Only the hosted reporting targets link the observer. Reorder executes after
 * grid_build and before the simulation permutes columns. Workers settle once
 * per positive slot and never touch another worker's observation row. */
/* Positive matter credited to this initiator's free pool by a resolved attack.
 * The same worker owns the slot's metabolic intake and contest settlement. */
void azt_predation_settled(const AzWorld *w, u32 slot, u32 credited_mu);
/* Observe the positive physical loss from a pooled waste stock immediately before
 * conversion, secretion or excretion; total includes all earlier internal gains. */
void azt_waste_removed(const AzWorld *w, u32 slot, u32 total, u32 amount, bool converted);
void azt_intake_reorder(const AzWorld *w);
void azt_intake_settled(const AzWorld *w, u32 slot, u32 light, u32 nut_x,
                        u32 org_y, u32 wst_fixed, u32 wst_carried);
#ifdef AZ_COMMONS_OBSERVER
void azt_commons_pulse(const AzWorld *w, u32 slot, const AzColony *colony,
                       bool received, bool contributed, bool eligible);
#endif
#endif
