If your progress file `Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native\build\work\w2\metab\PROGRESS.md` exists, read it first and continue from its next step; never redo a step it records as done and tested.

# Brief: az_metab.c (feeding, respiration, growth, stress, starvation)

**Amended 2026-11-18 (ABI-V4-NOTICE section 5, README rules 13 to 20; they win over the text below).** Bodies are parts (5.2): intake fills the parts (`AZ_T_ALLOMETRY` shares it), a full body asks for a new part with `az_part_request` when the brain drives `AZ_M_GROW`, an empty last part is given up with `az_part_shed_request` (`AZ_M_SHED`). What a part holds is `PART_HOLD`; the caps `AZ_POOL_MAX`, `AZ_INV_MAX`, `AZ_ENERGY_MAX` are gone. Upkeep should grow with what the creature carries: parts, neurons (`az_brain(w, s, &bytes)`), memory words (`az_memory`), so that size is a trade. Rates are traits or parameters in `core/params/az_metab.def`.

You own `core/az_metab.c` and `tests/test_metab.c`. Read `README.md` in this
folder first (the redox cycle under "Shared numbers" is yours to implement).
You write `energy`, `pool_a`, `pool_m`, `pool_t`, `waste`, `toxin`, `health`,
`mass` and the EPH column `eat`.

## Every tick, every active entity (HOT state and the cell only)

`az_metab_demand` (phase 1): demand on the uptake channels from the hot
phenotype and the brain's `AZ_M_EAT` drive: LIGHT by `AZ_P_DIET_L`, NUT_X by
`AZ_P_DIET_X`, ORG_Y by `AZ_P_DIET_Y`, WST_W by `AZ_P_DIET_W`, scaled by
`AZ_P_METAB` and by body size (`mass`). Write `eat[]`, call
`az_cell_demand`. Eggs and cysts demand nothing.

`az_metab_range` (phase 3), per slot:
- take what the cell granted (`az_cell_granted`), capped by pool room, and
  book it: heterotrophy and chemotrophy as in the cycle; light is energy
  only (`az_tally(w, worker)->energy_in`), WST_W is matter that light or the
  energy store upgrades (photosynthesis releases OXI to the cell);
- basal cost by `AZ_P_METAB` and `mass`, plus a share for sense range
  (`AZ_P_SENSE`), paid as heat; what cannot be paid becomes damage;
- thermal stress: `c_temp` against `AZ_TEMP_OF_TOPT(phen TOPT)` and
  `AZ_TEMP_TOL(phen TTOL)`: outside the band, extra cost and damage;
- toxin: ambient TOX_Z enters the body with the food; the load damages
  health, less with `AZ_P_TOXRES`. With `AZ_LESION_TOXIN`: no uptake, no
  damage, no secretion;
- `mass` = pools + waste + toxin, clamped; the energy cap (`AZ_ENERGY_MAX`);
- death: `az_request_death` with STARVED (no energy and no pool), WOUNDS
  (health 0), POISON, COLD_HEAT.

## The pulse (behind az_phase_due(w, uid, AZ_RS_METAB, 2); trait row allowed)

Four ticks of slow physiology at once, sized accordingly:
- respiration: burn pool_a for energy; with OXI demanded and granted the
  aerobic path (by `AZ_T_OXI_USE`; the demand is posted in phase 1 on pulse
  ticks), otherwise fermentation, which excretes ORG_Y; the share of the
  released energy the body keeps is set by `AZ_T_EFFIC`, the rest is heat;
- storage and growth: energy above the store set by `AZ_T_STORE_CAP` builds
  pool_m (body wall) and pool_t (germ store), split by `AZ_T_GROWTH`;
  starvation draws them back;
- repair: health is restored from pool_m;
- excretion: `waste` goes to the cell when it passes the level set by
  `AZ_T_WASTE_RATE`; carried waste harms a little;
- secretion: with `AZ_T_TOX_SECRETE`, part of the waste is turned into TOX_Z
  and released (2 eu per unit from the energy store), a chemical defence;
- pressure: `az_cell_pressure` away from the comfort set by
  `AZ_T_PRESS_TOL` damages (death cause PRESSURE);
- radiation: `c_charge` above the level set by `AZ_T_RAD_TOL` damages.

## Contracts others rely on

- You run after the pair resolves, the social range and the econ range, and
  before physics and life: `mass` and `health` are final for the tick when
  you return, and life applies the death you requested.
- `pool_t` is the germ store: `az_life_brood` provisions a brood from it
  first. `pool_m` is the wall: health heals from it.
- Uptake is the only way matter enters a body from the field; excretion,
  secretion and death are the only ways out (plus gifts and stakes).

## Traits you own (az_owners.def)

Through phen: METAB, TOPT, TTOL, TOXRES, DIET_LIGHT, DIET_X, DIET_Y, DIET_W.
From the row, in the pulse: EFFIC, PRESS_TOL, RAD_TOL, OXI_USE, STORE_CAP,
TOX_SECRETE, WASTE_RATE, GROWTH. Lesion: `AZ_LESION_TOXIN` (your part).

## Tests (tests/test_metab.c)

Each conversion alone on one creature in one cell, exact numbers. A producer
in light gains mass and releases OXI; in the dark it starves. A chemotroph
lives on NUT_X; a grazer on ORG_Y. Aerobic beats fermentation in energy per
unit burned. A creature outside its thermal band loses health; inside it
does not. Toxin harms, less with TOXRES, not at all under the lesion. Both
invariants every tick; equal hashes for 1, 3 and 8 workers.

References: Kleiber's law for the size scaling of basal cost (1932);
dynamic energy budget models for reserve, structure and reproduction
buffers (Kooijman 2010).
