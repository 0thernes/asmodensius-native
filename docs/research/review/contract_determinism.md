# Contract review: determinism and concurrency

Reviewer key: `determinism`. Scratch: `S:/review/determinism/`. Date: 2026-11-17.

## What was reviewed, and a warning about timing

I read ABI **version 1** (`az_abi.h`, `az_columns.def`, `az_cells.def`,
`az_world.c`, `az_tick.c`, the stubs, `ARCHITECTURE.md`, `tests/test_core.c`)
and copied that tree to `S:/review/determinism/src/`. While I was working the
lead started rewriting the contract in place: at 04:50 `N:/core/az_abi.h` said
"version 2" and `az_world.c`, `az_columns.def`, `az_cells.def` had new
timestamps, and the tree did not compile (stubs still v1). All experiments
below ran on the v1 snapshot or on the unmodified `N:/build/release/az_run.exe`.
For each finding I say whether the v2 draft I saw at 04:50 already changes it.
`az_tick.c`, `ARCHITECTURE.md` section 4, `stubs/az_life.c`, `stubs/az_ledger.c`
and `tests/test_core.c` were byte-identical to my snapshot at 04:50.

Every timing is noisy, parallel load. All numbers below were produced now.

## Builds I made

```
cmake -S S:/review/determinism/src -B src/b  -G Ninja -DAZ_ONLY=none            (stubs only)
cmake -S S:/review/determinism/src -B src/b2 -G Ninja -DAZ_ONLY="social;tier1"   (scratch social + tier1 probe)
cmake -S S:/review/determinism/src -B src/b3 -G Ninja -DAZ_ONLY="life"           (stub life + clutch size)
cmake -S S:/review/determinism/src -B src/b4 -G Ninja -DAZ_ONLY="grid"           (stub grid minus 5 reset lines)
cmake -S S:/review/determinism/fix -B fix/b  -G Ninja -DAZ_ONLY=none             (prototype of the fix for finding 1)
gcc -std=gnu2x -O1 -g -march=x86-64-v3 -fno-strict-aliasing
    -fsanitize=undefined,shift,signed-integer-overflow,bounds,alignment -fsanitize-trap=all ...   (ub/)
```

Baseline, stub build: `src/b/test_core.exe` -> `ok: 203 checks, 0 failures`,
all six hashes `c5232ce13a7e3925`.

Scratch-only additions (never in `N:/`): `tests/test_explore.c` (parametric
runner), `tests/test_pairs.c`, `tests/test_misc.c`, `core/az_social.c` (a small
interacting social module that follows the written rules), `core/az_tier1.c`
(a counting probe), and in the scratch copy of `az_tick.c` a debug switch that
randomises every split boundary and runs the serial parts in reverse order.

---

## Findings

### 1. CRITICAL - the hash depends on the reorder period as soon as the slot capacity binds

`az_spawn` refuses a birth when `n_slots >= cap`. `n_slots` counts dead slots
that have not been compacted yet ("slots in use (alive or dead since last
reorder)"), so how early births are refused depends on `reorder_period`.
`test_core` never sees it because it uses `cap = 4n + 1024` and has no deaths.

Reproduction on the official binary, nothing modified:

```
N:/build/release/az_run.exe --n 200 --ticks 600 --seed 7 --cap 1024 --grid 4,4,3 --stride 4 --k1 8 --k2 16 --species 8 --threads 1 --no-lock --quiet --reorder R
--reorder 1 : births 1014 deaths 361 cap_denied 59850   final hash d49cc72749763c70
--reorder 7 : births 1006 deaths 358 cap_denied 59669   final hash ae82c6ff0820115a
--reorder 31: births  999 deaths 358 cap_denied 60613   final hash 01ea12a8219d6097
```

It is also hit by the runner's own default capacity (`cap = n + n/2 + 4096`,
default `--reorder 16`):

```
az_run --n 4000 --ticks 500 --threads 1 --no-lock --quiet --reorder 1   -> births 6260 cap_denied 1292242 final hash 6b4707b9be68091a
az_run --n 4000 --ticks 500 --threads 1 --no-lock --quiet --reorder 16  -> births 6254 cap_denied 1292248 final hash 5990dee871c02759
```

Matter and energy stay exact in all of these; only determinism breaks.
Still present in the v2 draft (`az_world.c` line 228: `if (w->n_slots >= w->cap || ...`).

Proposed change (prototyped in `S:/review/determinism/fix/`, about 6 lines):

- `AzWorld`: add `u32 free_head;`.
- `az_grid_build`: for a slot that is dead at the start of the tick, instead of
  `spos[s] = AZ_NONE` do `spos[s] = free_head; free_head = s;` (start with
  `free_head = AZ_NONE`); `az_grid_reorder` ends with `free_head = AZ_NONE`.
- `az_spawn`: `if (free_head != AZ_NONE) { s = free_head; free_head = spos[s]; } else if (n_slots >= cap) deny; else s = n_slots++;`

Slots that die during the current tick are not on the list, so a pending
`father_slot` stays readable. A birth is then refused exactly when
`live at tick start + births so far this tick == cap`, which no reorder period
can change. Result with the prototype (same command as above through
`fix/b/test_explore.exe 7 200 600 <threads> <reorder> 1024`):

```
reorder 1, 7, 31 (serial) and 255 (3 threads): hash d49cc72749763c70, births 1014, denied 59850 in all four
cap 1408: 06c9d778f4a01d68 in all four;  fix/b/test_core.exe: ok: 203 checks, 0 failures
```

Add to `test_core`: the reorder-period comparison at a capacity that binds,
with `CHECK(cap_denied > 0)` and `CHECK(deaths > 0)`.

### 2. MAJOR - D1 allows writes that D2 lets the partner read; a module can obey every written rule and still break determinism and conservation

D1: "A parallel job writes only the entity slots or cells in its range".
D2, phase 3: "from your partner read only its EPH columns (`offer`, `act`,
`tgt`, `acc_by`, `motor`)". Nothing says the owner may not write those columns
of its own slot in phase 3. The tick itself reads one of them across workers:
`az_tick.c` line 186-187, `u32 c = w->acc_by[s]; pair_resolve(w, s, c, w->act[c], false, worker);`.

With my scratch social module (share and attack, both sides compute the
transfer from the two offers) the plain variant is exact and stable. Two
variants that only add a write to the module's own slot, legal under D1:

```
src/b2/test_pairs.exe 7 5000 300 0 <reorder> 0 <variant> 0        (serial, no threads involved)
variant 0 (no extra write):                    reorder 1/7/31 -> hash 30b110508eaf8e72, bad_audit 0
variant 1 (initiator sets its own act = NONE after resolving, "consume the intent"):
   reorder 1  hash 0ed17adc0282a552  bad_audit 300 (first tick 1)  energy_drift -19835555
   reorder 7  hash a8c2f6ec81376ca6  bad_audit 300                 energy_drift -19707160
   reorder 31 hash fafac49b7a365e32  bad_audit 300                 energy_drift -19826325
variant 2 (each side zeroes its own offer energy after resolving):
   reorder 1  hash 10c5a5f39018cd9a  bad_audit 300 ; reorder 7 16ffe14d4680b61e ; reorder 31 4f19b3f89554b779
   reorder 1 with shuffled split boundaries: 810cf5b582417991
```

Whoever has the lower slot resolves first, so the outcome follows slot order,
which follows the reorder period and the partition. With threads it is a data
race as well. Still present in the v2 draft (it adds `urge` to the readable
list, same wording).

Proposed rule text, in `ARCHITECTURE.md` section 4 and above the resolve
prototypes in `az_abi.h`:

> **D1a Phase-3 freeze.** In phase 3 nobody writes these columns of any slot,
> its own included: `offer`, `act`, `tgt`, `acc_by`, `motor`, `urge`, `cell`,
> `spos`, `uid`, `genome`, `trait`, `phen`, `sex`, `parent_a`, `parent_b`,
> `clade`, `tag`. `mod_gain` and `mod_bias` are written in phase 3 only by the
> learner of a TEACH pair. To drop an intent, set `req` bits, never `act`.

Cheap enforcement: in a debug build `job_act` checksums the frozen EPH columns
of its range before and after, and `az_tick` compares.

### 3. MAJOR - per-worker fixed-size queues make the state depend on the thread count

`AZ_BIRTHS_PER_WORKER(cap) = cap/4 + 256` is the capacity of each worker's
queue, and a full queue drops the request (`wk->n_births >= wk->cap_births`).
One worker can therefore hold `cap/4 + 256` requests in a tick, W workers W
times that, and the requests that are dropped are the last ones in that
worker's slot order. `AzBirthReq.n` ("which child of this mother this tick")
and `AZ_T_CLUTCH` say clutches are intended, so more than one request per
mother is contract-legal.

Experiment: the reference life stub with a clutch of K children per budding
(`hold/az_life_clutch.c`, the only change is the loop over `n`):

```
AZ_CLUTCH=40 src/b3/test_clutch.exe 7 3000 30 <threads> 1 16000      (per-worker queue = 4256)
threads 0: hash 34b668b4a5117ae7  births 25131  b_overflow 112976
threads 2: hash 983ad6726763200e  births 25476  b_overflow  42136
threads 4: hash 4d579e44df76b5a8  births 25702  b_overflow      0
threads 8: hash 4d579e44df76b5a8  births 25702  b_overflow      0
control AZ_CLUTCH=4: threads 0 and 4 both 8de1ac18dc6e7686, b_overflow 0
```

Second consequence, by arithmetic: `az_life_commit` gathers all queues into
`w->tmp` with no bound check. `tmp` is `cap * 128` bytes = `5.33 * cap`
requests of 24 bytes. At `cap = 1,048,576`: one queue = 262,400 requests,
`tmp` holds 5,592,405; 22 workers can queue 22 * 262,400 = 5,772,800. With the
24 workers of this machine and full queues the gather writes
(6,297,600 - 5,592,405) * 24 = 16.9 MB past the end of `tmp`.

Same pattern for events: `AZ_EVENTS_PER_WORKER = 16384`, drop when full, and
the ledger stub hands the buffers over in worker order. The v2 draft adds
`ledger_chain` ("running digest of every event emitted so far"); that digest
will differ between thread counts whenever order or loss differs.

Proposed change:

- `az_abi.h`: `#define AZ_CLUTCH_MAX 8` and the rule "a mother queues at most
  AZ_CLUTCH_MAX requests per tick, numbered n = 0..".
- Replace `AzWorker.births / n_births / cap_births` by one array
  `AzBirthReq *births` of `cap * AZ_CLUTCH_MAX` entries, request stored at
  `births[mother_slot * AZ_CLUTCH_MAX + n]`, plus `AZ_COL(nbirth, u8, 1, EPH)`.
  No queue can fill, `birth_overflow` disappears, nothing depends on the
  partition. (192 bytes per slot at K = 8; today's 64 queues cost 384.)
- `az_life_commit` sorts in its own buffer sized for the worst case, not `w->tmp`.
- Ledger: `az_ledger_flush` sorts the tick's events by
  `(kind, a, b, aux, v)` before the sink and before `ledger_chain`; size event
  buffers per slot range or state in the ABI that `ledger_chain` is undefined
  for a tick with `dropped_events > 0`.

### 4. MAJOR - the test that "holds the rules in place" cannot fail for most of them

`ARCHITECTURE.md`: "Tests that hold these rules in place (see
`tests/test_core.c`)". Measured on test_core's exact run (seed 7, N 3000, 80
ticks, 12 threads), `src/b/test_misc.exe 1`:

```
deaths 0, cap_denied 0, interactions 0, ticks with n_sorted >= 4096 (perceive split into more than 1 part): 51 of 80 (first at tick 30), dead slots ever compacted: no
```

So the claim/settle protocol, pair resolution, `az_life_kill`, compaction of
dead slots by a reorder, and capacity denial are never executed, and perceive
runs on one part for the first 29 ticks. Findings 1, 2 and 3 all pass
`test_core`. The reference social stub never proposes anything
(`az_social_score` returns -1).

Proposed change: add `tests/test_protocol.c` plus a test-only interacting
social module (mine is 70 lines: `S:/review/determinism/src/core/az_social.c`,
runner `tests/test_pairs.c`). Run N >= 5000 from tick 0, 300 ticks, threads
{0, 2, 5, 12}, reorder {1, 7, 31, 255}, once with `cap = 4n` and once with a
capacity that binds, and add `CHECK(deaths > 0)`, `CHECK(interactions > 0)`,
`CHECK(cap_denied > 0)` so the test cannot go vacuous again. Add
`AzConfig.debug_split_seed` (nonzero: `az_tick` randomises every split
boundary and, without a host, runs the parts in reverse order); the hash must
not change. My scratch `az_tick.c` has that switch (`dbg_shuffle`).

### 5. MAJOR - an entity born this tick is alive in Tier 1 with no cell and no public record

`az_spawn` sets `cell[s] = AZ_NONE` and (v1) leaves `spos[s] = 0`; births
happen in stage 5 and Tier 1 runs in stage 6 over `[0, n_slots)`. The column
comments promise otherwise: `cell` "cell index this tick (AZ_NONE if not
alive)", `spos` "order[spos[s]] == s, pub[spos[s]] is my record".

Probe in `az_tier1_range` on test_core's configuration
(`src/b2/test_pairs.exe 7 3000 80 0 1 0 0 0`):

```
t1 alive 39531 cell_none 157 spos_wrong 157 dead 0        (same with 5 threads, reorder 7)
```

157 alive entities reached Tier 1 with `cell == AZ_NONE`; indexing a cell
column with it reads `c_sp[0xFFFFFFFF * 8]`, about 128 GiB past the array.
`az_life_kill` on such a slot posts to cell `AZ_NONE` the same way.
v2 draft: `az_spawn` now sets `spos = AZ_NONE` and `az_scan_list` says "The
entity must have cell[slot] != AZ_NONE", which helps; the rule is still not
written where a Tier 1 or life owner will look.

Proposed change: in `az_life_commit`, after the child's position is set,
`w->cell[ch] = az_cell_at(w, pos_x, pos_y, pos_z)` (add
`AZ_INLINE void az_place(AzWorld *w, u32 slot)` to `az_abi.h` so every life
module does it the same way). Add to D6: "A slot born this tick has a valid
`cell` and `spos == AZ_NONE`: it has no `pub` record and is in no cell list
until the next `az_grid_build`. Stages 5 and 6 must test `spos` before using
`pub`." Fix the two column comments.

### 6. MINOR - an odd `field_stride` breaks the stated guarantee across the x and y wrap

D2: "the stride guarantees no two face-adjacent cells are stepped in the same
pass". The only validation is `if (field_stride < 2) field_stride = 8`, and
`az_run --stride` passes any value. Cells x = gx-1 and x = 0 are neighbours;
their sums differ by gx-1, so both are stepped when `(gx-1) % stride == 0`.
Counted by enumeration (face-adjacent pairs stepped together, tick 0):

```
grid 4,4,3: stride 2 -> 0, 3 -> 86, 4 -> 0, 5 -> 52, 6 -> 0, 7 -> 0, 8 -> 0, 15 -> 18
grid 6,6,5: stride 3 -> 1366, 7 -> 586, 8 -> 0, 9 -> 456, 21 -> 196
```

gx-1 is odd, so every even stride is safe. Proposed change: `AzConfig`
comment and `az_world_create`: "field_stride must be a power of two, 2..128;
anything else makes az_world_create return NULL", and step with
`((x + y + z + tick) & (stride - 1)) == 0`. Still open in the v2 draft.

### 7. MINOR - uid exhaustion: decide the policy now, the column is in the HOT budget

v1 wrapped silently. `src/b/test_misc.exe 3` (start `uid_next` 16 below 2^32,
60 ticks):

```
uid_next 1220, births 1236, alive with uid 0: 1, alive with uid 0xFFFFFFFF: 1, alive entities sharing a uid with another alive entity: 1219
```

The v2 draft now refuses at `uid_next == 0xFFFFFFFF` and counts it in
`cap_denied`. That removes the corruption, but then every birth is refused for
the rest of the run and the only sign is a counter that also means "arena
full". Reach, by arithmetic: the stub measured 54112 births in 60 ticks at
about 127,000 mean live (`az_run --n 100000 --ticks 60`), 0.71 % per tick; at
10^6 live that is about 7,100 uids per tick, and 4,294,967,295 / 7,100 is about
605,000 ticks.

Proposed change: `AzTotals.uid_exhausted` (own counter) and an
`AZ_EV_TICK`-level event when it first becomes nonzero; state in D6 "a world
ends after 2^32 - 2 births; `az_snapshot` plus an offline renumbering tool is
the only continuation". If that limit is not acceptable, widen `uid` to u64
now, before 30 modules store u32 uids in `parent_a`, `partner`, `AzPub` and
`AzBirthReq`.

### 8. MINOR - the role function ignores half of the seed and bypasses D4; half of all proposals are void by construction

`az_is_initiator` mixes `uid`, `tick` and `key[0] << 20` with `az_mix64`.
D4 says the only randomness is `az_rand*`, and `AZ_RS_ROLE` exists but is unused.
Computed with the same formula (2000 uids x 200 ticks):

```
seed 7 vs seed 7 + 2^32:                    differ in 0 of 400000 role bits
seed 7 at tick t vs seed 7^5 at tick t^(5<<20): differ in 0 of 400000
seed 7 vs seed 8 (control):                 differ in 199442 of 400000
```

Separately, an initiator's target must be a responder, and the percept's
nearest-neighbour slots are role-blind. Measured with the scratch social
module (`test_pairs 7 5000 300 0 1 0 0 0`): `intents with a target 656327,
dropped because the target is an initiator 327908`. In v1 a module could not
even filter, since D2 phase 1 allows "others only through pub[]" and `AzPub`
had no uid; the v2 draft puts `uid` in `AzPub`, which makes filtering legal.

Proposed change: `u64 role_key = az_mix64(((u64)key[1] << 32 | key[0]) ^ 0x9E3779B97F4A7C15ull)`
stored in `AzWorld` at creation, and
`h = az_mix64((((u64)uid << 32) | tick) ^ role_key)`; name the function in D4
as the one sanctioned exception. Add to section 7: "an initiator proposes only
to a neighbour with `!az_is_initiator(w, pub.uid)`".

### 9. MINOR - integer corners that D5 forbids and nothing prevents

Trap build (`ub/`, no `-fwrapv`), one call each, `ub/fuzz_fx.exe <case>`:

```
case 2  az_rotor_mul with components +-32767 (any i16 can sit in rot_*)   -> Illegal instruction (signed overflow)
case 4  az_brain_step_c with AzClade.shift1 = 40                          -> Illegal instruction (shift count)
case 6  az_abs_i32(INT32_MIN)                                             -> Illegal instruction
case 7  az_world_bytes with gx,gy,gz bits = 12,12,12 (called before az_world_create validates) -> Illegal instruction (1u << 36)
cases 1, 3, 5 (unit rotors, rotor step, extreme legal brain inputs)       -> ok
```

Case 4 matters for "C or assembly kernels": scalar x86 masks the count to 5
bits, a vector shift does not, so a clade with `shift1 >= 32` would hash
differently under a twin. Also `ARCHITECTURE.md` section 5 writes
`pos += vel << (shift - 16)`, a left shift of a negative value, and the build
passes `-fwrapv` while D5 says "No signed overflow".

Proposed change: `AzClade` comment "shift1, shift2 in 1..15" and
`az_clade_develop` clamps them; `az_world_bytes` returns 0 for an invalid
grid; `az_abs_i32` documented for `v > INT32_MIN`; section 5 formula becomes
`pos += (u32)(vel * (1 << (shift - 16)))`; D5 gets the sentence "the build
uses -fwrapv as a safety net; code must not rely on it", and CI builds the
tests once with `-fsanitize=...,signed-integer-overflow -fsanitize-trap=all`
and without `-fwrapv` (works with this gcc; `-lubsan` is not available).

### 10. MINOR - the per-tick scratch reset that the tick protocol depends on lives in a module stub

`az_tick.c` relies on `acc_by`, `claim`, `req`, `act`, `tgt` being reset each
tick, but the reset is five lines inside `stubs/az_grid.c`, and the v1 grid
contract listed only "cell[], cell_start[], order[], pub[], n_live; sets
AZ_F_INIT" (stub header: "produces exactly the same order[], cell_start[],
pub[] and flags"). Stub grid minus those five lines
(`src/b4/test_explore.exe 7 200 600 0 <reorder> 0`):

```
reference stub: reorder 1 and 7 -> hash 10a2094433971f7d, live 833
without resets: reorder 1 -> eef283bf007816a7 live 5 ; reorder 7 -> 87ab891ef635476e live 0 ; reorder 31 -> dcedd7310896cca1 live 36
```

The v2 draft comment now says "resets per-tick scratch" without naming the
columns, and adds a new EPH column `urge` that the 04:45 grid stub does not
reset. Proposed change: move the reset into `az_tick.c` as its own job
(`job_scratch_reset`, parallel over slots: `acc_by = AZ_NONE; claim = 0;
req = 0; act = AZ_ACT_NONE; tgt = AZ_NONE; urge = 0;`), so the tick owns what
the tick needs and no grid owner can break it.

---

## What the contract gets right - do not change

1. **The claim/settle protocol** (`claim = max(score << 32 | ~uid)` by CAS, the
   single winner writes `acc_by`, losers clear `act`, defensive checks on the
   target in `job_claims`). With the scratch social module: about 281,000
   pairs in 300 ticks, 169 deaths, hash `30b110508eaf8e72` for threads 0, 2,
   5, 12, 24, reorder 1, 7, 31, and six shuffled-boundary runs; seeds 5001
   and 5002 (four configurations each) and 5003 (two) likewise; `bad_audit 0`
   everywhere.
2. **Field columns read-only in the entity phases, integer accumulators,
   floor-fraction grants.** Arbitrary split boundaries and reversed part order
   gave the same hash and exact books, so nothing depends on the partition.
3. **Counter-based Philox keyed by (id, tick, stream, idx) and the pair rule**
   (smaller uid as id, larger in idx). Known answers pass; both sides of a
   pair agreed in every run.
4. **The hash design**: commutative sum of per-entity hashes keyed by uid,
   alive slots only, EPH excluded; hashed structs have explicit padding
   (`gcc -Wpadded` reports none inside `AzClade` (2944 bytes) or `AzColony`
   (104)). The v2 draft adds world constants and `gstate`, which is right.
5. **Serial life commit in (mother uid, n) order, append-only spawning inside
   a tick, and time used only for `prof_ns`.** The trap build without
   `-fwrapv` runs `test_core` and long runs with deaths to the same hashes as
   the release build (`c5232ce13a7e3925`, `71fc3865607a8fb3`, `01ea12a8219d6097`).

## Not ranked, one line each

- After a field underflow `az_field_commit` leaves `c_delta` / `c_dheat`
  pending; `az_total_matter` counts them but the hash does not
  (`test_misc 2`: "pending c_delta != 0 : hash SAME").
- If `az_spawn` fails for a mother that died earlier in the same tick,
  `az_life_commit` refunds into a dead slot and the matter vanishes at the
  next reorder. Not reachable with the stub (a dying entity queues no birth).
- 24-thread runs were very slow on this loaded machine (one batch of twelve
  200-tick runs passed 100 s and I stopped it): the pool's master spins on
  `pending` while pinned workers are descheduled. Slow, not wrong; every
  24-thread hash that finished matched.
