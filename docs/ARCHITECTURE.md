# Asmodensius-Native architecture (ABI version 4)

This is the contract every module is written against. The code that enforces
it is `core/az_abi.h`, `core/az_columns.def`, `core/az_cells.def`,
`core/az_world.c` and `core/az_tick.c`. If this document and those files
disagree, the files win and this document has a bug.

**What changed from version 3 to 4** (2026-11-18, the wave-2 freeze; the full
list, the owner tables and every contested decision are in
`docs/agents/ABI-V4-NOTICE.md`):

- `core/az_owners.def` names the module that reads each of the 96 traits and
  the file or files that honour each lesion bit. `tools/lint_core.py` checks
  it against the code.
- Every column has one writing module, named at its line in the `.def`
  files. No column was dropped. `engram` is four 64-bit patterns, `memory`
  one 64-bit word, `hop_n` counts the stored patterns.
- Rule D8 is spelled out for wave 2: offers and the common path of every
  hook use HOT state only; trait rows are read in pair score and resolve, at
  birth and death, in Tier 1, in serial passes and behind `az_phase_due`.
- New entry points: `az_life_brood` (the one way a brood is requested),
  `az_econ_global` (serial market pass), `az_chem_derive`,
  `az_weather_derive`, `az_look_class`, `az_kin_axis_c` with the kernel slot
  `AzKernels.kin_axis`. New helpers: `az_phase_due`, `az_tier1_due`,
  `az_request_death`, `az_stat_bump`, `az_temp_of`.
- Five event kinds are added after the old ones; `AzConfig` gains
  `lesion_ext` (64 unnamed switches) and a config with an unnamed lesion bit
  is refused.

**What changed from version 2 to 3** (after four adversarial reviews of the
contract; their notes are in `docs/research/review/`):

- Pair scratch is one record: `pair[s]` (`AzPair`: `act`, `urge`, `peer`,
  `claim`, `req`) replaces the columns `act`, `urge`, `tgt`, `acc_by`,
  `claim`, `req`. `peer` is the target for an initiator and the accepted
  claimant for a responder. The tick resets it, not the grid module.
- Births are requested with `az_birth_request(...)` into the mother's own
  `birth` record; there are no per-worker birth queues.
- Each `c_need[c]` holds eight atomic 64-bit demand sums in one cache line.
  A separate per-cell array holds eight 16-bit grants. The pending record is `c_acc[c]` (`delta`, `dheat`, `dmark`, `dmark_id`). Modules use
  `az_cell_post`, `az_cell_post_heat`, `az_cell_demand`, `az_cell_mark`,
  `az_cell_granted` (the first four now take `worker`) and never touch the
  records directly. `az_field_begin` is gone: `az_field_grant` clears demand.
- The scan entry point is `az_scan_range(w, p0, count, out)` over sorted
  positions; `az_scan_cell` is gone.
- `az_social_intent`, `az_econ_intent`, `az_mate_intent`, `az_metab_demand`
  and `az_physics_steer` take a trailing `worker`.
- Dead slots are chained into a free list by `az_grid_build` (through
  `spos`); `az_spawn` reuses them, so a birth is refused only when every slot
  was alive at tick start or has already been reused this tick (a slot that
  dies in phase 3 joins the free list at the next grid build; finding v42).
  A newborn has a valid `cell` (call `az_place`) and `spos == AZ_NONE`.
- `AzClade` is reordered (what a brain step reads is contiguous at the
  front; offsets asserted), has `alive`, `parent_uid`, `species`;
  `az_clade_found(w, seed_id, species)`; clade slots are reused;
  `cfg.max_clades` bounds the living clades.
- The column registry is `az_n_columns()` / `az_column(w, k, &desc)` (and
  the `_cell_` pair). `rank`, `inv`, `role`, `bond` moved to the HOT tier
  (140 bytes). `AZ_CHUNK` is 128.
- `field_stride` must be a power of two. `cfg.debug_split_seed` randomises
  work splits for tests.

## 1. What is being built

A native artificial-life engine: a 3D alien world holding 10^5 to 10^6
individual creatures, each with its own diploid genome, body, metabolism and
small recurrent brain, living through chemistry, weather, gravity, feeding,
fighting, trading, mating, colony life and evolution.

The owner's laws are in `docs/agents/COMMON.md`. The engineering consequences:

| Law | Consequence in the code |
|---|---|
| Native only | `core/` is freestanding C23 plus NASM twins in `kernels/`. No libc, no OS, no floating point in `core/`. |
| Cache-resident | All state lives in one pre-allocated arena. Per-entity state is structure-of-arrays, split in tiers by how often it is touched. The hot tier is exactly 140 bytes per entity; with per-tick scratch and the public record a creature costs about 270 bytes of memory traffic per tick. Hot loops work one 128-entity block at a time. |
| No pagefile | The Windows host locks the arena (`VirtualLock`). |
| Windows only, from Z: (rule L6, 2026-11-17) | The engine runs only as a Windows program. No boot image, nothing without an operating system, no USB boot, no virtual machine. Builds, executables, test runs, snapshots and ledgers live on Z:, the owner's ReFS Dev Drive. The former UEFI host is parked, unbuilt, in `build/dropped-uefi-2026-11-17/`. |
| Below assembly, inside Windows (rule L7) | Go as low as a Windows program can reach: machine code (the VNNI instructions are already emitted as raw bytes because the assembler cannot encode them), cache-set-aware layout, micro-op and port analysis, core placement, locked memory. Gates, transistors and lithography are research background (plan track M in `docs/handoff/STATUS_AND_PLAN.md`). |
| Wild runs and lab replay | A living world requests fresh host entropy at genesis and each tick. A missed tick draw increments the missed-entropy counter. Lab runs use a seed, integer state, counter-based Philox draws and canonical UID order. The signed production matrix matched across its tested worker counts, reorder periods and scalar and assembly kernels. The native GPU test compares Philox and brain-step kernels on one million inputs. A complete GPU world-state comparison still awaits a GPU tick path (v76, plan S2). |
| Exact conservation | Matter and energy are integers. Two global invariants are checked after every tick with `==`, not with a tolerance. |
| Zero cosmetic theater | Each mechanism must produce a measurable change when disabled. Each owned trait must change a simulation outcome. At the signed statistics release, source lint found owner reads for 108 of 112 traits and flagged the missing recurrence test. On the private P4 candidate, strict lint passed for all 112 declared traits and 23 switches. In 24 paired development seeds at 64 ticks, eight switches passed their prespecified primary effect threshold, and all 23 changed at least one final state hash. Individual causal trait checks and the longer-horizon evaluations continue (v77). |
| Indicator only | Cognition metrics are named as mechanistic indicators. Nothing claims sentience. A classical simulation of a small quantum circuit is called exactly that. |

## 2. Repository layout and build

```
core/            freestanding simulation core (C23)
  az_abi.h         the ABI: enums, structs, helpers, every module's entry points
  az_columns.def   per-entity columns (X-macro)      az_cells.def   per-cell columns
  az_world.c       arena, creation, column registry, state hash, audits   (lead)
  az_tick.c        the tick pipeline                                      (lead)
  az_<module>.c    one file per module (owner-written)
  tk_<name>.c/.h   pure kernels derived from the Tsotchke stack
  stubs/           reference stub of every module: compiles, runs, keeps the invariants
kernels/         NASM x86-64 twins of hot kernels, az_kernels.c (CPUID dispatch), win64.inc
platform/win/    Windows host: locked arena (a failed lock is an error), worker pool with
                 soft placement (ideal processors) and dynamic parts, clock (the only host)
gpu/             CUDA driver-API host and PTX kernels
view/            software renderer, skins, glyphs
tools/           az_run (runner), benchmarks, inspectors, lint
tests/           test_<module>.c, one executable each; az_test.h
  protocol/        test-only interacting modules (social, econ, mate, life) for test_protocol
  cli/             checks that drive the built programs (ledger resume, determinism matrix)
docs/            this file, research notes, requirements, reports
```

How to build is in `docs/agents/BUILD.md` (owner builds select only their
own pieces; the integration build takes everything).

## 3. One tick

| # | Stage | Runs | What happens |
|---|---|---|---|
| 0 | reorder, grid, reset | serial entry | every `reorder_period` ticks alive entities are moved into (cell, uid) order and dead slots dropped; then every tick: the cell of each entity, canonical cell lists `order[]`, the public snapshot `pub[]`, the role bit, the free list of dead slots; the tick then resets the `pair` and `birth` scratch |
| 1 | perceive | parallel over sorted positions (a part never splits a cell) | in batches of 32: `az_scan_range` -> percepts. Per entity: `az_sense_cell` when the cell changes; `az_sense` -> 48 inputs; brain step -> 16 motor outputs; `az_social_intent`, `az_econ_intent`, `az_mate_intent`; `az_metab_demand`; `az_physics_steer` |
| 2a | claims | parallel over slots | each initiator asks its target's module for a score and posts `(score, ~uid)` on the target's `pair.claim` with an atomic max |
| 2b | settle, grant | parallel, then serial | the winning initiator writes itself into the target's `pair.peer`; losers drop their intent; cells turn demand into granted fractions; `az_colony_grant` does the same for colony stores |
| 3 | act | parallel over slots, one 128-slot block at a time | pair outcomes; then `az_social_range`, `az_econ_range`, `az_metab_range`, `az_physics_range`, `az_life_range` over the block; then serial: tallies merged, `az_colony_commit` |
| 4 | fields | parallel over cells | commit entity deltas; serial `az_weather_global`; `az_chem_cell` and `az_weather_cell` on the strided cell subset; commit fluxes |
| 5 | life commit | serial | births in (mother uid, n) order, uids assigned, clade bookkeeping |
| 6 | tier 1, tier 2, ledger | parallel / serial | `az_tier1_range` (slow cognition for entities with `(uid + tick) % K1 == 0`); `az_colony_tier2` every `K2` ticks; events to the host |

Eggs and dormant entities get a zero motor vector and no brain step; every
other hook is still called for them, and each module decides what a passive
entity does.

## 4. Determinism rules

- **D1 Write only what you own.** A parallel job writes only the entity slots
  or cells in its range, its own tally (`az_tally(w, worker)`), and atomic
  accumulators through the `az_cell_*` calls. The one exception is built
  into `az_tick.c`: the winning initiator writes its target's `pair.peer`.
  Never write `w->totals` in a parallel stage; the tick merges tallies.
  Module code never indexes `w->workers`: `worker` is an opaque token.
- **D1a Frozen in phase 3.** In phase 3 nobody writes, for any slot
  including its own: `offer`, `motor`, `eat`, `pair.act`, `pair.urge`,
  `pair.peer`, `pair.claim`, `cell`, `spos`, `uid`, `genome`, `trait`,
  `phen`, `sex`, `parent_a`, `parent_b`, `clade`, `tag`. Only the learner of
  a teach pair writes `mod_gain` / `mod_bias` in phase 3. The outcome of a
  pair is fixed by the two offers; neither side can cancel it. The only
  writable member of `pair` in phase 3 is `req` (for `AZ_RQ_DIE`).
- **D2 Do not read what another worker may be writing.**
  - Phase 1: read your own entity freely; read others only through their
    public record `pub[]` (32 bytes: sub-cell position, velocity, size code
    and flags, tag, colony, signals, uid, parents, bond partner; all
    start-of-tick values). Field columns are read-only.
  - Phase 2: nothing but `pair.claim` is written, so a score may read both
    offers, both public records, both trait rows, `flags`, and the
    responder's own memory of others (`rep_tag`, `rep_val`, `partner`,
    `bond`, `rank`). A score is a pure function of those: it runs once per
    claimant on the claimant's worker, so it never tallies, emits or writes.
  - Phase 3: from your partner read only its frozen scratch (`offer`,
    `pair`, `motor`, `eat`), its public record `pub[spos[partner]]`, and
    columns nobody writes in phase 3: `uid`, `genome`, `trait`, `phen`,
    `sex`, `parent_a`, `parent_b`, `clade`, `tag`, and (of a teacher)
    `mod_gain`, `mod_bias`. Never its pools, energy, inventory, rank, stats,
    colony or flags.
  - Field columns (`c_sp`, `c_heat`, ...) are read-only in phases 1 to 3.
    Field dynamics read neighbours' `c_sp` and `c_heat` and may read and
    write the derived columns of their own cell; the stride guarantees no
    two face-adjacent cells are stepped in the same pass.
  - Stage 6 (Tier 1): `pub[]`, `order[]`, `cell_start[]` and the EPH columns
    `motor`, `eat`, `offer` still hold this tick's values and are read-only.
    Nothing writes flags in stage 6, so another entity's `flags` may be read.
    An entity born this tick has a valid `cell` but `spos == AZ_NONE`: it has
    no public record and is in no cell list until the next grid build; test
    `spos` before using `pub`. Tier 1 may call `az_scan_range` (one position)
    or `az_scan_list` for its own entity, passing its `worker` token (the
    scan keeps its working buffer in that worker's scratch block).
- **D3 Break ties by uid.** Never by slot index, sorted position, pointer or
  iteration order of a non-canonical container. The canonical iteration
  order of neighbours is: cells in increasing dz, dy, dx; inside a cell,
  increasing uid.
- **D4 One source of randomness.** `az_rand`, `az_rand4`, `az_rand_below`
  (Philox4x32-10 keyed by seed; counter = id, tick, stream, index). No time,
  no addresses, no uninitialised memory. A pair draws with the smaller uid as
  id and the larger in `idx`, so both sides get the same numbers. The one
  exception is the role bit `az_is_initiator` (a keyed hash of uid and tick).
- **D5 Integers with defined behaviour.** No float or double in `core/`. No
  signed overflow, no shift by a negative or too-large count, no division by
  zero, every narrowing through a clamp (`az_sat_*`). When a clamp changes a
  physically meaningful value, count it: `az_tally(w, worker)->sat[AZ_SAT_*]++`.
- **D6 Slots are not identities.** A slot index is valid for one tick. Across
  ticks refer to an entity by uid (`partner`, `parent_a`). Three index spaces:
  `slot` (column index), `spos` (position in the cell-sorted arrays;
  `order[spos[s]] == s`), `uid` (stable, never reused, starts at 1, 0 = none).
  A dead slot may be reused by a birth in a later stage of a later tick.
  Column base pointers may change at a reorder: never keep one across
  `az_tick` calls. A world ends after 2^32 - 2 births (`uid_exhausted`
  counts refusals); it continues only through a snapshot and offline
  renumbering.
- **D7 Nothing outside the simulation may change it.** No level of detail,
  culling, governor or shortcut may depend on a camera, a viewer, wall-clock
  time, temperature or thread count. A viewer only reads between ticks.
- **D8 Hot paths stay in the hot tier.** Phase-1 hooks and the per-slot loops
  of the phase-3 ranges read HOT, scratch and `pub` on their common path.
  WARM columns are read only on a branch that a minority of entities take in
  a tick (an entity that really acts), in pair score and resolve, at birth
  and death, and in Tier 1. A trait needed by every entity every tick must
  be one of the 16 `phen` scalars. The standard minority branch is phased
  work: `az_phase_due(w, uid, salt, log2_period)` with a period of at least
  4 ticks; what it computes for the ticks in between is kept in a HOT column
  (`sig`, `flags`, `arousal`). Offers are written by everybody every tick,
  so they are built from HOT state only; the trait part of a contest, a
  choice or a price is read from the trait rows when the pair is scored or
  resolved.

Tests that hold these rules in place (see `tests/test_core.c`): the hash is
equal for 1, 2, 5 and 12 threads; for reorder periods 1, 7, 31 and 255, also
when the slot capacity binds; and for randomised work splits.

## 5. Units

| Quantity | Type | Unit |
|---|---|---|
| position | `u32` per axis | the top `gbits` bits are the cell index; x and y wrap, z is bounded |
| neighbour space | `i32` | one cell edge = `AZ_SUB_ONE` = 16384; all neighbour offsets and radii |
| velocity | `i16` | 1/65536 cell per tick: `pos += vel << (shift - 16)` |
| rotor | 4 x `i16` | unit quaternion (w, a, b, c), Q1.14, 16384 = 1.0; `az_rotor_apply` maps body frame to world frame |
| body frame | | x = forward, y = left, z = up |
| size code | 0..15 | from body matter by `az_radius_code`; radius = (code + 1) / 32 cell |
| matter | integer | matter units, mu |
| energy | integer | energy units, eu |
| sensory inputs, hidden | `u8` | 0..127 |
| motor outputs | `i8` | -127..127 |
| traits, phen | `u8` | 0..255, 128 = neutral unless stated |
| time | `u32` tick | one tick; no wall-clock anywhere in `core/` |

One fixed-point format per quantity; there is no universal Q format.

## 6. Matter and energy

Matter can be: a field species in a cell (`c_sp`), an entity pool (`pool_a`,
`pool_m`, `pool_t`, `waste`, `toxin`), entity inventory (`inv`), a colony
store, or the buried reservoir. Every form has a chemical energy per unit:

| Form | eu per mu |
|---|---|
| `AZ_SP_NUT_X` reduced mineral | 24 |
| `AZ_SP_ORG_Y` organic matter; `pool_a`, `pool_m`, `pool_t` (biomass) | 16 |
| `AZ_SP_TOX_Z` radiotoxin; entity `toxin` | 4 |
| `AZ_SP_WST_W` waste; entity `waste` | 2 |
| `AZ_SP_SOLV`, `AZ_SP_GAS`, `AZ_SP_OXI`, `AZ_SP_STR_S`, reservoir | 0 |

Inventory slot k holds species `az_inv_species[k]` (NUT_X, ORG_Y, STR_S, OXI).
Energy is: entity `energy`, cell `heat`, and the chemical energy above.

Two invariants hold after every tick, exactly:

```
az_total_matter(w)  ==  constant
az_total_energy(w)  ==  constant        (it already subtracts energy_in and adds energy_out)
```

Rules for every module:

- **C1** Move matter only as integer mu from one place to another. Entity to
  cell: `az_cell_post(w, worker, cell, species, +n)`. Cell to entity: post a
  demand in phase 1 (`eat[d] = n` and `az_cell_demand(w, worker, cell, d, n)`),
  then in phase 3 take `az_cell_granted(w, cell, d, n)` and post `-taken`.
  A grant is defined only for a channel that had demand this tick.
- **C2** Whenever matter changes form, book the energy difference in the same
  statement block. Released energy goes to the entity's `energy` and/or to
  cell heat (`az_cell_post_heat`). Absorbed energy must be paid from `energy`.
- **C3** Every action has an energy cost paid as heat:
  `pay = min(cost, energy); energy -= pay; az_cell_post_heat(w, worker, cell, pay);`
  If the entity cannot pay, scale the action down. `energy` stays in
  `0 .. AZ_ENERGY_MAX`; excess becomes heat.
- **Births**: in phase 3 a mother calls
  `az_birth_request(w, slot, father_slot, n, energy, a, m, t)` once and, only
  when it returns true, deducts exactly that provision from herself in the
  same block (it refuses a second call, n out of range and any provision
  above 65535; finding v07).
  `az_life_commit` creates the children in (mother uid, child index) order,
  splits the provision among them, and returns what could not be used to
  the mother's cell.
- **C4** External flows are booked in the worker tally: light absorbed and
  geothermal input into `az_tally(w, worker)->energy_in`, radiative loss
  into `energy_out`, burial and vent emission into `reservoir` (signed).
  Matter is never created or destroyed.
- **C5** Limits: each pool `<= AZ_POOL_MAX`, each inventory slot
  `<= AZ_INV_MAX`. **Receive cap:** a receiver adds `min(q, headroom)` to its
  own pool or inventory and posts the rest to its own cell in the same
  statement block (biomass as `AZ_SP_ORG_Y`). A giver always removes the full
  amount. `mass` is a cache of the pool sum, refreshed by `az_metab_range`.
- **C6** No free value. A classical payoff matrix that hands out more than
  was put in (benefit > cost from nowhere) is not allowed. Gains from
  cooperation and trade must come from state: the same unit is worth more to
  the entity that lacks it.
- **C7** Field transport: a flux out of a cell through one face is at most
  1/8 of what the cell held at the start of the pass, so six faces can never
  overdraw. Round with a Philox bit, not by truncation, or gradients freeze
  into staircases.
- **Colony stores** use the cell pattern: a member posts a demand with an
  atomic add to `colony_eph[col].dem[k]` in phase 1; `az_colony_grant` turns
  demand into a fraction; in phase 3 the member takes
  `floor(n * grant / 65535)` and adds it atomically to `taken[k]`; deposits
  are atomic adds to `dep[k]`; `az_colony_commit` applies both. The tick
  zeroes `dem` and `grant` every tick but never `dep` and `taken`: what a
  commit cannot apply stays pending, is counted by the audits and the state
  hash, and a snapshot refuses the world until it is applied (finding v40).
  Colony 0 is "no colony": nothing deposits to it or takes from it.
- Death: `az_life_kill` returns every pool, the inventory and the energy to
  the cell. Request a death by setting `pair[s].req |= AZ_RQ_DIE`;
  `az_life_range` runs last in the block and performs it.

## 7. Interactions

Each tick an entity is either an initiator or a responder (`AZ_F_INIT`,
a hash of uid and tick), so it takes part in at most one pair interaction.

1. Phase 1, three intent hooks in a fixed order, for every alive entity:
   `az_social_intent` (zeroes the 16-byte `offer`, writes its bytes),
   `az_econ_intent` (bytes 6..11), `az_mate_intent` (byte 3 and the mating
   bits). Each may propose an act: it may replace `pair.act` / `pair.peer`
   only if its urge is strictly greater than `pair.urge`, and then it stores
   its own urge. Only an initiator proposes, and only to a responder: test
   the target's public uid with `az_is_initiator` (the percept is
   role-blind). Targets are slots taken from the percept. Which act a module
   proposes is a rule over the brain's drives (`AZ_M_AFFIL`, `AZ_M_AGON`,
   `AZ_M_MATE`), the hot phenotype and state; a module reads its traits only
   on the branch where it really proposes. Trade has no motor channel of its
   own and rides on `AZ_M_AFFIL` and `AZ_T_TRADE_DRIVE`.
2. Phase 2a: for each initiator the tick calls the owning module's score
   function, `score(w, responder, claimant, act)`. Negative = refused. For
   coercive acts (attack, steal) the score is the responder's priority among
   attackers, not consent.
3. Phase 2b: the highest score wins the responder (ties: lower uid).
4. Phase 3: both sides run `resolve(w, slot, partner, act, initiator, worker)`.
   Each changes only itself. Both compute the outcome from the two offers,
   the public records, constant columns, and `az_rand` keyed by the two uids,
   so they agree exactly.

Offer rules that make every transfer exact:

- Quantities in an offer are exact and capped by what the entity holds.
- `AZ_O_GIVE_QTY` is the exact amount of `AZ_O_GIVE_RES` the entity holds.
  Unwillingness to sell is `AZ_O_PRICE == 0xFFFF`. `AZ_ACT_STEAL` takes at
  most `GIVE_QTY` of `GIVE_RES`, whether or not it is for sale.
- `AZ_O_ENERGY` and `AZ_O_MATTER` are what the entity gives (share) or loses
  (lost contest). The giver always removes the full agreed amount; the
  receiver applies the receive cap (C5).
- Groom needs no number from the groomer: the target moves some of its own
  toxin to its cell.
- A lesion of trade or mating discards that module's proposal, never its
  offer bytes, so the lesion changes nothing else.

Owners: share, groom, teach, recruit, attack, steal -> `az_social.c`;
trade -> `az_econ.c`; mate -> `az_mate.c`.

Exact prices: `az_tally(w, worker)->trade_units[i][j] += units of i given
for j`; the price of i in j is a ratio of two exact totals.

## 8. Heredity

Two inheritance systems:

- **The individual genome** (`genome`, 128 bytes): diploid, 2 haplotypes of
  64 loci, each locus 4 bases of 2 bits. Recombines by meiosis, mutates per
  base. The world's genetic code (`w->code`, made once from the seed) says
  which three loci each trait reads and with what weights and dominance.
  Development (`az_develop`) turns the genome into 96 quantitative traits
  (`trait`), 16 hot phenotype scalars (`phen`), the recognition `tag`, `sex`,
  and the innate part of the brain modulation. A brood is requested through
  `az_life_brood(w, mother, father, worker)` only: `father == AZ_NONE` is
  budding, `father == mother` selfing, anything else a sexual pair (called by
  `az_mate_resolve` for the bearing side). `az_life.c` decides clutch size
  and provision and is the only caller of `az_birth_request`.
- **The neural program** (`AzClade`): neuron types, an 8x8 wiring rule, type
  biases and a variation seed, developed into the int8 network
  (`az_clade_develop`). Inherited from the bearing parent only. A rare
  macro-mutation founds a new clade. Members of a clade share the developed
  network; individuals differ through their own genome, modulation and
  learning.

## 9. Senses and the brain step

48 input channels, all 0..127 (`AZ_S_*`). A direction is encoded on four
tetrahedral receptor axes (`az_tetra4`) in the body frame, so every channel
is non-negative: food gradient, threat (larger strangers), prey (smaller or
passive strangers), kin and colony-mates. Specific others come as a
proximity channel plus an "ahead" channel: best mate, bond partner, family
(own parent or own child), home. The rest are the body's own state and the
local medium. `az_sense_cell` computes what a whole cell shares
(`AzCellSense`); `az_sense` combines it with the entity's percept and state.

Exact brain semantics are in `core/az_brain.c` (`az_brain_step_c`); the copy
in `core/stubs/` is what the stubs-only build links (finding v80).
Inputs and hidden activations are unsigned 0..127, weights signed int8, so
one AVX-VNNI dot product computes four products per output without
saturation. Hidden unit `h` uses `mod_gain[h & 7]` (64 = 1.0) and
`mod_bias[h & 7]`.

Tier 1 never feeds the brain directly. It writes `mod_gain`, `mod_bias` and
`arousal`, which the hot loop reads.

## 10. Lesions

`AzConfig.lesion` bits switch one mechanism off (`AZ_LESION_*`). A lesion is
data on the same code path (a mask or a zero gain), not a different
algorithm. A module that implements a mechanism honours its bit and ships a
test showing that the lesioned run differs from the intact run in a measured
outcome, on seeds that were not used to tune anything.

Which file honours which bit is data: the `AZ_LESION_OWNER` lines of
`core/az_owners.def` (table in `docs/agents/ABI-V4-NOTICE.md` section 3). Ten
bits are honoured by `az_tick.c` as a mask or a skipped call; the others by
the module that owns the mechanism; three are shared by two files (GRAVITY:
physics and chem; TOXIN: genome and metab; COLONY: tick and social). Bits
21..31 and `AzConfig.lesion_ext` have no names yet; a config that sets one is
refused.

## 11. Tests

- One file per module: `tests/test_<module>.c`, using `tests/az_test.h`
  (`CHECK`, `CHECK_EQ`, `az_test_config`, `az_test_world`, `az_test_done`).
- Every module ships: unit tests of its functions; a property test over many
  random inputs; an integration test that runs a small world and checks both
  invariants every tick, `field_underflow == 0`, and the thread-count and
  reorder-period hash equalities; and a lesion test if it has a lesion bit.
- An assembly twin ships a test comparing it with the C reference on at least
  100,000 random inputs plus edge cases, bit for bit.
- Seeds: tune on seeds 1..99. Report results on the preregistered seeds
  5001..5024 and 5101..5124. Never tune on a reporting seed. Unit tests use
  tuning seeds only: the two reporting blocks are reserved for the
  preregistered milestone-8 runs (rule tightened 2026-11-17; an older unit
  test that still uses a reporting seed because a pinned value depends on it
  is listed in build/work/PROGRESS.md).
- A test that cannot fail is a defect. Each check should be able to fail if
  the code under test were wrong. A new test is therefore run against planted
  defects (scratch copies of the tree with one fault each) and must fail on
  every one; the record says which it caught.
- The pair protocol, deaths, slot reuse and both kinds of birth are exercised
  by `tests/test_protocol.c` on a test-only interacting module set
  (`tests/protocol/az_social.c`, `az_econ.c`, `az_mate.c`, `az_life.c`, built
  as the library `az_core_proto`; never part of the engine). It requires equal
  hash traces, ledger chains and totals across worker counts, reorder periods,
  work splits and snapshot resume, also under a binding capacity, and it
  requires the interactions to happen: deaths, births in freed slots, sexual
  and budded births, resolved pairs of every kind the set makes, and claims
  that are refused or lost. Until wave 2 replaces the stubs this is the only
  world with deaths and pairs. The set is also the worked example of the
  intent, score and resolve hooks of section 7.
- The same equalities are checked through the host program:
  `tests/cli/determinism_matrix.py` drives `az_run` (ctest `cli_matrix_engine`)
  and `az_run_proto`, the same program on the protocol set (ctest
  `cli_matrix_protocol` and `cli_matrix_protocol_cap`), across 1 to 24 workers
  with the E cores, pinned and unpinned, reorder periods 7, 31 and 255, split
  seeds, kernels 0 and 1, and a snapshot saved mid-run and resumed at another
  worker count; every run audits matter and energy at every tick and shows the
  arena locked.
- Undefined behaviour: a second build folder configured with `-DAZ_UBSAN=ON`
  compiles the core, the hosts and the tests with `-fsanitize=undefined
  -fsanitize-trap=undefined` and without `-fwrapv`, so a violation executes an
  illegal instruction. `tools/ubsan_check.py` (ctest `ubsan_trap_build`) builds
  it, shows that six planted violations trap (`tests/ubsan_probe.c`) and runs
  every test program there.
- A hand-built `AzWorld` in a test comes from `az_test_shell()` (aligned as the
  type needs), never from `calloc`.

## 12. Coding rules for `core/`

- Includes: `az_abi.h` (and a `tk_*.h` you use). No libc calls
  (`az_memset`, `az_memcpy` exist).
- No floating point. No recursion. No heap. No mutable globals or statics
  (`static const` tables are fine). Loops have explicit bounds.
- Stack: no function frame above 2048 bytes (the build refuses one:
  `-Werror=stack-usage=2048`), no variable-length arrays, no `alloca`. Large
  scratch comes from `w->tmp` in serial stages, and in a parallel job from
  the worker's own scratch block: `az_scratch(w, worker)` gives the running
  module hook `AZ_WS_MODULE_BYTES` (16 KiB) of private bytes, undefined on
  entry and valid until the hook returns. The scan and the tick have their
  own parts of the block (`az_ws_scan`, `az_ws_tick`), so a hook that calls
  `az_scan_list` keeps its scratch. Nothing in the block is state: it is
  never hashed or saved.
- The only symbols the core may leave undefined are `memcpy`, `memset`,
  `memmove`, `memcmp` (a compiler may emit them for a struct copy); the
  host's C runtime supplies them. Nothing else: no compiler runtime calls
  (so no 128-bit division, no floating point helpers).
- Code that runs in a parallel stage never touches `w->host`, `w->k`,
  `w->arena`, `w->tmp`, `w->tmp_u32`, `w->splits`, `w->workers`,
  `w->prof_ns`, and reaches `w->ws` only through `az_scratch`. Serial-stage code (grid, life commit, ledger, colony
  passes) may use `tmp`, `tmp_u32` and `host->parallel_for`.
- A struct that is hashed, saved or sent to a GPU has fixed-width integer
  members, explicit padding, no pointers, and a size assert.
- `-fwrapv` and `-fno-strict-aliasing` are safety nets in the build; code
  must not rely on them.
- Every function either is in `az_abi.h` (or a `tk_*.h`) or is `static`.
- ASCII only, LF line endings, no tabs. Comments say why, not what.
- World-level module state that must persist goes in `w->gstate[]`
  (`AZ_G_*` ranges); it is hashed and saved.
- Do not add per-entity or per-cell state anywhere except through the
  `.def` files, which only the lead edits. If you need a field, say so in
  your report.
- Do not edit a file you do not own. If another module must change, write it
  in your report under "needs other owner".
