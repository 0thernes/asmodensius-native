If your progress file `Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native\build\work\w2\worldgen\PROGRESS.md` exists, read it first and continue from its next step; never redo a step it records as done and tested.

**LANDED 2026-11-18:** `core/az_worldgen.c` replaced the stub as wave 2 module 10 (46 WORLDGEN_* parameters in `core/params/az_worldgen.def`; ctest 46 of 46, test_worldgen 4034 checks, planted defects 31 of 31 caught; new lab references 73f1e9e013f8db50 at 100k and 5d9c02d2c7dd5fb1 at 1M in `tests/golden_hashes.txt`); record and limits in `build/work/PROGRESS.md` ("wave 2, module 10, worldgen landed") and `build/work/w2/worldgen/PROGRESS.md`.

# Brief: az_worldgen.c (the alien world and its founder species)

**Amended 2026-11-18 (ABI-V4-NOTICE section 5, README rules 13 to 20; they win over the text below).** You write the new vector field `grav` (i8 x 4). Founders differ in what they have (`FOUNDER_GENE_LOSS`, `BRAIN_FOUND_HID`, `BRAIN_DENSITY` are world parameters); `az_found` builds a founder's block and can be refused for want of store room.

You own `core/az_worldgen.c` and `tests/test_worldgen.c`. Read `README.md` in
this folder first. Lands first.

## What it must do

`az_worldgen(w)` runs once, serially, at tick 0. Everything is drawn with
`az_rand*` on stream `AZ_RS_WORLDGEN` (id = cell index, column index or
founder uid), so a seed gives one world.

1. **Terrain.** A height field over (x, y) from a few octaves of value noise.
   Below the height `c_solid = 255` (bedrock); the surface cell gets a
   fraction; above it 0. At least one layer of open cells everywhere.
2. **Layers.** By height above the terrain: a solvent ocean (liquid medium)
   up to sea level, about 60 percent of the open height, then carrier gas.
   Amounts per cell by layer: SOLV, GAS, OXI (dissolved and in the gas),
   sparse ORG_Y on the sea floor, NUT_X around vents, a few TOX_Z hot spots,
   a little STR_S on reef sites. Nothing in bedrock cells.
3. **Heat.** So that `az_temp_of` gives about 8192 at mid depth, warmer near
   vents and at the lit surface, colder in the deep and at the poles of y.
4. **Vents.** `vent_count` (up to 64) open cells on the sea floor in
   `vent_cell[]`, and `totals.reservoir_mu`: the buried matter the vents will
   re-emit (weather does the emitting).
5. **Suns and gravity.** `sun_period[2]` (two incommensurate days) and
   `gravity`.
6. **The genetic code**: `az_genetic_code_init(w)` before any founder.
7. **Derived columns.** Call `az_weather_global(w)` once, then
   `az_weather_derive` for every cell from the top layer down, then
   `az_chem_derive` for every cell.
8. **Founder species.** `cfg.n_species` species (1 to 100; 0 means 1), each
   its own clade (`az_clade_found(w, seed, k)`) and its own niche: a diet
   guild (producer: LIGHT and W; chemotroph: X; grazer and decomposer: Y;
   scavenger: W and Y; predator: Y with AGGR, WEAPON, PREDATION), a habitat
   (depth band, temperature optimum of that band, body density matching its
   medium), a mating mode (all eight modes occur across the species), a size
   class, a social style (solitary to colonial), a life history (egg or live
   birth, clutch against provision). Express the niche as trait targets and
   weights, fit ONE prototype genome per species with `az_genome_fit`, copy
   it to every founder of the species and vary a few loci per founder with
   your own draws, then `az_develop`.
9. **Founders.** `cfg.n_initial` in all, split over the species, placed in
   patches inside their habitat (never in bedrock), adults (`AZ_F_ALIVE`),
   with energy, pools and health for a fair start; `lineage` = species,
   `clades[cl].refcount` counted, `az_place` called.

## Contracts others rely on

- No cell has an amount that makes `az_cell_pressure` or `az_temp_of`
  overflow; totals stay far below 2^63.
- `gravity > 0`, `vent_count > 0`, both sun periods >= 256 ticks.
- The matter and energy totals after worldgen are what the invariants are
  measured against: nothing is created later except through the tallies.

## Tests (tests/test_worldgen.c)

Same seed, same hash; different seed, different world. Every founder alive,
in an open cell, with a developed genome; species counts add up; each of the
eight mating modes and each guild has at least one species when
`n_species >= 16`. Layer order holds in every column (bedrock under liquid
under gas). 200 ticks of the full tick keep both invariants exact. The
prototype fit reaches its targets within a stated error.

Reference for the niche layout: guild structure of chemosynthetic and
photic-zone communities (standard ecology); value noise (Perlin 1985).
