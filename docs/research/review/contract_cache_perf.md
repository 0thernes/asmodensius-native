# Contract review: cache behaviour and speed

Reviewer key: cache-perf. Date: 2026-11-17. Reviewed: `docs/ARCHITECTURE.md`,
`core/az_abi.h`, `core/az_columns.def`, `core/az_cells.def`, `core/az_world.c`,
`core/az_tick.c`, `core/stubs/*`, `platform/win/az_host_win.c`.
Nothing under `core`, `platform`, `tools`, `tests` was edited. Experiments are
in `S:/review/cache-perf/exp/` and use a copy of the tree in
`S:/review/cache-perf/tree/` (built with `cmake -S . -B b -G Ninja`, last line
`[30/31] Linking C executable test_core.exe`). All timings: **noisy, parallel
load** (other agents were running); I report best-of-N and ratios, and say
where the noise is too large to conclude.

P cores are logical processors 0 1 10 11 12 13 22 23, E cores are 2..9 and
14..21 (`topo.exe`, which calls `az_win_topology`).

## 1. Bytes per entity, per block, per worker

Tier sizes from `az_columns.def` (added by hand, checked against the static
assert): HOT 128 B in 32 columns, WARM 358 B in 23 columns, EPH 76 B in 11
columns; plus `pub` 16 B and `order` 4 B per entity. 66 columns in total
(`layout.exe`). Per cell (`az_cells.def`): FIELD 52 B, EPH 92 B, plus
`cell_start` 4 B = 148 B.

What each stage touches per entity (from the stubs and the contract text):

| Stage | Read (B) | Written (B) | Columns (streams) | Per 256 block |
|---|---|---|---|---|
| 0 reorder (every `reorder_period`) | 486 gather + 486 copy back | 486 + 486 | 55, one at a time | 124 KB moved twice |
| 0 grid build | flags 1, pos 12, uid 4, cell 4, order 4, vel 6, mass 2, tag 2, colony 2, sig 2 = 39 (+ uid again in the sort) | acc_by 4, claim 8, req 1, act 1, tgt 4, cell 4, flags 1, order 4, spos 4, pub 16 = 47 | pass 1: 5 read + 7 write; pass 3: 10 read + 2 write | 22 KB |
| 1 perceive (own data) | pub 16, order 4, phen 16, energy 4, health 2, uid 4, arousal 1, clade 2, hid 32, mod_gain 8, mod_bias 8, mass 2, flags 1, cell 4 = 104 | hid 32, motor 16, offer 16, act 1, tgt 4, eat 12, acc 6 = 87 | 20 own columns + 9 `pub` runs + 9 `cell_start` runs + `order` | 41 KB own |
| 1 perceive (shared) | neighbours' `pub`: 21.2 records = 339 B (default world), 26.5 = 424 B (1M on 1M cells) (`batch.exe`); clade tile 2752 B + 1 line | `c_dem` 4 (lock add) | | 87..109 KB of `pub` re-read, largely overlapping |
| 2a claims | flags 1, act 1, tgt 4 (+ target flags 1, uid 4, claim 8) | act 1, claim 8 (CAS on target) | 3 + 3 random | 1.5 KB |
| 2b settle | act 1, tgt 4, uid 4, claim[r] 8 | acc_by[r] 4, act 1 | 4 + 2 random | 1.3 KB |
| 3 act (stubs) | flags 1, act 1, tgt/acc_by 8, cell 4, phen 16, eat 12, pools 10, health 2, energy 4, acc 6, vel 6, pos 12, uid 4, req 1 = 87 | pools 6, mass 2, health 2, energy 4, vel 6, pos 12, req 1 = 33; cell `grant` 12 read, `delta` 32 + `dheat` 4 lock add | 24 columns + 3 cell tables | 23 KB + cell lines |
| 4 fields (per cell) | begin: -, grant: dem 24, commit x2: 56 each | begin: 36, grant: 12, commit: where non-zero | 2 + 2 + 4 | n/a |
| 6 tier 1 | uid 4, flags 1 swept; a visited entity (1 in K1) reads about 62 B WARM + 20 B HOT | same | 2 swept + ~10 random | |

Whole-tick set per entity that the contract forces every tick: HOT 128 + EPH
76 + pub 16 + order 4 = **224 B**. A 256-entity block: HOT alone 32,768 B;
HOT + EPH 52,224 B; with pub and order 57,344 B. Real modules add WARM reads
(finding 6), so 224 B is a lower bound.

Per worker, 8 P workers, entity state only (224 B each):

| Population | Per worker | vs 3 MB P-core L2 |
|---|---|---|
| 100,000 | 12,500 entities = 2.67 MB | 0.9x (full; no room for cells, pub neighbours, clades) |
| 1,000,000 | 125,000 entities = 26.7 MB | 8.9x |

With 24 workers (8 P + 16 E): 100,000 gives 4,167 entities = 0.89 MB each; an
E cluster's four workers then need 3.6 MB of a shared 4 MB L2. At 1,000,000:
8.9 MB each, 3x a P-core L2 and 8.9x an E cluster's per-core share.

Cell tables at the az_run default grid (131,072 cells): 148 B x 131,072 = 18.5
MB. At a 2^20-cell grid: 148 MB.

## 2. Where the hot working set stops fitting (plain statement)

Hot set = 224 B x N + 148 B x cells + 2.8 KB x live clades. MB = 2^20 bytes.

- **L2, P cores only (8 x 3 MB = 24 MB):** 24 MB / 224 B = **112,000 entities**
  if nothing else were in L2; with cells at the default density (1.31 cells
  per entity: 224 + 194 = 418 B per entity) 24 MB / 418 B = **60,000**.
- **All L2 (40 MB):** 187,000 entities (entity state only); 100,000 with cells.
  The az_run default world (100,000 entities, 131,072 cells, 100 clades) is
  21.4 + 18.5 + 0.3 = 40.2 MB: it sits exactly at the total-L2 limit.
- **L2 + L3, P cores (24 + 36 = 60 MB):** 281,000 entities (entity state
  only); **150,000** with cells.
- **All cache (76 MB):** 356,000 entities (entity state only); **190,000** with
  cells.
- **1,000,000 entities on a 2^20-cell grid:** 214 + 148 = 362 MB, **4.8x all
  the cache in the machine**. Every full sweep of the population is a DRAM
  stream. The tick makes about ten such sweeps (grid x2, field begin,
  perceive, claims, settle, grant, act, commit x2, step, tier 1).

So: above roughly 100,000 to 190,000 entities the design is a streaming
design. "Cache-resident" then means "each 256-slot block stays in L1/L2 while
the four phase-3 modules run over it", which `job_act` does achieve, and
"sequential streams the prefetcher can follow", which holds if finding 2 is
fixed. Measured: 20 concurrent column streams over 1M entities ran at the same
ns per entity as the L1-resident case when the columns were not aliased
(`alias4.exe`: 4.28 ns at 1M vs 4.17 ns at 16K entities).

AZ_CHUNK = 256: HOT + EPH of a block is 52 KB, above the P core's 48 KB first
level and the E core's 32 KB L1D; it fits the P core's 192 KB level. AZ_CHUNK =
128 gives 26 KB, which fits both. Recommended: 128 (arithmetic only; the
end-to-end run is too noisy to measure a block-size difference today).

AZ_SCAN_BATCH = 32 is never reached at the densities the contract implies
(finding 7): the mean batch is 1.43 to 1.55.

## 3. Findings

### F1 (critical) Per-entity atomic adds into one global `w->totals` line

Rule C4 says: "External flows are booked in `w->totals` with atomic adds: light
absorbed and geothermal input into `energy_in`". Light is absorbed by every
phototroph every tick, so every worker does a `lock add` on the same cache
line once per phototroph per tick.

```
$ S:/review/cache-perf/exp/atomics.exe 1.0      (gcc -std=c2x -O2 -march=x86-64-v3)
B: 1 threads (LP0..0): shared lock add   4.05 ns/add ; per-worker plain add   1.29 ns/add ; ratio 3.1x
B: 2 threads (LP0..1): shared lock add  12.34 ns/add ; per-worker plain add   1.39 ns/add ; ratio 8.9x
B: 4 threads (LP0..3): shared lock add  53.11 ns/add ; per-worker plain add   1.29 ns/add ; ratio 41.1x
B: 8 threads (LP0..7): shared lock add 176.75 ns/add ; per-worker plain add   1.40 ns/add ; ratio 126.3x
```

(LP0..7 is 2 P + 6 E cores. Back-to-back adds are the worst case; in the real
loop each add still costs one cross-core line transfer.) 177 ns per phototroph
is the same size as the whole per-entity budget.

The same lines also hold read-mostly pointers (`layout.exe`):
`totals.energy_in` is at AzWorld offset 816 on line 12 with `clades` (776) and
`c_dmark_id` (768); `k.brain_step` (944) is on line 14 with
`totals.field_underflow` (928) and `interactions[5..8]`.

Change:
- `AzWorker` gets `AzTotals t;` and is declared `AZ_ALIGNED(64)` (today
  `sizeof(AzWorker)` is 168, not a multiple of 64).
- C4 becomes: "External flows and counters are booked with plain adds in
  `w->workers[worker].t`. `az_tick` folds every worker's `t` into `w->totals`
  serially after phase 3 and after phase 4. No parallel job writes
  `w->totals`."
- `AzTotals totals` in `AzWorld` gets `AZ_ALIGNED(64)` and is moved after
  `k` and `host`.
- While there: allocate birth and event queues for `n_workers`, not
  `AZ_MAX_WORKERS`. `layout.exe`: at cap 1,504,128 the birth queues are 551.2
  MB of a 1755.3 MB locked arena, and 56 of the 64 queues can never be used
  with 8 workers.

### F2 (major) With a power-of-two `cap`, every column base has the same address mod 4096

`az_world_create` allocates the columns back to back, each `pad64(cap * per *
sizeof)`. When `cap` is a multiple of 2048 every size is a multiple of 4096.

```
$ S:/review/cache-perf/exp/layout.exe
cap 1048576 (arena 1278.3 MB): column base mod 4096: pos_x=3968 pos_y=3968 pos_z=3968 vel_x=3968 ...
  66 entity columns; largest group sharing one L1 set at slot 0: 66
cap 1504128 (arena 1755.3 MB): column base mod 4096: pos_x=3968 pos_y=3456 pos_z=2944 vel_x=2432 ...
  66 entity columns; largest group sharing one L1 set at slot 0: 7
cap 131072 (arena 188.1 MB): ... largest group sharing one L1 set at slot 0: 66
```

The HOT tier has 20 `u16` columns. Slot s of all 20 then maps to the same L1
set (12 ways on a P core, 8 on an E core): a 256-slot block needs 20 x 8 = 160
lines in 8 sets that hold 96.

Microbenchmark (`alias4.c`: 20 u16 columns, 256-slot blocks, four passes per
block as `job_act` does, column spacing a power of two versus the same spacing
plus 192 B per column; ns per entity per pass, best of 9, checksums equal):

```
--- P core LP0, 20 cols, stagger 192
     16384          4.597          4.170     1.10
    131072         14.532          4.282     3.39
   1048576         41.816          4.281     9.77
--- E core LP4, 20 cols, stagger 192
    131072         12.830          5.706     2.25
   1048576         32.480          5.819     5.58
--- stride 2097152+2176 (cap 1049664)
   1048576          4.157          4.068     1.02
```

Real engine, stubs, 700,000 entities, 8 threads, `act` stage in ms per tick,
five alternating pairs (`N:/build/release/az_run.exe --n 700000 --cap <cap>
--ticks 12 --threads 8 --quiet`):

```
cap 1048576: act = 7.122 20.234 7.532 12.662 9.932   (min 7.12, median 9.93)
cap 1049664: act = 5.253  6.748 6.000  5.454 4.570   (min 4.57, median 5.45)
```

Slower in 5 of 5 pairs, 1.6x on the minimum, although the stubs touch only 10
of the 20 u16 columns. Mechanism above L1 is not proven (a 64 B stagger and a
4096 B stagger both remove it; probable cause is set conflicts in L2/L3 too,
because Windows colours physical pages by virtual address).

Change, in `az_world_create` and `az_world_bytes` (no ABI change): give the
k-th column (entity and cell) the base `align4096(arena) + ((k * 11) & 63) *
64` and reserve `+ 8192` instead of `+ 64` per column. Add to the header of
`az_columns.def`: "Column bases are deliberately staggered; assume 64-byte
alignment and nothing more."

### F3 (major) Every cell post is a `lock`-prefixed add, and the signatures leave no way out

`az_cell_post`, `az_cell_post_heat` and the phase-1 hooks (`az_metab_demand`,
`az_social_intent`, `az_physics_steer`) have no `worker` argument, so the only
legal implementation is one atomic per post. C3 makes every action post heat.

```
$ S:/review/cache-perf/exp/atomics.exe 1.0
A: 1048576 entities, 1.00 per cell, 5 posts each, one thread pinned to LP0 (noisy, parallel load; best of 9)
  lock add (contract today)       20.76 ns/entity  checksum fbd82d378c3743b8
  plain add (no lock)              4.72 ns/entity  checksum fbd82d378c3743b8
  combine per cell, then lock     12.90 ns/entity  checksum fbd82d378c3743b8
$ S:/review/cache-perf/exp/atomics.exe 0.76
  lock add 22.69   plain add 6.53   combine per cell, then lock 11.25   (same checksum)
```

About 3.2 ns per uncontended lock add: 16 ns per entity for 5 posts, 32 ns for
10. A one-entry per-worker combining cache already saves 8 to 11 ns per
entity at one entity per cell and more at higher density.

Change (signatures only; the bodies can stay as they are today):

```
AZ_INLINE void az_cell_post(AzWorld *w, u32 worker, u32 c, u32 species, i32 d_mu);
AZ_INLINE void az_cell_post_heat(AzWorld *w, u32 worker, u32 c, i32 d_eu);
AZ_INLINE void az_cell_demand(AzWorld *w, u32 worker, u32 c, u32 diet, u32 n);   /* new: no raw atomics on c_dem */
void az_metab_demand(AzWorld *w, u32 slot, u32 worker);
void az_social_intent(AzWorld *w, u32 slot, const AzPercept *p, u32 worker);
void az_physics_steer(AzWorld *w, u32 slot, const AzPercept *p, u32 worker);
```

Rule: "Modules never touch `c_dem`, `c_delta`, `c_dheat`, `c_dmark` directly."

### F4 (major) Ten fork-join barriers per tick with one static part per worker

`az_abi.h`: "run fn(w, splits[k], splits[k+1], k) for k in 0..n_parts-1".
`az_host_win.c`: `if (k < p->n_parts) p->fn(p->world, p->splits[k],
p->splits[k + 1], k);`. So the part index is the worker index, there are at
most `n_workers` parts, and each stage lasts as long as its slowest core.
`az_tick` calls `run_parts` ten times per tick.

Arithmetic: 24 equal parts finish in the time of the E cores. With the E/P
speed ratio 0.74 measured on the column loop of F2 (4.28 ns vs 5.82 ns per
entity): 24 x 0.74 = 17.8 P-equivalents instead of 8 + 16 x 0.74 = 19.8 (10
percent lost). With an assumed 0.55 for vector-heavy code: 13.2 instead of
16.8 (21 percent lost).

Measured (noisy, parallel load), `az_run --n 5000 --ticks 300 --quiet`:

```
threads 1: stages (ms/tick): ... match=2.059 act=0.727 field=3.675 life=0.996 tier1=0.000
threads 8: stages (ms/tick): ... match=0.403 act=0.194 field=0.538 life=8.036 tier1=1.571
threads 8: stages (ms/tick): ... match=0.923 act=0.385 field=0.580 life=8.185 tier1=3.404
```

`az_tier1_range` is an empty stub, yet its parallel region costs 1.6 to 3.4 ms
per tick (workers have gone to sleep during the serial life commit). At
300,000 entities the cheap stages were slower on 8 threads than on one
(`--n 300000 --ticks 12`): serial `match=2.205 act=8.075 field=3.904`; 8 P
threads `match=14.057 act=14.147 field=4.934`, `match=27.893 act=19.973
field=23.645`. Much of that is other agents preempting pinned workers, which
is exactly what a static split cannot absorb.

Change:
- `AzHost.parallel_for` comment becomes: "call fn(w, splits[k], splits[k+1],
  t) exactly once for every k in 0..n_parts-1; parts are claimed dynamically;
  t < n_workers is the index of the calling thread". `splits` holds
  `AZ_MAX_PARTS + 1 = 1025` entries; `split_even` and `split_cells_by_load`
  cut 8 parts per worker.
- Fewer regions: delete `az_field_begin` (F5); call `az_tier1_range(w, cb,
  ce, worker)` inside the `job_act` block loop after `az_life_range` (the
  block is still in L1, and one sweep of `uid`/`flags` over all slots
  disappears); run `job_settle` and `job_field_grant` in one region.
- Determinism is unaffected: births are sorted by (mother uid, n) and events
  by the ledger, so it does not matter which thread ran a part.

### F5 (major) The per-cell tables cost the same every tick whatever the population

Per cell per tick: `az_field_begin` zeroes 36 B; `az_field_grant` reads 24 B;
`az_field_commit` reads 56 B and runs twice. 36 + 24 + 112 = 172 B per cell
per tick = 172 MB at 2^20 cells, in 2 + 2 + 4 separate streams.

```
$ az_run.exe --n 5000 --grid 7,7,6 --ticks 40 --threads 1 --quiet
ticks 40 in 2.072 s = 19.3 ticks/s; 51.797 ms/tick; live 11648 ...
stages (ms/tick): reorder=0.394 grid=2.659 perceive=10.398 match=8.169 act=0.945 field=29.014 ...
$ az_run.exe --n 5000 --grid 7,7,6 --ticks 40 --threads 8 --quiet
ticks 40 in 1.236 s = 32.4 ticks/s; 30.908 ms/tick ... match=7.250 ... field=10.534 ...
```

31 to 52 ms per tick for 11,648 entities; on the default 131,072-cell grid the
same population costs `match=2.059 field=3.675` serial. That is about 40 ns
per cell per tick, serial.

Change:
- Remove `az_field_begin` from the ABI and the pipeline. `az_field_grant`
  zeroes `dem` after it has used it. Rule: "`grant[c][d]` is defined only for a
  channel on which demand was posted this tick."
- Replace the EPH columns `delta`, `dheat`, `dmark`, `dmark_id` by one record:
  `typedef struct AzCellAcc { i32 delta[8]; i32 dheat; u32 dmark[4]; u32
  dmark_id; u32 pad; } AzCellAcc;` with `_Static_assert(sizeof(AzCellAcc) ==
  64)` and `AZ_CELL(acc, AzCellAcc, 1, EPH)`. One line per cell: a matter post
  and a heat post for the same cell hit one line instead of two, and the
  commit reads one stream instead of four.
- Guidance in ARCHITECTURE section 3: "choose the grid so that there are 2 to
  4 entities per cell". For 1M entities a 2^18-cell grid has 37 MB of cell
  tables (the size of L3) instead of 148 MB. The price is 4x more `pub`
  candidates per scan, so this is a trade to measure, not a free win.
- `job_field_step` does `(x + y + z + w->tick) % stride` (a division) for
  every cell every tick; require `field_stride` to be a power of two and use a
  mask.

### F6 (major) The offer contract makes WARM columns hot, so "128 bytes per entity" is not the per-tick set

`az_columns.def`: "WARM - touched on events and by the phased Tier 1 pass", and
`rank`, `inv`, `role`, `bond` are WARM. `az_abi.h` requires, for every entity
every tick ("written in phase 1 by az_social_intent from the entity's own
start-of-tick state"): `AZ_O_RANK`, `AZ_O_WEALTH` ("log-scale energy +
inventory"), `AZ_O_GIVE_QTY` ("already capped by what the entity really
holds"), `AZ_OD_BONDED`, `AZ_OD_BREEDER`. Those can only come from `rank`,
`inv`, `bond`, `role`. That is four more columns and four more cache lines per
entity per tick, and nothing in the text stops modules from reading `trait`
(96 B) in the per-slot loops as well.

Arithmetic: forced per-tick set = HOT 128 + EPH 76 + pub 16 + order 4 = 224 B,
+ 12 B (rank 2, inv 8, role 1, bond 1) in 4 extra streams = 236 B. A 256 block
is 60 KB, not the "L1-sized" 32 KB the architecture table implies.

Change:
- Move `rank`, `inv`, `role`, `bond` to the HOT tier (new group "HOT / social:
  12 bytes") and change the assert to `AZ_HOT_BYTES == 140`.
- Add rule D7: "Phase-1 hooks and the per-slot loops of the phase-3 ranges
  read only HOT, EPH and `pub`. WARM columns are read in pair resolve, on
  lifecycle events and in Tier 1. If a per-tick loop needs a trait, it is
  one of the 16 `phen` scalars."
- ARCHITECTURE section 1: replace "The hot tier is exactly 128 bytes per
  entity" by "an entity costs about 236 bytes per tick (HOT 140, EPH 76, pub
  16, order 4)".
- `#define AZ_CHUNK 128` (26 to 30 KB per block; fits the 48 KB and 32 KB
  first-level caches).

### F7 (major) `az_scan_cell` is per cell, so the scan batch is 1.4 entities

```
$ S:/review/cache-perf/exp/batch.exe
n 100000, cells 131072 (0.76 per cell): occupied cells 69784, az_scan_cell calls 69784, mean batch 1.43 (AZ_SCAN_BATCH 32), max cell 8
  ... candidate AzPub records read per entity: 21.2 (339 bytes)
n 1000000, cells 1048576 (0.95 per cell): occupied cells 644397, az_scan_cell calls 644397, mean batch 1.55 (AZ_SCAN_BATCH 32), max cell 9
  ... candidate AzPub records read per entity: 26.5 (424 bytes)
```

The sense range is at most one cell and at most 64 neighbours are used, so
the contract itself implies a few entities per cell. A per-cell call cannot
share the 27-cell gather between cell c and cell c+1 (18 of the 27 cells are
the same), and the core forbids statics, so each call starts from nothing.
The assembly and GPU twins will be written against this signature.

Change:

```
/* Fill out[k] for the entities at sorted positions p0 + k, k = 0 .. count-1.
 * The positions may span cells; cell of position p is w->cell[w->order[p]]. */
void az_scan_range(const AzWorld *w, u32 p0, u32 count, AzPercept *out);
```

`job_perceive` then walks sorted positions `cell_start[cb] .. cell_start[ce]`
in steps of `AZ_SCAN_BATCH`. Canonical neighbour order (D3) is unchanged.
Expected gain: batches of 32 at any density, and a row sweep reads each `pub`
record for 9 neighbour rows instead of 27 neighbour cells (estimate, not
measured).

### F8 (minor) Five EPH columns for pair matching, reset one by one every tick

`az_grid_build` writes `acc_by` 4, `claim` 8, `req` 1, `act` 1, `tgt` 4 = 18 B
per slot per tick in five store streams (18 MB at 1M, 36 MB of traffic with
the read-for-ownership). An entity is an initiator or a responder, never both
(ARCHITECTURE section 7), so `tgt` and `acc_by` are never both live.

Change: one column `AZ_COL(pair, AzPair, 1, EPH)` with
`typedef struct AzPair { u64 claim; u32 peer; u8 act; u8 req; u16 pad; }
AzPair;` (16 B; `peer` = target for an initiator, accepted claimant for a
responder). Reset is one stream of 16 B; settle touches 2 lines per pair
(`pair[s]`, `pair[r]`) instead of 4 (`act[s]`, `tgt[s]`, `claim[r]`,
`acc_by[r]`). EPH becomes 74 B.

### F9 (minor) The reorder moves the 358-byte WARM tier and copies everything twice

Arithmetic: 486 B gathered into `tmp`, 486 B copied back = 1944 B of traffic
per entity per reorder, 74 percent of it WARM. Measured with the serial stub
at 700,000 entities, one reorder in 16 ticks: `reorder=25.328` ms per tick
(runs: 25.3, 27.3, 22.9, 20.4) = 0.33 to 0.44 s per reorder, 17 percent of
the 148 ms tick.

Change: add to D6: "Column base pointers may change at a reorder; never keep
one across `az_tick` calls." That lets the grid owner gather each column into
a spare buffer and swap pointers (half the traffic). Decide now whether WARM
should stay sorted at all; if not, it needs a stable index column in HOT.

### F10 (minor) Clade tiles: free up to about 100 mixed clades, up to 2.7x slower at 4096

```
$ S:/review/cache-perf/exp/clades.exe
AVX2 twin vs az_brain_step_c on 20000 inputs: 0 mismatches
  clades   weights KB |   C random  C grouped |  AVX2 rand  AVX2 grpd
       1          2.8 |      182.9      186.8 |       52.4       53.9
     100        275.0 |      183.5      184.1 |       55.8       52.1
     512       1408.0 |      191.1      194.8 |       74.4       55.0
    1024       2816.0 |      215.5      187.0 |       88.5       60.2
    4096      11264.0 |      279.7      248.7 |      144.4       71.5
```

(ns per brain step, one thread on P core LP0, 65,536 entities, clade ids
random versus grouped in runs.) The (cell, uid) order does not group clades,
and 4096 x 2944 B = 11.5 MB is 3.8x a P-core L2.

Change: add `u16 max_live_clades` to `AzConfig` (default 256 = 0.7 MB of
weights per core) and make `az_clade_mutate` return the parent when the live
count is at the limit; reuse clade slots whose `refcount` is 0. Move `salt,
var, shift1, shift2, pad0` to directly after `b2` so one brain step reads 44
consecutive lines (today `shift1` is at offset 2907, three lines past `b2`).

## 4. What must not change

1. Structure-of-arrays columns generated from the X-macro files. Twenty
   concurrent sequential streams ran at L1 speed over 1M entities once the
   bases were staggered (4.28 ns vs 4.17 ns, `alias4.exe`).
2. `AzPub`: 16 bytes, cell-sorted, built once per tick, the only thing a
   neighbour may read. Four records per line, read as sequential runs.
3. Row-major cell index with x in the low bits (`az_cell_xyz`): the 27
   neighbour cells are 9 contiguous runs of `pub`.
4. `job_act` running social, metabolism, physics and life over one block
   before moving on: one population sweep instead of four.
5. The `AzClade` weight layout (`AZ_W1_IDX`: 128-byte groups, all on 64-byte
   boundaries, `sizeof` = 46 lines) and sharing one network per clade: mixing
   100 clades costs nothing measurable.

## 5. Not checked

- The mechanism of F2 above L1 (no performance-counter access here).
- F4 on a quiet machine; the fork-join numbers include other agents' load.
- AZ_CHUNK 128 versus 256 end to end (noise larger than the expected effect).
- The gain of F7 (signature argument and batch statistics only).
