If your progress file `Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native\build\work\w2\life\PROGRESS.md` exists, read it first and continue from its next step; never redo a step it records as done and tested.

# Brief: az_life.c (stages, broods, births, deaths)

**Amended 2026-11-18 (ABI-V4-NOTICE section 5, README rules 13 to 20; they win over the text below).** Brood size has no cap (`AZ_CLUTCH_MAX` is gone): matter, energy, slots and store room limit a brood. A birth can be refused for want of store room (`az_conceive` returns false): refund the provision as the stub does. A body is parts (`az_parts`): `kill` sends every part's matter to the cell. You own `AZ_T_DEV_PLAST` (how far the birth place bends development). Stage lengths and hazards are traits or parameters in `core/params/az_life.def`.

You own `core/az_life.c` and `tests/test_life.c`. Read `README.md` in this
folder first, and the "Life stages" comment in az_abi.h. You are the only
writer of the flags ALIVE, EGG, JUV, SENESC, DORMANT and RECEPT and the only
caller of `az_birth_request`. Start from the stub: its kill, its radix sort
and its commit loop keep the books exact; keep that structure.

## az_life_range (phase 3, last hook of the block)

Per alive slot, in this order:
1. A requested death (`AZ_RQ_DIE`) is carried out: `az_life_kill`.
2. `AZ_RQ_HATCH`: an egg or a cyst wakes (EGG or DORMANT cleared).
3. Budding, HOT only: a ripe adult (RECEPT) whose brain drives `AZ_M_REPRO`
   and whose mating mode (`AZ_P_MATE` low bits) is BUD calls
   `az_life_brood(w, s, AZ_NONE, worker)`. (Selfing is in the phased check,
   because its rate is a trait.)
4. The phased check, behind `az_phase_due(w, uid, AZ_RS_LIFE, 4)` (every 16
   ticks; trait row allowed). Age = `tick - birth_tick`, plus for a founder
   (`parent_a == 0`) an offset drawn from its uid, so that founders are not
   one cohort:
   - egg: hatches after the time set by `AZ_T_EGG_TIME` (emit HATCH);
   - juvenile: becomes adult after the time set by `AZ_T_MATURITY` if its
     body has reached the size set by `AZ_T_SIZE`;
   - adult: RECEPT is set when energy is above the level of
     `AZ_T_REPRO_THRESH` and pool_a above the level of `AZ_T_SIZE`, cleared
     otherwise;
   - senescence starts at the age set by `AZ_T_LIFESPAN` (emit STAGE); from
     then on a hazard that doubles at a fixed interval kills by a draw
     (`AZ_RS_LIFE`), cause AGE; a small age-independent hazard kills at any
     stage, cause HAZARD;
   - dormancy: a starving creature whose brain drives `AZ_M_REST` encysts
     with a readiness set by `AZ_T_DORMANCY`; a cyst wakes when its cell
     offers food again or after a draw;
   - a ripe hermaphrodite whose brain drives `AZ_M_REPRO` selfs
     (`az_life_brood` with itself as father) with a probability set by
     `AZ_T_SELFING`: high odds in mode SELF, the fallback of mode HERMA.

## az_life_brood(w, mother, father, worker)

Refuse (return 0) unless the mother is a ripe adult with no standing
request. Clutch size from `AZ_T_CLUTCH` (1 to `AZ_CLUTCH_MAX`), provision
from `AZ_T_PROVISION` as a share of her stores: pool_t first, then pool_a,
and energy; more children means less each. Call `az_birth_request`, deduct
exactly what was given, clear RECEPT, bump `AZ_ST_OFFSPRING`. Return the
clutch size.

## az_life_commit (serial)

As the stub (mothers in uid order, equal shares, refunds to the cell), plus:
the child's own `AZ_T_EGG_TIME` decides egg or live birth (BIRTH event
`aux`); placement at a distance set by the mother's `AZ_T_DISPERSAL`, never
in bedrock; the child joins the mother's colony; a rare macro-mutation
founds a new clade (`az_clade_mutate`, draw on `AZ_RS_BIRTH`, events CLADE
or SPECIATE); health and flags of the newborn.

## az_life_kill

As the stub: everything the body holds goes back to its cell (biomass as
ORG_Y, waste, toxin, inventory by species, energy as heat), the clade loses
a member, one DEATH event with the cause from `pair.req`.

## Contracts others rely on

- `az_mate_resolve` calls `az_life_brood` for the bearing side and trusts
  the return value; nobody else requests births.
- RECEPT is what `AZ_PF_RECEPT` shows the neighbours from the next tick on.
- A slot that dies is not reused before the next grid build.

## Traits you own

SIZE, LIFESPAN, DORMANCY, REPRO_THRESH, CLUTCH, PROVISION, EGG_TIME,
MATURITY, DISPERSAL, SELFING (all from the row, on the paths above).

## Tests (tests/test_life.c)

A creature walks through egg, juvenile, adult, senescent and dies of age at
the times its traits give. Brood books are exact for every clutch size;
refunds when no slot is free. Budding, selfing and (with a hand-made pair)
sexual broods give children with the right parents and flags. Mortality by
age follows the doubling hazard within sampling error on seeds 1 to 24.
Dormancy is entered and left. Both invariants every tick; equal hashes for
1, 3 and 8 workers, reorder 1 and 9, also when the capacity binds.

References: Gompertz mortality (1825); the offspring size and number
trade-off (Smith and Fretwell 1974).
