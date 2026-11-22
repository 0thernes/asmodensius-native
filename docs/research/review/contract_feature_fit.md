# Contract review: feature fit (key: feature-fit)

Question asked: can every planned feature be built inside ABI version 1
(`core/az_abi.h`, `az_columns.def`, `az_cells.def`, `az_world.c`, `az_tick.c`)
without new columns or hooks? Method: walk each feature as its implementer,
note the exact line where it gets stuck, and back each gap with an experiment
or with two quoted lines.

Short answer: no. Twelve of the nineteen features fit as they stand or with a
rule change or a small EPH/WARM addition. Seven (1, 2, 3, 5, 6, 7, 14) are
blocked or partly blocked. Findings 1 to 3 would force a rewrite of the scan,
the percept consumers or the brain kernels if they are fixed after the module
owners have built on them. **None of the proposed
fixes adds a byte to the HOT tier; it stays exactly 128.**

## What was run (2026-11-17, all timings noisy, parallel load)

Scratch copy: `S:/review/feature-fit/src` (copy of `N:/core`, `platform`,
`tools`, `tests`, `CMakeLists.txt`) plus one new file
`S:/review/feature-fit/src/tests/test_fit.c`. Nothing under `N:/core`,
`N:/platform`, `N:/tools` or `N:/tests` was edited.

```
cmake -S S:/review/feature-fit/src -B S:/review/feature-fit/src/b -G Ninja
cmake --build S:/review/feature-fit/src/b
S:/review/feature-fit/src/b/test_fit.exe
```

Output (complete):

```
A: sizeof(AzPercept)=100  n_other small=1 big=1  n_touch small=0 big=0
A: neighbour radius code in pub: small=13 big=26 (my code=15)
A: percept small-vs-big neighbour   identical=1   sens[32] identical=1
A: percept stranger-vs-bond-partner identical=1   sens identical=1
A: percept stranger-vs-own-child    identical=1   sens identical=1
A: percept stranger-vs-own-parent   identical=1   sens identical=1
B: snapshot bytes=2177616  slots=4212 live=4206 at save (dead slots present=1)
B: hash at save 8e945a894ff8aa92  after load 8e945a894ff8aa92  equal=1
B: hash after load with gravity+999 and sun_period+1: 8e945a894ff8aa92  equal to saved=1
B: +60 ticks original 686ae9f56b8fc270  restored 686ae9f56b8fc270  equal=1  matter ok=1 energy ok=1
C: newborn uid 3001 slot 3000: cell=ffffffff spos=0 -> order[spos]=slot 0 (uid 2781, not me)
C: newborn uid 3002 slot 3001: cell=ffffffff spos=0 -> order[spos]=slot 0 (uid 2781, not me)
C: newborns over 60 ticks=1322  with cell==AZ_NONE=1322  with spos==0=1322
D: after 60 ticks: n_live=4322 uid_next=4323  -> uids per tick=22.03, per live entity per tick=0.00510
D: interactions counted with the reference stubs: 0
ok: 9 checks, 0 failures
```

What each experiment is:

- **A** Two entities in one cell, 0.8 cell apart, not kin, no colony. The
  neighbour is varied: mass 100 or 9000; stranger, my bond partner
  (`partner[0] = uid[1]`), my child (`parent_a[1] = uid[0]`), my parent. Then
  `az_grid_build`, `az_scan_cell`, `az_sense` (noise channel zeroed) and a
  `memcmp` of the whole `AzPercept` and of the 32 channels. The scan and sense
  are the reference stubs, so the sensory result is a statement about the
  stubs; the percept result is a statement about the struct, which has no
  field that could differ.
- **B** A snapshot written only from the column registry (`az_columns`,
  `az_cell_columns`, non-EPH tiers), the clade and colony tables, and 9 world
  scalars; loaded into a world made by `az_world_create` without worldgen.
- **C** State of entities born in a tick, as `az_tier1_range` sees them in
  stage 6 of that tick.
- **D** uid consumption of the stub life cycle.

Also run: `N:/build/release/test_core.exe` -> `ok: 203 checks, 0 failures`.

Grep evidence used below:

```
grep -n "az_social_intent\|az_econ_\|az_mate_" core/az_abi.h core/az_tick.c
  az_abi.h:685:void az_social_intent(AzWorld *w, u32 slot, const AzPercept *p);
  az_abi.h:688:i32  az_mate_score(...)      az_abi.h:689:i32  az_econ_score(...)
  az_abi.h:697:void az_mate_resolve(...)    az_abi.h:698:void az_econ_resolve(...)
  az_tick.c:130:                az_social_intent(w, slot, &pc[k]);
grep -n -i "colony\|colonies" core/az_tick.c
  258:    if (!(w->cfg.lesion & AZ_LESION_COLONY) && (w->tick % w->cfg.tier2_period) == 0)
  259:        az_colony_tier2(w);
grep -c "gravity\|sun_period\|vent_" core/az_world.c        -> 0
grep -rn "AZ_ARCH" core tools tests platform
  core/az_columns.def:74:AZ_COL(archetype,u8,  1, WARM)  /* measured behavioural class AZ_ARCH_* */
grep for a uid -> slot lookup (uid_to_slot|slot_of|az_find) in N:/  -> no match in code
```

## Walk-through of the nineteen features

| # | Feature | Verdict | Where it gets stuck |
|---|---|---|---|
| 1 | predator versus prey | **blocked** | `AzPercept` has one undivided `other[3]`; no size class; all 32 channels are assigned. Finding 3. |
| 2 | pair bond, find partner by uid | **blocked** | `AzPub` has no uid; the percept offers 3 slots chosen by distance. Finding 1. |
| 3 | parental care | **blocked** | same; kin by tag cannot tell my child from a cousin. Finding 1. |
| 4 | eggs and cysts as prey, wind dispersal | fits | every hook is called for passive entities; `c_wind` is readable in phase 3; `AZ_PF_PASSIVE` marks them. Waking a cyst is a life-module rule (it has no brain step). Run length: finding 10. |
| 5 | colonies, swarms, fleets | **partly blocked** | deposits, marks, roles, recruitment, founding and fission fit. Withdrawals from `store[]` have no deterministic path (finding 4). `vel[3]` sums colony-mates only, so kin swarms without a colony get no alignment input (finding 3). |
| 6 | trade, reservation prices, recorded prices | **blocked for the owner** | the offer fields are sufficient, but `az_econ.c` has no phase-1 entry point (finding 2). Prices can be recorded through `AzEvent`, which may drop; an exact record needs a totals field (finding 2). |
| 7 | eight mating modes, responder choice | **partly blocked** | choice by the responder works through `az_mate_score` and max-claim. `az_mate.c` has no phase-1 entry point (finding 2). Monogamy, fidelity and guarding need finding 1. Selfing versus budding: both have `father_slot == AZ_NONE`; say that selfing is `father_slot == mother_slot`. |
| 8 | share, groom, teach, steal, attack from offers alone | fits with two rule changes | steal is tied to the trade lot; a receiver's cap is not in any offer (finding 5). |
| 9 | reputation, indirect reciprocity | fits | an observer sees one bit, `AZ_PF_COOP`, through `pub[spos[candidate]]`; own memory is 4 tags. That is enough for image scoring; second-order standing is not expressible. Design limit, not a defect. |
| 10 | four diets, oxidiser, toxin, radiotoxin | fits except uptake of toxin | `AZ_D_*` has no `TOX_Z` channel, so rule C1 gives no way to take toxin from a cell (finding 9). Mutation rate from radiotoxin fits: the genome functions run in the serial commit and can read the mother's `toxin` and the cell. |
| 11 | weather on bodies, gravity, buoyancy | fits except pressure | wind, temp, charge, phase, `sp[]`, `solid`, `w->gravity` are readable in phase 3. There is no pressure column and no shared definition, so `AZ_T_PRESS_TOL` has no stimulus (finding 9). |
| 12 | lifetime learning through `mod_gain` / `mod_bias` | fits | `elig`, `rbase`, `rprev`, `hid`, this tick's `motor[AZ_M_GATE]` are all available in stage 6. Traces are sampled every K1 ticks, not per tick. |
| 13 | Tier 1: state vector, SH descriptor, associative memory | fits with a rule and a small column | Tier 1 can call `az_scan_cell` itself or walk `pub[]`; the contract does not say it may, and newborns alias sorted position 0. The 64 spins have no place for stored patterns. Finding 8. |
| 14 | archetypes from 12 counters | **partly blocked** | `AZ_ARCH_*` is not defined anywhere; no counter separates the four diets (finding 7). |
| 15 | renderer | fits | `pos`, `rot`, `mass` -> `az_radius_code`, `AzClade.body[16]`, the 8 appearance traits (contiguous at the end of `trait`), `flags`, `role`, `archetype`. The host owns the tick loop, so it reads between ticks. |
| 16 | snapshot and exact restore | fits; no API yet | experiment B restores exactly from the registry. Three gaps: no entry point, world constants outside the hash, no home for world-level module state (finding 6). |
| 17 | founder species 1..100 | fits | `n_species` is u16; species = root of the clade parent chain (`AzClade.parent`), founder id in `lineage`. Niche axes exist: 4 diets, oxidiser, `TOPT`, density (medium), size, predation, mating mode. |
| 18 | senescence, stages, maturation | fits | `flags` has egg, juvenile, senescent, dormant; age is `tick - birth_tick`; traits `LIFESPAN`, `MATURITY`, `EGG_TIME`, `GROWTH`. Note: all 8 flag bits are used; `AZ_F_INIT` is per-tick and could move to an EPH byte if a ninth state is ever needed. |
| 19 | trait axes each with a causal reader | 96 defined, not 100 | I found no requirement text that says 100; the contract says 96 (`AZ_TRAITS`). Traits whose reader is blocked by a finding: `CARE`, `BOND`, `FIDELITY`, `GUARD` (1); `PRESS_TOL` (9); `STORE_SHARE` (4); `HOPFIELD` (8). The genome has 256 two-bit bases per haplotype, enough for 96 independent polygenic traits. |

## Findings (ranked)

### 1. CRITICAL - no way to find a specific individual: bond partner, own child, own parent

Evidence. The identities are stored as uids (`az_columns.def`:
`AZ_COL(partner, u32, 1, WARM) /* pair-bond partner uid, 0 = none */`,
`parent_a`, `parent_b`) and rule D6 says "Across ticks refer to an entity by
uid". But (a) the code has no uid -> slot lookup; (b) rule D2 says "Phase 1:
read your own entity freely; read others only through `pub[]`" and `AzPub`
(x, y, z, vx, vy, vz, rf, tag, colony, sig_a, sig_b) holds no uid; (c)
`az_social_intent` must take its target from the percept, which carries three
slots chosen by distance (`near_other`, `near_kin`, `best_mate`). Experiment A:
the percept is byte-identical whether the neighbour is a stranger, my partner,
my child or my parent.

Consequence. A bonded entity can only act on its partner when the partner
happens to be the nearest receptive neighbour. A parent can only feed its
child when the child is the nearest kin; in a colony with tens of colony-mates
in range that is rare. Monogamy, guarding, fidelity, parental care and nurse
behaviour toward own brood cannot be written. Fixing it later means changing
the scan, its assembly and GPU twins, and every percept consumer.

Change.

```c
/* second sorted snapshot, built by az_grid_build next to pub[] (not a column) */
typedef struct AzPubId { u32 uid, parent_a, parent_b, partner; } AzPubId;   /* 16 bytes */
/* AzWorld: */ AzPubId *pub_id;      /* sorted position -> identity record */

/* appended to AzPercept (a stack struct; costs no stored bytes) */
u32 bond;       /* neighbour whose uid == my partner, else AZ_NONE          */
u32 parent;     /* neighbour whose uid == my parent_a or parent_b           */
u32 child;      /* nearest neighbour whose parent_a or parent_b == my uid   */
u32 d2_bond, d2_parent, d2_child;
```

Rule text for D2: "Phase 1 and phase 3 may read `pub_id[spos[x]]` of any
entity (start-of-tick values)." That also lets both sides of a mating agree on
"we are each other's partner" without widening the offer. Cost: 16 bytes per
live entity outside the tiers, and three WARM columns read once per entity in
the serial grid build. If that read is too much, the minimum is
`{ u32 uid, parent_a; }` (8 bytes): partner and maternal care work, paternal
care goes through the bond.

### 2. MAJOR - trade and mating have no phase-1 entry point; one module writes every offer byte and every intent

Evidence. ARCHITECTURE section 7: "Owners: share, groom, teach, recruit,
attack, steal -> `az_social.c`; trade -> `az_econ.c`; mate -> `az_mate.c`."
Against that, `az_abi.h` gives econ and mate only `_score` and `_resolve`
(grep above), and `az_tick.c:130` calls only `az_social_intent`, the single
writer of `act`, `tgt` and all 16 offer bytes including `AZ_O_PRICE`,
`AZ_O_GIVE_QTY`, `AZ_O_ORNAMENT`. Built against the stubs as section 2 tells
owners to do, the stub sets `w->act[slot] = AZ_ACT_NONE` for every slot
(`stubs/az_social.c:12`), so `job_claims` never reaches `az_econ_score` or
`az_mate_score`; the econ and mate owners cannot run their own code without
editing `az_social.c`, which section 12 forbids.

Change.

```c
/* phase 1, called in this order after az_social_intent, for every alive slot */
void az_econ_intent(AzWorld *w, u32 slot, const AzPercept *p);  /* owns offer bytes 6..11 */
void az_mate_intent(AzWorld *w, u32 slot, const AzPercept *p);  /* owns AZ_O_ORNAMENT and the
                                         AZ_OD_RECEPTIVE | AZ_OD_SEX | AZ_OD_BONDED | AZ_OD_BREEDER bits */
AZ_COL(urge, u8, 1, EPH)   /* strength of the proposal now held in act/tgt */
```

Rule: "A module may replace `act`/`tgt` only if its urge is strictly greater
than `urge[slot]`, and then stores its own. Social writes offer bytes 0, 1, 2,
4, 12..15 and the remaining DISP bits. `AZ_LESION_TRADE` and
`AZ_LESION_MATING` suppress the act, never the offer bytes."

For recorded prices add to `AzTotals`:
`u64 trade_units[AZ_NINV][AZ_NINV]; /* units of i given for j, atomic adds */`
The price of i in j is then an exact ratio that no dropped event can distort.

Note for the lead, not a separate finding: there is no motor channel for
trading, and six affiliative and agonistic acts share `AZ_M_AFFIL` and
`AZ_M_AGON`. Which act is proposed will therefore be a trait-and-state rule,
not a brain output. If that is intended, say so in section 7.

### 3. MAJOR - the brain cannot tell a larger threat from smaller prey, and has no input for a mate, partner, child, toxin or pressure

Evidence. Experiment A: neighbour radius code 13 versus 26 (mine 15) gives an
identical percept and identical 32 channels. The size is in `AzPub.rf` but
`AzPercept` has only `n_other`, `other[3]`, `near_other`. The sensory enum is
full: `_Static_assert(AZ_S_AROUSAL == AZ_SENS - 1, ...)` with 32 names, none of
which is a threat, a mate direction, a partner, a toxin or a pressure. Law 4
forbids replacing the missing input with hardcoded steering.

Change, percept (append):

```c
u16 n_big;      /* non-kin, non-colony neighbours with radius code > mine      */
i32 big[3];     /* sum of offsets to those; other[3] keeps the sum over all    */
i32 mate[3];    /* offset to best_mate, 0 if none                              */
i32 kvel[3];    /* sum of pub velocity of kin that are not colony-mates        */
```

Change, channels: `AZ_SENS` 32 -> 48, adding
`AZ_S_BIG_PX, _NX, _PY, _NY, _PZ, _NZ` (dipole of larger strangers),
`AZ_S_MATE_NEAR, AZ_S_MATE_AHEAD`, `AZ_S_BOND_NEAR, AZ_S_BOND_AHEAD`,
`AZ_S_FAMILY_NEAR, AZ_S_FAMILY_AHEAD` (proximity, and forward component of the
direction, the same scalar style as `AZ_S_HOME`), `AZ_S_TOXIN`, `AZ_S_PRESS`,
`AZ_S_AGE`, `AZ_S_CARRY`.

Cost, step by step: layer 1 goes from 32 x (32 + 32) = 2048 to
32 x (48 + 32) = 2560 products, layer 2 stays 16 x 32 = 512, total
2560 -> 3072 (+20 %). `AzClade.w1` grows by 512 bytes and `ntype` from 80 to
96. `sens[]` is a stack array, so no column changes. `az_brain_step_c` already
loops over `(AZ_SENS + AZ_HID) / 4` groups, so the reference needs no edit.
Changing `AZ_SENS` after the assembly and GPU twins exist is a rewrite of
both; do it now or decide never to.

If 32 must stay: redefine `AZ_S_OTHER_*` as the dipole of larger strangers and
fold smaller strangers into `AZ_S_FOOD_*` weighted by `AZ_T_PREDATION`. That
fixes feature 1 only.

### 4. MAJOR - nothing can take from a colony store deterministically

Evidence. Section 7: "Group effects (colony stores, public goods, marks,
signals) go through cell accumulators or the colony table". D1 allows a
parallel job to write only its own slots, its own worker and "atomic
accumulators". `AzColony` has `u32 store[AZ_NINV]` and nothing else for it,
and `az_tick.c` calls the colony module only in stage 6 (grep above). Adds
commute, so deposits are fine. A bounded take does not: store = 10, members
with uid 5 and uid 9 on different workers each want 8. Either order is legal:
(8, 2) or (2, 8). The two outcomes give different pools and a different state
hash, so the take breaks law 6. The serial alternative is `az_colony_tier2`,
default every 256 ticks, which is too slow for a nurse feeding brood.

Change. Copy the cell pattern.

```c
typedef struct AzColonyEph { u32 dem[AZ_NINV]; u16 grant[AZ_NINV]; u16 pad[AZ_NINV]; } AzColonyEph;
/* AzWorld: */ AzColonyEph *colony_eph;      /* AZ_MAX_COLONIES entries, zeroed each tick, not hashed */
void az_colony_grant(AzWorld *w);            /* serial, called right after job_field_grant */
```

Rule: "Phase 1: a member posts a demand with
`az_atomic_add_u32(&colony_eph[col].dem[k], n)`. Phase 3: it takes
`floor(n * grant[k] / 65535)` and subtracts it with an atomic add. Deposits
are atomic adds to `store[k]`." The loop in `az_colony_grant` is at most 4096
colonies x 4 resources.

### 5. MAJOR - the offer couples theft to trade, and no offer carries the receiver's cap

Evidence, theft. The only inventory quantity in the offer is `AZ_O_GIVE_QTY`,
"most units of GIVE_RES I will hand over" in a trade. Phase 3 forbids reading
the partner's inventory, so a thief can agree with its victim only on that
number. An entity that offers nothing for trade cannot be robbed, and
`AZ_LESION_TRADE` would change stealing, against "A lesion must change nothing
else".

Evidence, cap. Rule C5: "each pool <= AZ_POOL_MAX" (12000). A contest winner
with `pool_a` = 11900 that wins a stake of 500 would hold
11900 + 500 = 12400 > 12000. The loser has already removed 500 and may not
read the winner's pool, so it cannot shrink the transfer. Inventory is worse:
`inv` is u16, 65000 + 1000 = 66000, stored as 464, and 65536 mu vanish from
the matter total.

Change, rule text for section 7:

- "`AZ_O_GIVE_QTY` is the exact amount of `AZ_O_GIVE_RES` the entity holds.
  Unwillingness to sell is `AZ_O_PRICE == 0xFFFF`. `AZ_ACT_STEAL` takes at
  most `GIVE_QTY` of `GIVE_RES`."
- "Receive cap: a receiver adds `min(q, headroom)` to its own pool or
  inventory and posts the rest to its own cell in the same statement block
  (`az_cell_post`, biomass as `AZ_SP_ORG_Y`). The giver always removes the
  full `q`."

With those two sentences share, groom, teach, steal and attack all close
exactly from the two offers. Groom needs no number from the groomer: the
target alone moves its toxin to the cell.

### 6. MINOR - snapshot has no entry point, the hash skips world constants, and world-level module state has no home

Evidence. Experiment B: a registry-built snapshot restores exactly (hash
equal at load and after 60 more ticks, dead slots present). So the design is
sound. But: no `az_snapshot_*` exists; changing `gravity` by 999 and
`sun_period[0]` by 1 leaves `az_state_hash` unchanged (B, third line;
`az_world.c` never mentions them); and `AzWorld` offers the weather, economy
and stats owners no persistent field (storm position and strength, a season
phase, a running price index), while section 12 forbids statics.

Change.

```c
#define AZ_NGLOBAL 64
/* AzWorld: */ i32 gstate[AZ_NGLOBAL];   /* world-level module state: hashed and saved */
enum { AZ_G_WEATHER = 0 /* 0..31 */, AZ_G_ECON = 32 /* 32..47 */, AZ_G_SPARE = 48 };
u64      az_snapshot_bytes(const AzWorld *w);
u64      az_snapshot_save(const AzWorld *w, u8 *buf, u64 cap);          /* 0 if cap too small */
AzWorld *az_snapshot_load(AzArena *a, const u8 *buf, u64 n, AzHost *host);
```

Add `gstate`, `gravity`, `sun_period`, `vent_count`, `vent_cell` to the
globals part of `az_state_hash`, and add the B experiment as a core test:
save at T, run both to T + N, hashes equal.

### 7. MINOR - archetypes: the enum does not exist and the counters cannot separate the diets

Evidence. `az_columns.def:74` refers to `AZ_ARCH_*`; grep finds no definition.
`AzStats.archetype[32]` expects up to 32 classes. The 12 counters are mates,
partners, attacks, wins, losses, shares, received, trades, offspring, travel,
signals, stolen: nothing measures what an entity ate. A phototroph and a
decomposer that never interact have identical counters, and classifying them
by `phen[AZ_P_DIET_*]` would be a label shortcut (law 4), not a "measured
behavioural class".

Change. Define `enum { AZ_ARCH_NONE = 0, ... , AZ_ARCH_COUNT }` (at most 32)
in `az_abi.h`. `AZ_NSTAT` 12 -> 16 (WARM, +8 bytes per entity):
`AZ_ST_EAT_L, AZ_ST_EAT_X, AZ_ST_EAT_Y, AZ_ST_EAT_W`, each incremented in
Tier 1 from this tick's `eat[]` and `c_grant` (still valid in stage 6), so the
WARM column is not touched in the hot loop. Rule: "stat counters saturate at
65535; `az_classify` uses them as rates over `tick - birth_tick`."

### 8. MINOR - Tier 1: allowed reads are not stated, newborns alias sorted position 0, and the 64 spins have no stored patterns

Evidence. Experiment C: all 1322 entities born in 60 ticks reach
`az_tier1_range` with `cell == AZ_NONE` and `spos == 0`; `pub[spos]` is the
record of another entity (uid 2781). `az_spawn` sets `cell`, `tgt` and
`acc_by` to `AZ_NONE` but leaves `spos` zero. Section 4 defines read rules for
phases 1 to 3 only. Memory: `AZ_COL(memory, u8, 8, WARM) /* 64 spins */` is
one 64-bit state; an associative memory also needs the patterns it recalls,
and the only pattern storage is `AzColony.memory[32]`, which a solitary entity
does not have.

Change. In `az_spawn`: `w->spos[s] = AZ_NONE;`. Rule D2 addition: "Stage 6:
`pub[]`, `pub_id[]`, `order[]`, `cell_start[]` and the EPH columns `motor`,
`eat`, `offer` still hold this tick's values and are read-only; an entity with
`cell == AZ_NONE` was born this tick and has none. Tier 1 may call
`az_scan_cell(w, cell[s], spos[s] - cell_start[cell[s]], 1, &p)`." That gives
the spherical-harmonic module its neighbour directions without storing a
percept. Column: `AZ_COL(engram, u8, 16, WARM) /* two learned 64-bit patterns */`;
innate patterns come from the genome, shared ones from `AzColony.memory`.

### 9. MINOR - toxin cannot enter a body from the field, and pressure is undefined

Evidence. Rule C1: "Cell to entity: demand in phase 1, then take
`floor(demand * grant / 65535)`". The channels are
`enum { AZ_D_LIGHT = 0, AZ_D_NUT_X, AZ_D_ORG_Y, AZ_D_WST_W, AZ_D_OXI, AZ_D_STR_S };`
with no `TOX_Z`, so the entity `toxin` pool can only fill from inside, and
`AZ_ACT_GROOM` ("remove toxin from target") has little to remove. Pressure:
`az_cells.def` has no pressure column, yet `AZ_T_PRESS_TOL` must have a reader
and the task list names pressure as acting on bodies.

Change. `AZ_NDIET` 6 -> 8: add `AZ_D_TOX_Z` and `AZ_D_SOLV` (EPH only: `eat`
+4 bytes per entity, `dem` +8 and `grant` +4 per cell) and the two cases in
`az_field_avail`. One shared definition instead of a column:
`u32 az_cell_pressure(const AzWorld *w, u32 c);` in the field module, defined
from `c_sp[AZ_SP_GAS]`, `c_temp` and depth, used by weather, physics,
metabolism and sense alike.

### 10. MINOR - uids run out in long, egg-heavy runs

Evidence. `az_world.c:228`: `w->uid[s] = w->uid_next++;` on a `u32`, no check;
uid 0 means "none". Experiment D: the stub life cycle uses 0.0051 uids per
live entity per tick. At 10^6 entities that is 5100 per tick, and
(2^32 - 1) / 5100 = 842,150 ticks: 23.4 h at 10 ticks per second, 7.8 h at 30,
3.9 h at 60. A clutch of 16 eggs divides those by 16. After the wrap two
entities share a uid, and canonical order, the claim key and the pair RNG all
break. This is the stub rate, so the real number is unknown; the wall is not.

Change. In `az_spawn`: refuse when `uid_next == 0xFFFFFFFF` (count it in
`cap_denied`). Add `void az_uid_compact(AzWorld *w);` (serial, run at the
start of a tick when `uid_next > 0xC0000000`): renumber the living 1..n in
old-uid order, which keeps every comparison; remap `partner`, `parent_a`,
`parent_b` (dead referents become 0) and the founder uids; emit one ledger
event. Widening `uid` to 64 bits instead costs 4 HOT bytes and repacks the
claim key, so it is the worse option.

## What the contract gets right (do not change)

1. **Offers plus the initiator/responder split.** Each entity is in at most
   one pair, both sides compute from two frozen 16-byte offers, and
   `job_act` resolves the pair before any other phase-3 change to that slot.
   Exact conservation without reading the partner's mutable state follows
   from this.
2. **The column registry as the single source of truth, and EPH never
   saved.** Experiment B: a snapshot built only from the registry restores
   the world exactly, with dead slots present and a reorder period of 7.
3. **Demand -> grant -> take, and integer delta accumulators for the fields.**
   It is the one pattern that makes shared resources deterministic; finding 4
   asks for the same pattern on colony stores, not a new one.
4. **The brain kernel is parametric and its inputs are not stored.** Groups
   of four inputs, `sens[]` on the stack, Tier 1 acting only through
   `mod_gain`, `mod_bias`, `arousal`. This is why finding 3 costs no HOT
   bytes.
5. **Passive entities get a zero motor vector but every other hook.** Eggs
   and cysts as prey, wind-borne dispersal, hatching and waking all fall out
   of that one sentence in section 3.

## Priority

Fix before the owners start: 1, 2, 3 (they change `AzPercept`, the scan, the
tick's phase-1 call list and `AZ_SENS`; every module reads at least one of
them). Fix in the same edit because they are one-line rules or EPH-only: 4, 5.
Can follow within the first round: 6 to 10.
