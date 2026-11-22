# ABI version 4 notice (2026-11-18): the wave-2 freeze

The contract moved from version 3 to 4 in one change. It holds everything the
thirteen wave-2 modules need, so that no owner has to ask for another owner's
file. If your build broke on `engram`, `memory`, `az_delta_ns`, `AzConfig` or
a missing `az_econ_global`, `az_life_brood`, `az_chem_derive`,
`az_weather_derive`, `az_look_class` or `az_kin_axis_c`: this is why.

**Read section 5 first.** It was added the same day on the owner's
instruction ("nothing fixed") and overrides sections 1 to 4 wherever they
differ; the places are marked.

Read, in this order: `core/az_abi.h`, `core/az_columns.def`,
`core/az_cells.def`, `core/az_owners.def`, then your brief in
`docs/agents/W2-BRIEFS/`. The worked example of the pair protocol (intent,
score, resolve with exact books) is the test-only module set in
`tests/protocol/`.

## 1. What changed (one line each)

| Version 3 | Version 4 |
|---|---|
| `engram` u8 x 16, `memory` u8 x 8 | `engram` u64 x 4 (`AZ_HOP_PATTERNS` tk_hop words), `memory` u64 x 1, new WARM `hop_n` u8 (patterns stored) |
| inventory could only be filled from the body | new EPH column `gather` u16 x 4: econ's own demand on the cell per inventory resource (metab keeps `eat`) |
| `az_delta_ns` | removed (no caller; finding v64). `az_offer_u16` stays: the resolvers read offers with it |
| `AzConfig` 40 bytes | 64 bytes: `u32 lesion_ext[2]` after `lesion` (64 unnamed switches, must be 0); `max_clades`, `max_colonies`, `store_units`, `store_period`, `mode` (section 5); old snapshots do not load |
| unnamed lesion bits were ignored | `az_world_create` refuses a config with a lesion bit that has no name |
| births: any module called `az_birth_request` | only `az_life.c` does; everybody else calls `az_life_brood(w, mother, father, worker)` |
| deaths: `pair.req \|= AZ_RQ_DIE` | `az_request_death(w, slot, AZ_DIE_*)`: the cause goes into the `AZ_EV_DEATH` event (`aux`) |
| Tier 1 schedule written out by hand | `az_tier1_due(w, uid)`; phased work `az_phase_due(w, uid, salt, log2_period)` |
| `AzKernels`: brain_step, philox_n | plus `kin_axis` (the integrator of one wrapping axis; C definition `az_kin_axis_c` in az_physics.c) |
| event kinds 1..13 | plus `AZ_EV_SHARE`, `STEAL`, `RECRUIT`, `HATCH`, `STAGE` (14..18; old numbers unchanged); table of who emits what in az_abi.h |
| `gstate` ranges only | named words other modules read: `AZ_G_SUN0_PHASE`, `SUN1_PHASE`, `SUN0_LIGHT`, `SUN1_LIGHT`, `STORM`, `STORM_CELL`, `SEASON`, `AZ_G_PRICE` (4), `AZ_G_VOLUME` (4) |
| mark channels 0..3 | named: `AZ_MK_TERRITORY`, `TRAIL`, `ALARM`, `NEST` |
| no shared temperature | `az_temp_of(heat, matter)`, `AZ_TEMP_OF_TOPT`, `AZ_TEMP_TOL` |
| `AzStats` | more fields (stages, roles, diets, species, look classes, mean age and mass) |
| tick: no market pass | `az_econ_global(w)`, serial, after phase 3 (skipped under `AZ_LESION_TRADE`) |
| chem / weather: one entry each | plus `az_chem_derive(w, c)`, `az_weather_derive(w, c)` (derived columns only, no posts) |
| stats | plus `az_look_class(w, slot)` |
| lint W1: a trait name anywhere in a real module | W1: read by the owner `az_owners.def` names, by name or through its carrier; W2: honoured by every owner file |

New constants: `AZ_VEL_MAX` 16000, `AZ_HOP_PATTERNS` 4, `AZ_PHASE_OF(phase, species)`,
`AZ_DIE_*` (causes of death).

Additions made earlier in this run, already in version 3 (listed here so one
page has them all): per-worker scratch (`az_scratch(w, worker)`, 16 KiB for
the running hook; `az_ws_scan`, `az_ws_tick`); `az_scan_range` and
`az_scan_list` take the caller's `worker`; the event pool (`az_events_close`,
`az_events_reset`); `az_snapshot_config`; `AZ_EV_COUNT`; `az_event_less`;
`AzHost.sink_ctx`; `az_host_parallel`; `az_ledger_fold`; `az_stat_bump` and
`az_request_death` are new in 4.

## 2. Decisions (each was open; this is what was decided and why)

1. **No column is dropped (findings v59, v63).** The fifteen entity columns
   and six cell columns that the stub world never touched are the state of
   wave-2 mechanisms. Each now has one writer, written at its line in the
   `.def` files. Dropping them and adding them back module by module would
   have changed the hash and the snapshot format thirteen times.
2. **The HOT tier holds bookkeeping only (124 bytes with the block handle;
   it was 140, see 5.1).** There is no age byte and no extra
   phenotype scalar. A trait that is needed every tick must be one of the 16
   `phen` scalars; every other trait is read from the WARM row only where
   rule D8 allows: pair score and resolve, birth and death, Tier 1, serial
   passes, and behind `az_phase_due` with a period of at least 4 ticks. What
   a phased branch computes for the ticks in between lives in a HOT column
   (for example `sig`, `flags`).
3. **Life stages have no column.** Stage = flags + `birth_tick` + traits.
   `az_life.c` is the only writer of ALIVE, EGG, JUV, SENESC, DORMANT and
   RECEPT; it changes stage in a phased check. RECEPT means "ripe" (an adult
   above its own thresholds) and is what the mate module and the senses see.
4. **One owner per trait, as data.** `core/az_owners.def` gives each of the
   112 traits (96 before section 5) a module and the way the value gets there (ROW, a phen scalar,
   or the tag). Any module may read any trait; the owner is answerable: its
   test shows the trait changing an outcome, and `tests/test_traits.c`
   checks them all the same way (clamp low, clamp high, compare a digest of
   the world that leaves the trait and its carriers out).
5. **Offers come from HOT state only.** Everybody writes an offer every
   tick, so no offer byte may need a trait. What a trait adds is read from
   the trait row of either side at score or resolve time (the row is constant
   in phases 2 and 3). The trade bytes are written only by an entity that is
   in the market this tick.
6. **A score is a pure function.** `az_*_score` runs on another worker's
   thread once per claimant: no tally, no event, no write.
7. **Births go through `az_life_brood`.** Clutch size and provision are
   life-history traits, so `az_life.c` decides them for budding, selfing and
   sexual pairs alike; `az_mate.c` decides who mates with whom and calls it
   for the bearing side.
8. **Colonies are founded in the serial Tier 2 pass only.** Joining is at
   birth (the mother's colony) or by accepting `AZ_ACT_RECRUIT`. Roles are
   assigned in Tier 2.
9. **Derived cell columns have one writer each.** chem: `phase`, `med`, and
   the decay of `mark`; weather: `temp`, `wind`, `charge`, `light`; worldgen:
   `solid`, and it initialises the derived ones through the two derive
   functions. A reader may see a value up to `field_stride - 1` ticks old.
10. **Shared lesions.** `GRAVITY` is honoured by physics (bodies) and chem
    (settling of matter); `TOXIN` by the genome (mutation term) and metab
    (uptake, damage, secretion); `COLONY` by the tick (no Tier 2 pass) and
    social (no recruiting). `MATING` drops mate pairs only: budding and
    selfing stay, obligate outcrossers stop reproducing. `CARE` is social's
    (care proposals toward one's own young).
11. **Room for more switches.** Bits 21..31 of `lesion` and the 64 bits of
    `lesion_ext` are unnamed. Milestone 8 names what the suites need; the
    config and snapshot layout do not change again for that.
12. **Hopfield working memory: four patterns in the fixed record; a
    long-term memory of any length in the block (5.5).** tk_hop is reliable up
    to about 6 random patterns of 64 spins; four per creature plus the four
    of its colony (`AzColony.memory`) stay inside that. `hop_n` counts the
    stored ones; `tk_hop_store` replaces the most similar when full.
13. **Events are budgeted.** The pool holds 2 events per slot and tick: an
    entity emits at most one event from its phase-3 hooks (its pair act, by
    the initiator, or its stage change) plus its death.
14. **No in-world census.** `AZ_G_STATS` has no writer: a census nobody in
    the world reads would be theatre. Classification is per entity
    (`archetype`); `az_stats_collect` and `az_look_class` are measurements
    for tools and the viewer and feed nothing back.
15. **The sex locus (finding v09) is unchanged.** Marker loci feed no trait.
    `SEX_BIAS` acts on which hermaphrodite bears.
16. **The integrator twin.** physics gathers one block into scratch arrays
    and calls `w->k.kin_axis` for x and y; z (bounded, reflecting) stays in
    C. The NASM twin must equal `az_kin_axis_c` on every input.

## 3. Lesion owners

| Bit | Honoured in | What stops |
|---|---|---|
| BRAIN, RECURRENCE, NOISE, SENSES | az_tick.c | masks on the brain step |
| SOCIAL, TRADE, MATING | az_tick.c | proposals dropped (TRADE also skips `az_econ_global`) |
| WEATHER, CHEM | az_tick.c | the field step hooks are not called |
| COLONY | az_tick.c, az_social.c | no Tier 2 pass; nobody recruits or joins |
| MUTATION | az_genome.c | no new mutation |
| TOXIN | az_genome.c, az_metab.c | toxin term of the mutation rate; uptake, damage, secretion |
| KIN | az_scan.c | the kin window is closed |
| SIGNALS | az_social.c | `sig` is written 0 |
| CARE | az_social.c | no care toward one's young |
| GRAVITY | az_physics.c, az_chem.c | weight of bodies; settling of matter |
| PLASTICITY, METRIC, TIER1_Q, TIER1_SH, MEMORY | az_tier1.c | learning; its preconditioner; tk_qsv; tk_sh; tk_hop |

## 4. What every wave-2 module must show before it lands

- Its own test passes in a normal and in a `-DAZ_UBSAN=ON` build; `test_core`
  passes with the module in; matter and energy are checked with `CHECK_EQ`
  after every tick of its test world.
- The hash is equal for 1, 3 and 8 workers and reorder periods 1 and 9 (use
  `tests/cli/determinism_matrix.py` on `az_run` for the full matrix).
- Each lesion bit it owns changes a measured outcome in its test, and the
  test names the bit (`AZ_LESION_*`), which is what lint W2 looks for.
- Each trait it owns is read as `az_owners.def` says and comes out as
  MATTERS in `tests/test_traits.c`.
- `python "Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native\tools\lint_core.py" "Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native"` reports no error and no
  warning that
  names the module.
- Tuning uses seeds 1 to 99 only. No reporting seed (5001-5024, 5101-5124).

## 5. Amendment of 2026-11-18: nothing fixed

The owner's instruction, in his words: "Yes, leave room." "Life isn't fixed
or fixed sizes or fixed anything." "Yes, nothing fixed." And the standing
list: dynamic, adaptive, responsive, reactive, interactive, spectrums,
variances, gradients, breadths, depths, vectorized, curved, directional,
linear, layers, game theories, non deterministic, wild, feral, chaotic,
improvized. `docs/requirements/OWNER-WORDS.md` maps every word to the place
in the code that carries it. The ABI number stays 4. This section is part of
the freeze.

### 5.1 The fixed record is bookkeeping; the creature is in its block

| Bytes per creature | Before | After |
|---|---|---|
| Fixed record read every tick (HOT, plus the 4-byte handle) | 140 | 124 |
| WARM (events, birth, death, Tier 1) | 387 | 325 |
| Per-tick scratch (EPH) | 102 | 170 |
| Genome, brain, body | none of its own: 80 fixed loci in a column, the brain shared by its lineage (3,520 B per lineage) | its own block: 30 to 55 units of 64 B measured, mean 2,740 B (two haplotypes of about 80 genes of 4 B, a brain of about 1.85 KB, a mind of 128 B, 8 B per body part) |

Every creature has one block in the creature store (`core/az_store.c`), a
part of the locked arena, behind the handle column `blk` (tier HND: moved by
the reorder, not hashed as a number, not saved as a number). The block is
hashed and saved as bytes. It has a 64-byte header and six segments, in the
order of use:

| Segment | Holds | Who changes its length |
|---|---|---|
| `AZ_SEG_BODY` | `AzPart[]`, 8 bytes a part: matter (u32), kind, health | `az_part_request`, `az_part_shed_request` |
| `AZ_SEG_MIND` | the last senses and the hidden state of the brain | birth (with the brain) |
| `AZ_SEG_BRAIN` | `AzBrain`: the creature's own sparse int8 network | birth (az_brain.c: inheritance with neural mutation) |
| `AZ_SEG_GEN_A`, `AZ_SEG_GEN_B` | `AzGene[]`, 4 bytes a gene: the two haplotypes | birth (az_genome.c) |
| `AZ_SEG_MEMORY` | `u64[]`: what the creature has learned and keeps | `az_memory_request` (az_tier1.c owns the words) |

Read with `az_seg`, `az_parts`, `az_body_matter`, `az_genes`, `az_brain`,
`az_memory`. A hook reads and writes the block of its own slot (C1 to C7
hold for blocks as for columns) and reads the public record of others, never
their blocks. A hook that walks slots and reads blocks calls
`az_blk_prefetch(w, slot + 4)` for a slot a few steps ahead.

Rules that follow, for every module:

- **R1. No size is assumed.** Never the number of genes, neurons, tiles,
  parts, memory words, members of a colony, or young in a brood. Loops run
  over what the block or the record says. A creature may have no hidden
  neuron, one body part, no memory, or a genome that lacks a gene family:
  each is a legal creature.
- **R2. No number of the biology is a constant in a module.** A number that
  shapes what a creature is or does is (a) a trait read from the creature,
  (b) a property of its own brain, body or memory, or (c) a world parameter
  with a recorded default and range (5.3). Laws of the physics that
  `az_abi.h` names (`AZ_EPS_*`, `AZ_VEL_MAX`, the species list) are laws,
  not parameters.
- **R3. Running out is an event, never a crash.** The store, the slot table,
  the lineage table and the colony table are memory; their sizes are set at
  run time (`AzConfig.cap`, `store_units`, `max_clades`, `max_colonies`).
  When one is full, the request is refused, counted in `AzTotals`
  (`cap_denied`, `store_denied`, `clade_denied`, `colony_denied`) and
  written to the ledger once per kind (`AZ_EV_REFUSED`, `AZ_REF_*`); the run
  goes on. A module checks the `bool` it gets and carries on.
- **R4. Changes of length are asked for, not made.** Only serial engine code
  resizes blocks. A hook asks from its own slot, in phase 3 or in the Tier 1
  stage: `az_part_request(w, slot, kind)`, `az_part_shed_request(w, slot,
  index)` (the part's matter must be 0 by then), `az_memory_request(w, slot,
  words)`. `az_store_commit` applies the requests of the living at the end
  of the tick in uid order, so a full store refuses the same requests for
  every thread count and slot order.

### 5.2 Genome, brain, body, memory

**Genome.** Two haplotypes of any length. A gene is `{ u16 fam; u8 allele;
u8 expr; }`. Families below `AZ_CORE_FAMS` (80) are the named loci of the
genetic code; families up to the world parameter `FAMILY_SPACE` are latent:
each adds to one trait chosen by a hash of the world's code key. The value
of a family in a haplotype is clamp(128 + sum over its copies of (allele -
128) * expr / 128, 0, 255): copies add (dosage), expression 0 silences, a
haplotype without a copy has no value. A trait whose first family is in
neither haplotype is 0: no gene, no organ. Per birth the genome can gain a
copy of a gene, lose a gene, gain a copy of a block of 2 to 8 genes, lose a
block, gain a new gene of any family, or change a gene's family; expression
drifts; an exchange between haplotypes of different length is unequal and
changes the number of genes. The rates are the mother's own heritable traits
(`GENE_DUP`, `GENE_DEL`, `GENE_NEW`, `MUT_RATE`) times world parameters.
Modules read traits exactly as before: `w->trait[slot * AZ_TRAITS + t]`
(u8, the WARM cache of what the genome expresses) and the 16 `phen`
scalars; only az_genome.c reads genes.

**Brain.** `AzBrain` (64-byte header) plus biases, tile lists and int8
weights: `n_hb` blocks of 8 hidden units (0 is legal), `AZ_MOT / 8` motor
blocks, and tiles of 32 weights that join one input group of 4 to one block
of 8. The step walks the creature's own tiles (`w->k.brain_step`:
`az_brain_step_c` is the definition, `_avx2` and `_vnni` are bit-identical
twins; tests/test_brain.c holds all three to a 64-bit oracle). A child's
brain is the mother's with point changes, tiles added and removed, and
blocks of 8 neurons added and removed, at the mother's own heritable rates
(`NEURO_MUT`, `NEURO_GROW`, `NEURO_PRUNE`) times `NEURO_RATE`. `gain[8]` and
`bias[8]` in the header are what Tier 1 and a teach pair adapt in life.
A lineage (`AzClade`, 64 bytes) is now only a record: founder, parent,
birth tick, members.

**Body.** Any number of parts, each with its own matter, inside the exact
matter balance (`az_total_matter` counts `az_body_matter`). A creature is
born with 1 + (SEGMENT >> 5) empty parts whose kinds follow its traits; it
fills them by eating and grows more in life. What one part can hold is the
world parameter `PART_HOLD`; no 16-bit field limits a mass. Kinds:
`AZ_PART_CORE`, `GUT`, `FIN`, `ARMOR`, `WEAPON`, `STORE`, `SENSOR`,
`GONAD`. az_metab.c owns growth and resorption; a dead body's parts go to
its cell with its pools (az_life.c, `kill`).

**Memory.** `az_memory(w, slot, &words)`: 64-bit words, none at birth, any
number in life, not inherited. The engine gives them no meaning; az_tier1.c
does (patterns for tk_hop beyond the four of `engram`, or anything it
learns). Memory costs what the module makes it cost: charge for it in
matter or energy, so that a larger memory is a trade and not a gift.

### 5.3 World parameters (everything that is not a gene)

A world parameter is read as `w->param[AZ_WP_<name>]`, set before worldgen
(`az_param_set`; `az_run --param name=value,...`), hashed and saved. The
engine's own:

| Parameter | Default | Range | What |
|---|---|---|---|
| `FAMILY_SPACE` | 4,096 | 256 to 65,536 | gene families that exist in this world: a new gene takes a family number below this |
| `FOUNDER_GENE_LOSS` | 0 | 0 to 128 | chance in 256 that a founder species lacks a core gene family (marker families are never lost) |
| `GENE_DUP_RATE` | 48 | 0 to 4,096 | scale of gene and block duplication, with the mother's GENE_DUP trait |
| `GENE_DEL_RATE` | 48 | 0 to 4,096 | scale of gene and block deletion, with the mother's GENE_DEL trait |
| `GENE_NEW_RATE` | 16 | 0 to 4,096 | scale of the birth of new genes (insertion, family drift), with the mother's GENE_NEW trait |
| `EXPR_MUT_RATE` | 64 | 0 to 4,096 | scale of changes to a gene's expression level |
| `BRAIN_FOUND_HID` | 4 | 1 to 256 | hidden blocks (of 8 neurons) in a founder's brain |
| `BRAIN_DENSITY` | 96 | 1 to 256 | chance in 256 that a founder brain has each possible tile of connections; 256 = fully connected |
| `NEURO_RATE` | 64 | 0 to 4,096 | scale of neural mutation at birth (weights, connections, neurons), with the mother's NEURO traits |
| `CELL_HOLD` | 1,073,741,824 | 65,536 to 1,073,741,824 | most matter (mu) or energy (eu) one body or one colony pool holds: a body's remains must fit the books of its cell |
| `PART_HOLD` | 4,096 | 16 to 1,073,741,824 | matter (mu) one body part can hold per unit of the SIZE trait's scale; capacity grows with parts, not with a constant |

Each wave-2 module has a file of its own for its parameters:
`core/params/az_<module>.def`, same `AZ_PARAM` form, names starting with the
module's name in capitals (`METAB_...`). The owner of `az_<module>.c` edits
that file and no other `.def` file. The table has room for 256 parameters.
A new parameter changes the reference hashes (parameters are state): say so
in your report.

The alphabet of this world, fixed when it is compiled, is not a size of any
creature: the 64 sense channels and 24 motor channels the physics offers
(`AZ_SENS`, `AZ_MOT`; a brain uses the ones it has tiles for), the 112 named
traits, the 8 kinds of body part, the chemistry's species, the roles of a
colony, the 4 inventory resources and `AZ_VEL_MAX`. The brain twins and the
snapshot rely on them. Changing one is a change of ABI, with a notice.

### 5.4 The 16 new traits, senses and motors (112 traits, 64 senses, 24 motors)

| Trait | Owner | What it carries |
|---|---|---|
| `AZ_T_SEGMENT` | genome | parts at birth (az_develop_body); life: readiness to grow more |
| `AZ_T_ALLOMETRY` | metab | how intake is shared among the parts |
| `AZ_T_DEV_PLAST` | life | how far the birth place bends development |
| `AZ_T_GENE_DUP` | genome | rate of gene and block duplication |
| `AZ_T_GENE_DEL` | genome | rate of gene and block loss |
| `AZ_T_GENE_NEW` | genome | rate of new genes and of family change |
| `AZ_T_NEURO_MUT` | brain | rate of weight and bias change |
| `AZ_T_NEURO_GROW` | brain | rate of new tiles and new neurons |
| `AZ_T_NEURO_PRUNE` | brain | rate of lost tiles and lost neurons |
| `AZ_T_TAXIS` | sense | gain of the gradient channels |
| `AZ_T_CURVE` | sense | shape of the sensory response curve |
| `AZ_T_IMPROV` | tier1 | own noise let into actions (with AZ_M_IMPROV) |
| `AZ_T_STAKE` | social | what it risks in a game (with AZ_M_STAKE) |
| `AZ_T_SIG_AIM` | social | how directed its signals are |
| `AZ_T_CASTE` | colony | which role it leans to |
| `AZ_T_COMMONS` | econ | share of gains given to the colony's pools |

New senses: `AZ_S_GRAD_T`, `LIGHT_AHEAD`, `GRAV_AHEAD`, `FLOW_AHEAD` (the
gradient or vector of the cell projected on the way the creature faces; 64 =
nothing either way), `SIGA_AHEAD`, `SIGA_SIDE`, `SIGB_AHEAD`, `SIGB_SIDE`
(where the signals come from: `AzPercept.sigv_a`, `sigv_b`), `COLONY`,
`COLONY_NEED`, `MASS`, `PARTS`, `SEASON`, `STORM`, `PAYOFF` (what the last
interaction gained or lost: 64 = nothing), `STAKE` (what the neighbour it
faces has staked). New motors: `AZ_M_SIG_AIM`, `IMPROV`, `STAKE`, `GIVE`,
`GROW`, `SHED`, `COLONY_SIG`, `BUILD`. New cell fields: `grav` (worldgen)
and `lvec` (weather), i8 x 4 vectors; `AzCellSense` carries `temp_grad`,
`light_grad`, `grav`, `lvec`; `AzPub.face` is the facing vector.

What the owner's words ask of a module that uses them:

- **Spectrums, curved and linear.** A trait acts through a graded response,
  never a switch at a fixed threshold: linear, or bent by the creature's own
  `AZ_T_CURVE`. No table of cases.
- **Game theory.** A strategy is heritable (`AZ_T_STAKE`, `AGGR`, `ALTRUISM`,
  `COMMONS`, ...) and adjustable in life by the brain (`AZ_M_STAKE`,
  `AZ_M_GIVE`). A payoff is conserved matter or energy that a pair resolve
  moves; there is no payoff matrix in the code. The outcome is fed back to
  the creature (`AZ_S_PAYOFF`).
- **Gradients and vectors.** Worlds have fields with direction; senses
  project them on the creature's own axes; actions have a direction.
- **Levels.** Body parts, the creature, the colony (5.5), the lineage
  (`AzClade`), the ecosystem (cells and `gstate`): state lives at the level
  it belongs to, and no level is tied to a fixed count.
- **Improvised.** Behaviour comes from the creature's own brain, its noise
  sense and `AZ_M_IMPROV` with `AZ_T_IMPROV`; never from a script of cases.

### 5.5 The colony is an organism

`AzColony` (160 bytes) has its own energy pool (`i64 energy`, inside the
energy balance), its matter stores as before, caste counts (`caste[]`, by
Tier 2), four shared signal channels (`signal[4]`) and no limit on members.
The engine owns the plumbing, so the books hold under any colony module:

| Phase | Call | Meaning |
|---|---|---|
| 1 | `az_colony_ask_energy(w, colony, eu)` | add to the demand on the pool |
| 3 | `az_colony_take_energy(w, colony, eu)` | returns floor(eu * grant / 65535); the member adds it to its own energy; ask in phase 1 for what you take |
| 3 | `az_colony_give_energy(w, colony, eu)` | the member has already taken eu out of its own energy |
| 3 | `az_colony_say(w, colony, channel, v)` | one voice, 0..255; next tick's `signal[channel]` is the mean of the voices; a silent channel fades by 1/8 per tick |

A pool never gives what it does not hold. tests/test_protocol.c runs this
with real traffic on 8 and 24 workers and across a snapshot.

### 5.6 Two modes: wild is the default

`AzConfig.mode`: `AZ_MODE_WILD` (0, what a zeroed configuration gives) or
`AZ_MODE_LAB` (1).

- **Wild** is a living run. The world is made of fresh entropy from the host
  (`AzHost.entropy`; Windows: the system generator, stirred with the
  processor's counters) and draws 16 more bytes at the start of every tick,
  which go into every draw of that tick. No seed is read, no seed decides
  the run, and no two runs are the same. Without a host that gives entropy
  `az_world_create` returns NULL (also for `az_snapshot_load` of a wild
  snapshot). A tick the source had nothing for is counted
  (`totals.entropy_missed`), is one ledger event (`AZ_REF_ENTROPY`), and the
  run goes on. Matter and energy are exact every tick, as in lab mode.
- **Lab** is the seeded, repeatable test bench: the test suite
  (`az_test_config`), `az_bench`, `az_budget`, `az_lesion`, the reference
  hashes and the determinism matrices. A lab world never asks for entropy.
  `az_run` and `az_snap` (the runner and the picture) are wild unless `--lab`
  is given; `--seed` needs `--lab` (ctest `cli_snap_modes` holds both modes
  of both tools).
- **The journal** (`AzHost.entropy_log`, `az_run --entropy-journal FILE`:
  24 bytes per tick, the tick and the 16 bytes) is a record for looking into
  a surprising event afterwards. Nothing reads it back into a run.

For a module nothing changes in the code: draw with `az_rand` and
`az_rand4` only (they carry the tick's entropy in a wild world and are a
function of the seed in a lab world). Never keep a generator of your own,
never ask the operating system, never branch on the mode. Your tests run in
lab mode; tests/test_wild.c holds the wild properties for the whole engine.

### 5.7 Measured cost (8 workers on P cores, a lightly loaded machine)

Medians of interleaved rounds (build\work\scripts\ab_time.ps1; rows in
build\work\m4\logs\ab_*.csv). "Before" is the first freeze of ABI 4 (fixed
loci, one dense brain per lineage); "after" is this section.

| World | Before, ms per tick (perceive) | After, ms per tick (perceive) |
|---|---|---|
| 100,000 founders, 60 ticks, C reference | 14.51 (10.63) | 19.24 (11.20) |
| 100,000 founders, 60 ticks, assembly twins | 9.11 (5.45) | 16.77 (9.13) |
| 1,000,000 founders, 20 ticks, assembly twins | 72.42 (42.76) | 140.99 (81.17) |

Where it goes at 100,000 with the twins: neighbour scan, senses and intents
about 4.7 ms; touching each creature's own block about 1.2 ms; the brain
step about 3.1 ms (every creature reads about 1.9 KB of its own, where a
lineage's shared brain used to sit in cache); phase 3 0.6 -> 1.8 ms (body
parts); the birth commit 0.7 -> 2.3 ms (a genome, a brain and a body are
built per birth); packing the store 18 ms each time at 100,000 and about
250 ms at 1,000,000 (every 16 ticks by default, `--store-period`). Ordering
the block by use and one prefetch took the tick from 21.2 to 19.2 ms. What
could buy the rest back is in `docs/TRIGGER-REGISTER.md` (TR-14 to TR-17).

Memory: the store is two halves; the default is 64 units (4 KiB) per slot of
capacity in each half (`--store-units`). 1,000,000 creatures use 2.64 GB of
live blocks; the measured run locked a 7.8 GB arena.

### 5.8 Order of wave 2 (the owner's)

life, mate, tier1, metab (evolution itself); then social, econ, colony
(game theory and levels); then weather, chem, worldgen (gradients); sense,
physics and stats where their first reader needs them. If the money does not
reach a module, the final report says so by name.
 
## 6. Isolated P1 sensory candidate (revision 2)

The development build appends four bytes of AzSenseMemory to each creature's MIND segment. Genomic development sets TAXIS and CURVE after each founder or offspring brain has its final layout. Social intent records free energy plus the nominal value of matter at risk under the current commitment and stake strategy in seven-bit log units. The next grid publishes that previous declared stake in the existing 32-byte public record. The neighbor scan retains the nearest neighbor in the open forward half-space, with the scan's existing deterministic tie order. The resolving actor records the signed change in free energy plus retained matter valued at AZ_EPS_BIO per unit during its own matched-pair action, clamped to -64 through +63. Neutral payoff persists until the first interaction.

Snapshot format 5 identifies the new MIND payload and the two appended parameter descriptors. Descriptor ids 0 through 232 retain their historical meanings. Format 4 worlds continue to load in the archived format 4 engine. The format 5 reader rejects format 4 before allocating restore state. TAXIS uses lesion bit 21 and disables directional field gain. No new per-entity SoA column is allocated.

## 7. Isolated P1 sensing revision 3

Each MIND record has a fifth byte for innate STAKE. Development fills the TAXIS, CURVE and STAKE caches when building the organism. Common offer publication reads the owner's cache and motor. Event-time combat retains the original trait semantics. Sensor-part health and matter increase directional social acuity independently of the TAXIS field-gradient lesion. Signed payoff encoding is saturated for every stored byte value.

The field system publishes hydrostatic pressure to a four-byte scratch value at each cell after world creation, world load and each completed tick. It integrates the physical column with the same fixed-point conversion used by the point reference. A receptor observes its local published pressure. The species scan in az_cell_pressure remains available to the physical model. Direct test or tool writers that change settled species manually publish the derived field at their transaction boundary.

The writer emits snapshot format 6. The reader accepts format 5 by consuming a single verified alignment byte after the old four-byte MIND payload and recovering the innate STAKE value from the stored developed trait. Segment starts, block size and existing fields are preserved. Format 4 belongs to the prior archived engine. A format 5 migrated into the revised engine follows the revised sensory dynamics.

## 8. Pending cell transfers and snapshot format 8

The accumulator retains its 64-byte layout. The signed heat delta at byte 32 and the unsigned word at byte 52 together encode a signed 64-bit amount. Each of the eight species has a signed low word in the accumulator and a high word in a separate 32-byte per-cell column. Each signed low-word crossing adjusts the high word atomically. Readers inspect both after the producing workers join.

At commit, a funded positive amount fills available field storage and any excess stays booked at the same cell. Later withdrawals free storage. An unfunded negative withdrawal leaves the debt and the field intact and increments the field error counter. Matter and energy audits include pending amounts. A nonzero high word contributes to the state hash even when its low word is zero.

Format 7 uses the former zero padding at byte 52 for the heat high word. Format 8 also saves nonzero species high words in a sparse section. Its reader accepts formats 5, 6 and 7, treating an absent species high word as zero and checking the legacy heat padding. Migration preserves the state actually encoded by each writer. Amounts an older engine had already lost to arithmetic wrap cannot be inferred. Original format 4 worlds use their archived engine under OD-19.

## 9. Concurrent marks and capped colony storage

During one commit interval the pending strength of each field mark saturates at UINT32_MAX. A later post cannot cause the pending sum to wrap. The field still uses its registered per-channel byte representation after commit.

AzColonyEph is 192 bytes. Per-resource demand, deposit and withdrawal totals use 64-bit integers. The colony commit enforces CELL_HOLD for each resource and for energy. A funded surplus is posted to the field at the colony using the spill path, and each affected destination increments its refusal counter. The founding endowment applies the same rule. An unfunded debit remains pending and records an accounting error. Grants use available capped stock.

Snapshot format 8 remains in use. Historical over-cap stock survives the initial load and is settled at the next ordinary commit. In the tested historical founder case the repaired and previously over-cap saves converged to byte-identical files after that tick. See build/work/w2/rva-codex-01/p1-colony-pantry-fix/REVIEW.md and canonical-p1-integrated/review-skeptic-founder/REVIEW.md under that work area.

## 10. Native physics, unsigned deposits and snapshot format 9

This private integration keeps ABI major version 4 and writes snapshot format 9.
Rebuild native consumers against the matching header. The main tick passes
the healthy-fin fraction from az_sense_and_fin to az_physics_steer_fin after
one traversal of the creature's parts. Existing callers can still use
az_sense and az_physics_steer. Phase 3 prices the powered acceleration using
energy remaining after metabolism and transfers payment to heat in the cell
where the creature started that tick.

travel_carry is an appended HOT u32 column. It accumulates actual travel until
the creature's UID-specific statistics phase. Reorder moves it with that UID.
It participates in hashes and format-9 saves. phys_passive (three i16s) and
phys_quote (one u16) hold the phase-1 contact force and quote. They are
ephemeral. At capacity 1,004,096 the measured increase in the arena size was
12,073,792 bytes. The total includes alignment and world structure changes.

The configured parameters append PHYS_BUOY_SHIFT, PHYS_FIN_GAIN,
PHYS_CONTACT_GAIN, PHYS_THRUST_COST and PHYS_ANG_RETAIN at IDs 235 through
239. Defaults are 4, 192, 128, 4 and 192. Sixteen parameter words remain
reserved. AZ_LESION_THRUST (bit 22) suppresses powered translation, retaining
passive contact, drag, buoyancy and wind. AZ_LESION_GRAVITY suppresses the
local body density response to gravity.

Readers support formats 5 through 9. In the legacy formats the reader
validates the original 235 parameters, preserves their values, applies the
five new defaults and starts travel carry at zero. A real physics build
relocates a covered living center before sensing, with deterministic search
and a rebuilt grid. A wholly sealed world is refused without consuming the
caller's arena. The reference physics build preserves positions when reading
its own format-9 saves on terrain with at least one open cell.

az_cell_deposit takes one unsigned 32-bit matter store and posts bounded
positive increments through the signed species journal, all at the same cell.
Life uses it for each unspent provision and for each death carrier. A birth
divides energy by the full positive clutch count. The normal cached mass of a
newborn saturates at UINT32_MAX while its individual provision pools retain
their exact amounts. The transfer changes no column or on-disk field.

## Existing demo save inventory at format-9 transfer

The shared build/demo/worlds directory contained zero save files when checked on 2026-11-19. Its path was present. The separate format-8 migration fixture exercised high-word stocks and matched five future tick hashes after conversion. Historical viewer sessions use independent save locations. The migration receipt is in build/work/w2/rva-codex-01/p2-selected-transfer/migration.json.
