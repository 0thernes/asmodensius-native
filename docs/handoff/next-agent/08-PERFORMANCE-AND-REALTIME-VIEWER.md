# 08 Performance and the real-time viewer

> Written 2026-11-18 for the next agent (OpenAI GPT Astra 6.2 or any coding agent) by a Claude session that
> only read files. Nothing was built, run or changed for this document. Every number below is cited to the
> file it comes from as `path:line` (line numbers as of 2026-11-18; files grow, so search for the quoted text if
> a line moved). Numbers marked "estimate" are arithmetic, not measurements; numbers marked "goal" are targets
> to be measured, never promises. Project root, written `$A` below:
> `Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native`.

## Contents

- [0. The plain answer for the owner](#0-the-plain-answer-for-the-owner)
- [1. How the demo works today](#1-how-the-demo-works-today)
- [2. How fast the engine is today](#2-how-fast-the-engine-is-today)
- [3. Rules every task here keeps](#3-rules-every-task-here-keeps)
- [4. Order of work and milestone map](#4-order-of-work-and-milestone-map)
- [5. Track PQ: quick wins for the picture demo](#5-track-pq-quick-wins-for-the-picture-demo)
- [6. Track RV-A: the real-time viewer in the browser](#6-track-rv-a-the-real-time-viewer-in-the-browser)
- [7. Track RV-B: the native Windows viewer](#7-track-rv-b-the-native-windows-viewer)
- [8. Track ES: engine speed](#8-track-es-engine-speed)
- [9. Track SC: scaling after the gate](#9-track-sc-scaling-after-the-gate)
- [10. What is honest to promise](#10-what-is-honest-to-promise)
- [11. Problems, holes, blind spots and dilemmas](#11-problems-holes-blind-spots-and-dilemmas)
- [12. Decisions for the owner](#12-decisions-for-the-owner)
- [13. Evidence formats](#13-evidence-formats)
- [14. Checklist of every task](#14-checklist-of-every-task)

Effort sizes used below: **S** = up to half a day of agent work, **M** = 1 to 2 days, **L** = 3 to 5 days,
**XL** = more than a week. Task ids: `PQ-n` (picture demo), `RV-An` (browser viewer), `RV-Bn` (native viewer),
`ES-n` (engine speed), `SC-n` (scaling), `PB-nn` (problems list).

---

## 0. The plain answer for the owner

You asked (P51, 2026-11-18 19:29:42 UTC): "I notice it runs on PICTURES and TICS. Not Frames Per Second. Why?
Can that be fixed? It's really slow. I thought this would be fast."

**What a tick is and what a picture is.** A tick is one step of the world's own time: every creature senses,
thinks, moves, eats, fights, trades, mates, and the chemistry and weather move once. A picture is a drawing of
the world. They are kept apart on purpose. The world must turn out exactly the same whether anyone watches it
or not, and however often they look (rule D7, `docs/ARCHITECTURE.md:192-194`). So the demo counts both: ticks
for the world, pictures for you. "Frames per second" is the picture rate.

**Why the demo is slow.** The demo was built to prove that the world is real, not to be fast. In a small world
four things slow it down, and the engine itself is not one of them. In your own world of a million creatures
the engine is slow too (see "Your own world, measured" below):

1. **The world waits for the painter.** `az_live` runs the next ticks only when the next picture is due
   (`tools/az_live.c:504-507`, `:525-529`). So the world moves at "ticks per picture" times "pictures per
   second". The page starts with 2 ticks per picture and at most 6 pictures per second (`demo/live.html:222-223`):
   at most 12 ticks per second. A picture takes 0.1 to 0.5 s, so in practice about 2 to 6 pictures and 4 to 12
   ticks per second. The engine alone does 150 to 330 ticks per second at that size (5,000 to 10,000
   creatures). (Note added 2026-11-18 by the fixer pass: these numbers and the picture times below were
   measured before worldgen and no log holds them, `docs/handoff/POC-MVP-REPORT.md:163-165`; the PoC
   report's "3 to 13 ms" per tick is 77 to 333 ticks per second. Quote 77 to 330 until PQ-0 re-measures on
   the worldgen world.)
2. **One CPU core paints every picture.** The engine's own software renderer (`view/az_view.c`) runs on one
   thread. For every pixel it computes sky and floor (`view/az_view.c:355-400`); for every nearby creature it
   traces the full 3D outline of its body plan, step by step along each view ray (`view/az_view.c:666-743`).
   Measured in the session that wrote this package (no log file was kept): 484 ms for one 960x540 picture
   with about 11,000 creatures, about 100 ms at 640x360 with about 5,000. In your own world of 1,000,416
   creatures one 1600x900 picture took 4,261.3 ms (logged, frame 226; section 2.5).
3. **Every picture is a 1.5 MB file on disk.** It is packed as an uncompressed PNG (`view/az_png.c:1-6`,
   960 x 540 x 3 bytes), written to disk, read back by the Python server and decoded by the browser. Before and
   after each picture the whole world is hashed twice to prove that drawing changed nothing
   (`tools/az_live.c:532-536`), and a statistics pass walks every creature (`tools/az_live.c:564-584`).
4. **The page asks for news only 4 times a second** (`demo/live.html:721`, a 250 ms poll). Even a faster
   engine could not show more than 4 pictures per second.

Also: the page never asks for the assembly brain kernels (`demo/live.html:538-540` sends no `kernels`), so the
demo always runs the slower C reference brain, and `az_live` cannot use the 16 E cores (it passes no host
flags, `tools/az_live.c:384`).

**Your own world, measured.** The world you were watching was read, read-only, from the demo's
`/api/state` on 2026-11-18. The read of frame 226 (returned 11:40:56 local, 19:40:56 UTC) is saved as
`build/work/logs/demo_owner_world_2026-11-18_f226.json`; the read of frame 228 (19:42:01 UTC) is saved as
`build/work/logs/demo_owner_world_2026-11-18.json`. The figures below are frame 226. It is run `20261118-103059-46dbdf`, wild, started at
10:31 local with `--n 250000 --species 100 --cap 1004096 --size 1600x900 --every 60 --fps 30 --threads 0`
(8 workers) on a 64 x 64 x 32 grid. At tick 13,560 it held 1,000,416 living creatures: the population had
reached its cap of 1,004,096 (12,241,216 births refused by the cap, none by the store), and matter and energy
were exact. There:
- one tick took 414.3 ms, about 2.4 ticks per second;
- one 1600x900 picture took 4,261.3 ms (878,035 bodies drawn, 126,398 of them ray-marched; the time includes
  the two state hashes, `tools/az_live.c:532-537`), plus 27.5 ms for the PNG;
- `az_live` runs `az_tick` (`tools/az_live.c:527`) and then `az_view_render` (`:534`) on the same thread, so
  the world stands still while each picture is drawn: about 4.3 s per picture, about 15 percent of the wall
  time at 60 ticks per picture (60 x 0.414 s = 24.9 s of ticks, then 4.3 s of drawing);
- so a new picture reached the page about every 29 s, by arithmetic (60 x 414.3 ms + 4,261.3 ms + 27.5 ms
  = 29.1 s, plus a statistics pass that is not timed); the period itself was not measured (PQ-1 adds
  `ms_period`).

So at a million creatures both halves are slow: the picture path (4.3 s per picture, with the world stopped)
and the world itself (2.4 ticks per second). The real-time viewer (section 6) gives a smooth camera at 60
frames per second whatever the tick rate, and it must never stop the world for seconds (RV-A9 target case 2).
Only the engine work (section 8) makes the world step faster. Section 2.5 has the stage times.

**Can it be fixed? Yes.** In three steps:

1. **Quick fixes (days, section 5).** Let the world tick at full speed while pictures are drawn on the side;
   paint with 8 threads instead of 1; use the assembly kernels and the E cores; stop the 4-per-second cap in
   the page. Expected: the world runs at nearly the engine's own speed; pictures several times faster
   (estimates, to be measured).
2. **A real-time viewer (one to two weeks, section 6).** Instead of a painted picture, the engine sends a
   compact list of every creature (24 bytes each: where it is, which way it faces, its size, colour, flags and
   lineage) and the world's terrain, ocean, vents and suns. Your graphics card draws them in the browser at
   60 frames per second and moves each creature smoothly between ticks. The world runs flat out on its own
   clock, with a speed control and pause and step. You can fly the camera and click a creature to see its
   genome size, brain, energy and family. The old painted picture stays as the proof view: one click shows the
   engine's own drawing of the same moment, with the hash check.
3. **Engine speed (milestones 6, 8 and 9, sections 8 and 9).** Make the world itself compute more ticks per
   second: measure every stage against the caches, the scan and physics assembly twins, parallel births,
   fewer serial steps, all 24 cores, and after the foundation gate the GPU and the DDR5 streaming tier.

**What to expect, honestly.**
- The display can be smooth (goal: 60 frames per second with 100,000 creatures in view on the RTX 5070 Ti).
- Smooth display does not make the world think faster. At 100,000 founders the engine does 13.6 ticks per
  second on 8 P cores today (16.7 on all 24; `build/work/logs/wg_ref100k_t8.txt:7`,
  `build/work/PROGRESS.md:1589`). The viewer then shows about 14 world steps per second, drawn smoothly at 60.
- At 1,000,000 founders the engine does 0.75 ticks per second (`build/work/logs/wg_ref1m.txt:7`), so the
  world moves slowly however good the display is. The engine plan (section 8) aims at 30 ticks per second at
  100,000 founders. That is a goal to be measured, and section 10 shows why it may need all 24 cores.
- Your own wild world at 1,000,416 creatures does about 2.4 ticks per second (section 2.5). A 60 frames per
  second viewer keeps the camera smooth there, but each creature gets a newly computed place only about 2.4
  times per second; the motion in between is smoothing, and the page says so.

---

## 1. How the demo works today

### 1.1 The pieces

| Piece | File | What it does |
|---|---|---|
| Engine program | `tools/az_live.c` | makes one world (wild by default, lab with `--lab`), ticks it, renders a picture every `--every` ticks with `az_view_render`, writes `DIR\frame-NNNNNNNN.png`, prints one JSON line per picture on stdout, reads commands on stdin (`run`, `pause`, `step K`, `every K`, `fps F`, `camera YAW PITCH DIST`, `size WxH`, `save FILE`, `quit`; `tools/az_live.c:24-29`, `:195-234`) |
| Renderer | `view/az_view.c`, `view/az_view.h` | hosted C with floats; reads `const AzWorld *` between ticks; sky, a floor tinted by light and organic matter, the world box, every living body (full silhouette ray-march when its projected radius is at least 1.6 px, else a shaded sphere), text overlay and a clade legend (`view/az_view.c:885-896`, `:770-803`) |
| PNG writer | `view/az_png.c` | stored (uncompressed) deflate blocks, CRC-32 four bits at a time (`view/az_png.c:14-27`), one byte per loop step (`:92-116`) |
| Server | `demo/live_server.py` | Python 3.9 standard library; serves `live.html` on `127.0.0.1:8765`; starts `az_live` with argument lists (`:172-189`); reads its JSON lines; serves the newest PNG at `/frame/<run>/<file>` (`:608-615`); forwards whitelisted commands (`:678-680`) |
| Page | `demo/live.html` | no libraries, no internet; "Ticks per picture" and "Pictures per second (max)" (`:222-223`); polls `/api/state` every 250 ms (`:691-722`) and loads the newest PNG through an `Image` object (`:603-616`) |
| Smoke test | `tests/cli/live_smoke.py` (ctest `cli_live_smoke`, `CMakeLists.txt:301-305`) | 21 checks: modes, exact books, observer hash, lab hash equals az_run's, pipe commands, save and load |

### 1.2 The loop in az_live, step by step (`tools/az_live.c:463-624`)

1. Read stdin commands (`:464`).
2. If not paused and no step is pending: if less than `1/fps` seconds passed since the last picture started,
   sleep 2 ms and loop (`:504-507`). Otherwise `todo = every`.
3. Run `todo` ticks; after each tick compare matter and energy with the start (`:526-529`).
4. `h0 = az_state_hash(w)`; `az_view_render`; `h1 = az_state_hash(w)`; count a failure if they differ
   (`:532-536`).
5. Copy the pixels, draw the label, `az_png_encode`, `fopen`/`fwrite`/`fclose` the PNG (`:542-561`).
6. `az_stats_collect` and a pass over every slot reading genes, brain and parts (`:564-584`).
7. Print one JSON line with `ms_tick`, `ms_render`, `ms_png` and the per-stage times `stages` (`:586-618`).
8. Delete frames older than `--keep` (`:622`).

Consequence: the period of the loop is `max(1/fps, T_ticks + T_hash + T_render + T_png + T_stats)`, and the
world advances `every` ticks per period. Ticks per second = `every / period`.

### 1.3 The caps on speed, and what each fix is worth

| # | Cap | Where | Today | Fix (task) | Expected effect (estimate unless measured) |
|---|---|---|---|---|---|
| C1 | ticks wait for pictures | `tools/az_live.c:504-507` | world speed = every x pictures/s, 4 to 12 ticks/s at the page defaults | free pacing with a render thread (PQ-2, PQ-5) | world speed close to az_run's (150 to 330 ticks/s at 5k to 10k creatures, measured by the writer's session before worldgen with no log; quote 77 to 330 until PQ-0) minus the scene capture cost |
| C2 | one thread paints | `view/az_view.c:355-400`, `:666-743` | 0.1 to 0.5 s per picture in small worlds; 4.3 s at 1,000,416 creatures and 1600x900, the world stopped meanwhile (section 2.5) | row bands on 8 threads (PQ-4) | about 5x faster pictures (estimate from the tick's own 1-to-8-worker scaling of 5.05x, `build/work/PROGRESS.md:1589`) |
| C3 | ray-marched bodies | `view/az_view.c:711-728` (6 to 24 steps, 5 bisections, 3 gradient samples per pixel) | most of the body cost | a "fast" view mode drawing spheres only (PQ-8) | several times faster at high creature counts (estimate); measure with `marched` in the JSON line |
| C4 | PNG, disk, decode | `view/az_png.c`, `tools/az_live.c:547-561` | 1.5 MB per picture | 8-bit table CRC (PQ-3); optional raw RGBA drawn into a canvas (PQ-3b) | removes most of `ms_png` (measure: the field exists today) |
| C5 | two whole-world hashes | `tools/az_live.c:532-536` | cost unknown; it hashes the creature store too (`core/az_abi.h:1346`, about 2.7 KB per creature, `build/work/logs/wg_ref100k_t8.txt:10`) | measure it (PQ-1); hash the captured scene path once per picture, not the render | unknown until PQ-1 |
| C6 | stats pass | `tools/az_live.c:564-584` | one full slot walk per picture | rate-limit to 4 per second (PQ-2) | small at 5k; grows with n |
| C7 | 250 ms page poll | `demo/live.html:721` | at most 4 pictures per second shown | multipart push stream (PQ-6) | shows every picture made |
| C8 | C reference brain | `demo/live.html:538-540`, `demo/live_server.py:186-187` | kernels 0 always | send kernels 1 (PQ-7) | at the econ landing: 49.7 -> 40.3 ms per tick at 100k (`build/work/PROGRESS.md:864-865`) |
| C9 | 8 P cores only | `tools/az_live.c:384` (host flags 0) | E cores idle | `--ecores` (PQ-7) | at 100k: 73.7 -> 59.9 ms per tick (`build/work/PROGRESS.md:1589`) |

### 1.4 What the picture shows and does not show

- Shows: a dark sky, a floor at z = 0 tinted by light and organic matter per column (`view/az_view.c:322-353`),
  the world box, every living body with its clade body plan and individual colour, the tick, the living count,
  the 8 largest clades.
- Does not show: the terrain (`c_solid`, `core/az_cells.def:41`), the ocean surface and volume (the solvent
  phase in `c_phase`, `core/az_cells.def:36`), vents (`w->vent_cell`, `core/az_abi.h:1034`), the suns and
  their light direction (`c_lvec`, `core/az_cells.def:50`), temperature, colonies, the grav field. Worldgen
  makes all of these since 2026-11-18; the picture predates it.
- Known limit, not a viewer defect: the physics module is still a stub that ignores gravity, rock and density,
  so bodies can pass through rock (`build/work/PROGRESS.md:1603`). A terrain-aware viewer will show that.

---

## 2. How fast the engine is today

### 2.1 Stage times of the two reference worlds

100,000 founders, 60 ticks, 8 workers, lab seed 42, `--kernels` not given (so the C reference brain:
"kernels: brain step C reference", `build/work/logs/wg_ref100k_t8.txt:4`), idle machine. The world grows from
100,000 to 227,469 living creatures during the run (`:7`). Source: `build/work/logs/wg_ref100k_t8.txt:7-8`.

| Stage | ms per tick | Share |
|---|---|---|
| reorder | 1.566 | 2.1 % |
| grid | 7.497 | 10.2 % |
| perceive (scan, sense, brain, intents) | 34.188 | 46.4 % |
| match | 1.513 | 2.1 % |
| act | 6.628 | 9.0 % |
| field | 1.795 | 2.4 % |
| life (serial birth commit) | 12.740 | 17.3 % |
| tier1 | 6.311 | 8.6 % |
| tier2 | 0.052 | 0.1 % |
| ledger | 1.408 | 1.9 % |
| **total** | **73.698** | 13.6 ticks/s; 324 ns per live creature per tick |

1,000,000 founders, 20 ticks, 8 workers, `--kernels 1 --store-units 52000000`. 1,209,948 alive at the end.
The creature store is full: 380,865 births refused by the store (`build/work/logs/wg_ref1m.txt:11`).
Source: `build/work/logs/wg_ref1m.txt:7-8`.

| Stage | ms per tick | Share |
|---|---|---|
| reorder | 21.039 | 1.6 % |
| grid | 58.570 | 4.4 % |
| perceive | 254.498 | 19.0 % |
| match | 8.438 | 0.6 % |
| act | 32.754 | 2.4 % |
| field | 6.816 | 0.5 % |
| life | 313.054 | 23.4 % |
| tier1 | 633.330 | 47.3 % |
| tier2 | 0.985 | 0.1 % |
| ledger | 9.963 | 0.7 % |
| **total** | **1339.447** | 0.75 ticks/s (1/1.339 s); 1,107 ns per live creature per tick |

Thread scaling at 100k: 372.4 ms on 1 worker, 73.7 on 8, 59.9 on 24 with E cores (`--ecores --no-pin`),
the same hash `73f1e9e013f8db50` on all three (`build/work/PROGRESS.md:1589-1590`): 5.05x on 8 P cores, 6.2x on
24 cores.

### 2.2 Speed history at 100k founders (8 workers)

| When | ms per tick | Source |
|---|---|---|
| ABI 4 freeze, before the biology: C reference / twins | 19.24 / 16.77 | `build/work/PROGRESS.md:426-427` |
| life landing | 34.2 | `build/work/PROGRESS.md:501-503` |
| mate landing | 41.1 | `build/work/PROGRESS.md:546-547` |
| tier1 landing | 45.7 (tier1 5.7) | `build/work/PROGRESS.md:595-597` |
| metab landing | 45.2 | `build/work/PROGRESS.md:678` |
| econ landing: C / twins | 49.7 / 40.3 | `build/work/PROGRESS.md:864-865` |
| chem piece A | 46.3 | `build/work/PROGRESS.md:1212` |
| chem piece B (lightly loaded) | 54.7 | `build/work/PROGRESS.md:1599` |
| worldgen (idle) | 73.7 | `build/work/PROGRESS.md:1589-1600` |

Three modules are still stubs: sense, physics and stats (`docs/handoff/next-agent/02-STAGES-AND-STATUS.md:22`).
Each will add cost of a size nobody knows yet. Every speed target in this document must be re-measured after
they land.

### 2.3 The 1M world is a full-store stress test, not a tick benchmark

- Tier 1 is 47 percent of the 1M tick and about 100 times its 100k cost for about 5.3 times the creatures.
  The cause was named at the metab landing: a surprised creature asks for long-term memory words, and each word
  relocates its whole block in the serial store commit (`build/work/PROGRESS.md:683-688`). The fix named
  there, "grow memory in chunks", was not done.
- The life stage (313 ms) runs while the store refuses 380,865 births (`build/work/logs/wg_ref1m.txt:11`).
  Store packing costs about 250 ms each time at 1M (`build/work/PROGRESS.md:434`).
- So the 1M number mostly measures a full store. E3 (task ES-14) must report store refusals on every row.

### 2.4 The cache arithmetic, corrected (blind spot PB-07)

- The plan says a world is DRAM-bound "above roughly half a million creatures" because the hot tier is
  140 bytes per creature and L2 + L3 is 76 MB (`docs/handoff/CONTINUE-BUILD.md:29`; L2 40 MB plus L3 36 MB,
  `docs/agents/COMMON.md:45-47`).
- Since ABI 4 every creature has its own brain in its own store block, and a brain step reads about 1.9 KB per
  creature per tick (`docs/TRIGGER-REGISTER.md:35`, TR-15). The fixed record is now 124 bytes and EPH scratch
  170 bytes (`docs/handoff/next-agent/04-GOALS-AND-BENCHMARKS.md:203-204`).
- Estimate with the same arithmetic: about 124 + 170 + 1,900 = about 2,200 bytes touched per creature per tick,
  so 76 MB holds about 35,000 creatures. By this estimate **both reference worlds are far beyond the caches**,
  the 100k world included (227,469 alive at its end).
- It is an estimate. M2 (task ES-1) measures it. Until then no document may call the 100k world
  cache-resident, and law 2 ("hot loops must not depend on DRAM latency", `docs/agents/COMMON.md:19-21`) is
  met only through the prefetches added at ABI 4 (`build/work/PROGRESS.md:430-433`), not by residency.
- A second estimate, for scale only: 227,469 creatures x about 2.2 KB = about 0.5 GB per tick; at 13.6 ticks
  per second that is about 7 GB/s. Run `tools/membench/membench.c` (it exists) to get this machine's real
  numbers before concluding whether bandwidth or latency binds. No GB/s figure for this machine is recorded in
  the package; do not quote one until membench prints it.

### 2.5 The demo's picture timings, and the owner's 1M world

Every measured picture time on record. The two small-world rows were measured by the session that wrote this
package, before worldgen, and no log holds them; the two 1M rows are logged.

| World | Picture size | Creatures | ms per picture | Tick speed | Log |
|---|---|---|---|---|---|
| lab, before worldgen | 640x360 | about 5,000 | about 100 | engine alone 150 to 330 ticks/s at 5,000 to 10,000 (quote 77 to 330 until PQ-0) | none; PQ-0 re-measures |
| lab, before worldgen | 960x540 | about 11,000 | 484 | same | none; PQ-0 re-measures |
| the owner's wild world, run `20261118-103059-46dbdf`, tick 13,560 (frame 226) | 1600x900 | 1,000,416 alive; 878,035 drawn, 126,398 ray-marched | 4,261.3 render (both state hashes included, `tools/az_live.c:532-537`) plus 27.5 PNG | 414.3 ms per tick, about 2.4 ticks/s | `build/work/logs/demo_owner_world_2026-11-18_f226.json` (key `last`) |
| the same world, tick 13,680 (frame 228) | 1600x900 | 1,000,166 alive; 860,072 drawn, 127,977 ray-marched | 4,383.3 render plus 28.5 PNG | 397.6 ms per tick, about 2.5 ticks/s | `build/work/logs/demo_owner_world_2026-11-18.json` (key `last`) |

Stage times of the owner's world at tick 13,560, frame 226 (8 workers; C reference brain, because the page sends no
`kernels`: `"kernels": false` in the saved options; wild mode; grid 64 x 64 x 32, 131,072 cells; arena
8,773.4 MB locked):

| Stage | ms per tick | Share of 414.3 ms |
|---|---|---|
| perceive | 138.7 | 33.5 % |
| grid | 45.0 | 10.9 % |
| act | 38.1 | 9.2 % |
| tier1 | 27.2 | 6.6 % |
| life | 19.8 | 4.8 % |
| reorder | 10.7 | 2.6 % |
| match | 3.9 | 0.9 % |
| field | 2.6 | 0.6 % |
| ledger | 1.1 | 0.3 % |
| tier2 | 0.0 | 0.0 % |
| sum of the named stages | 287.1 | 69.3 % |
| not in the named stage times | 127.2 | 30.7 % |
| **total** | **414.3** | about 2.4 ticks/s; about 414 ns per live creature per tick |

- The state hash before and after drawing was equal (`5c622edb6b637b82`): the observer rule held.
- The world at that moment: eggs 197,295, max generation 24, 6 clades, 617 colonies, births 41,132,095,
  deaths 40,381,679, store refusals 0, `entropy_missed` 0. Bodies: genes 144 to 183 (mean 160.0), neurons 8 to
  56 (mean 31.1), parts 1 to 8 (mean 2.0), units 21 to 49 (mean 39.6).
- Wall clock: not measured. By arithmetic one picture period is about 29 s (60 x 414.3 ms of ticks,
  4,261.3 ms of drawing, 27.5 ms of PNG, plus the untimed statistics pass, `tools/az_live.c:564-584`), and
  the world stood still for about 4.3 s of it, about 15 percent (PB-36). An earlier note said "pictures 226
  to 228 (120 ticks) took about 40 s"; that cannot be right, because 120 x 414.3 ms is already 49.7 s
  before any drawing. The 40 s matches the gap between two reads (19:41:21 and 19:42:01 UTC) that showed
  frame 226 and then frame 228; reads do not time pictures. PQ-1 (`ms_period`) measures the period.
- Do not compare it with the lab 1M reference of section 2.1 as if it were the same world: that one is lab,
  20 ticks from 1,000,000 founders, `--kernels 1`, a full creature store (380,865 store refusals); this one is
  wild, at tick 13,560, bound by the population cap (none refused by the store), with the C brain.
- About 31 percent of the tick (127.2 ms) is not in the named stage times. ES-1 (M2) must find where it goes
  (PB-37). One candidate, read from the code and not measured: after every tick `az_live` calls
  `az_total_matter` and `az_total_energy` (`tools/az_live.c:528`), inside the time that `ms_tick`
  counts, and each walks every cell and every slot on one thread (`core/az_world.c:628`, `:649`).
- Two saved reads. Frame 226 (the figures above): `build/work/logs/demo_owner_world_2026-11-18_f226.json`, saved
  from the lead's read that returned at 19:40:56 UTC (PowerShell `ConvertTo-Json -Depth 2`, so arrays
  below depth 2 are flattened to strings; it also holds the per-frame history from tick 0). Frame 228:
  `build/work/logs/demo_owner_world_2026-11-18.json`, captured 11:42:01 local (19:42:01 UTC): tick 13,680, 1,000,166 alive,
  397.6 ms per tick, 4,383.3 ms per picture, 28.5 ms PNG, named stages 275.3 ms, hash before and after
  drawing equal (`9b5d728547e13441`), units 21 to 48.

---

## 3. Rules every task here keeps

| Law or rule | Source | What it means for this work |
|---|---|---|
| D7 the viewer is an observer | `docs/ARCHITECTURE.md:192-194` | The stream and every viewer read the world only between `az_tick` calls, through `const AzWorld *`. No level of detail, pacing or culling may change what the simulation computes. Snapshot cadence may depend on wall time; the world may not. Proof: state hash before and after each read, and the lab final hash equal to az_run's at several snapshot rates. |
| Deterministic, same hash | `docs/agents/COMMON.md:27-28`, `docs/handoff/CONTINUE-BUILD.md:43` | No change in this document may change a reference hash, except where a task says "hash changes by design" and records the cause in `tests/golden_hashes.txt`. Every speed change shows the reference hashes before and after and passes the determinism matrix. |
| Native only | `docs/agents/COMMON.md:16-18` | Python and JavaScript only as development tooling and for the viewer page, never in creature or world code. The browser viewer is outside the simulation. The native viewer (RV-B) is C. |
| Floats only outside the hashed state | `docs/handoff/CONTINUE-BUILD.md:29` (last sentence), `view/az_view.h:1` | The viewer and the stream extractor may use floats (hosted C). Nothing they compute flows back into the world. |
| Cache first, locked arena, no pagefile | `docs/agents/COMMON.md:19-21`, `docs/handoff/CONTINUE-BUILD.md:30` | Every reported run shows the arena locked. `--no-lock` and `--allow-pageable` are diagnostics only. The stream must not create a pagefile-backed shared-memory section for state; the stream buffers are viewer data in ordinary heap memory, never world state. |
| Wild by default | `tools/az_live.c:16-20` | The stream and viewer work in wild mode by default; lab mode for tests. |
| No downloads, no installs | `docs/agents/COMMON.md:103-105` | No three.js, no npm packages, no CDN. WebGL2 by hand. The native viewer uses system DLLs that already exist (d3d11, dxgi, d3dcompiler_47) and MinGW headers. |
| No servers, browsers or windows started by the agent | `docs/agents/COMMON.md:106-107`, `docs/handoff/CONTINUE-BUILD.md:36` | The agent builds the viewer; the owner opens it. Every automatic test is headless: `socket.socketpair()` instead of a listening server, `node` for pure JavaScript functions, a D3D11 WARP device rendering off screen. See PB-14 and decision O-3. |
| Honest evidence | `docs/agents/COMMON.md:32-34`, `docs/handoff/CONTINUE-BUILD.md:39-40` | Every timing names the command, the output tail and the machine load ("idle", "lightly loaded" or "noisy, parallel load"). Smoothed motion in the viewer is labelled as smoothed. |
| No theatre in the viewer either | `docs/handoff/CONTINUE-BUILD.md:26` (spirit) | Every visual element maps to a world quantity: no invented particles, plumes or motion. Interpolation between ticks is a display choice, with a "raw ticks" switch. |
| Never weaken a check | `docs/handoff/CONTINUE-BUILD.md:33` | New pacing is opt-in so that `tests/cli/live_smoke.py:99` (frame count with `--every 5`) keeps passing unchanged. |
| ASCII, LF, no tabs | `build/work/scripts/check_text.py` | All new files. PowerShell `Set-Content` writes CRLF: write files with the editor tools. |
| Path rules | `docs/agents/COMMON.md:84-94` | Quote `$A`; `-LiteralPath`; no `>` redirection (pipe to `Set-Content -LiteralPath`); scripts as `[scriptblock]::Create(...)`; Python `glob.escape`; CMake through `AZ_GLOB_ROOT`. |

**Python `-I` and imports.** The demo and the tests run Python with `-I` (isolated mode). In isolated mode
the script's own folder is not on `sys.path`, so `demo/live_server.py` cannot `import` a sibling module, and a
test cannot `import live_server`. Keep server code in `live_server.py`, and load it in tests by path with
`importlib.util.spec_from_file_location`.

**Running az_live from an agent shell.** `az_live` reads commands only when stdin is a pipe, and the end of the
pipe means quit (`tools/az_live.c:26-28`, `:237-243`, `:464`). An agent harness may give programs a pipe that
is already at its end: `az_live` then quits at once. Measure from Python with `stdin=subprocess.DEVNULL`
(the null device is not a pipe, so `az_live` runs until `--max-ticks`), or with a pipe you keep open, as
`tests/cli/live_smoke.py` does.

---

## 4. Order of work and milestone map

| Order | Tasks | Milestone | Can start | Why this order |
|---|---|---|---|---|
| 1 | PQ-0, PQ-1 | 6 (W3 viewer) | now | measure before changing anything |
| 2 | PQ-7, PQ-3, PQ-6 | 6 | after PQ-1 | small, hash-neutral, owner-visible at once |
| 3 | PQ-4, PQ-5, PQ-2, PQ-8 | 6 | after PQ-1 | the picture path fast and decoupled |
| 4 | RV-A1 to RV-A9 | 6 | after PQ-2 (shares the pacing code) | the real fix of the owner's complaint |
| 5 | ES-0, ES-15 | 6 / 8 | now | baseline, missing BENCH.md |
| 6 | W2: sense, physics (with its NASM twin, ES-11), stats | 5 | in progress | the real world must exist before M2 |
| 7 | ES-1 (M2), then ES-2 to ES-13 (M3) | 6 (track M) | after W2 | M2 needs the real world (`docs/handoff/STATUS_AND_PLAN.md:676`) |
| 8 | RV-B1 to RV-B6 | 6 (W3) | after RV-A9 | for millions of creatures |
| 9 | ES-14 (E3) | 8 | after A1 (`docs/handoff/STATUS_AND_PLAN.md:691`) | the measured table the gate needs |
| 10 | SC-1 to SC-6 | 9 | after the foundation gate and the owner's yes on the law-2 wording (`docs/handoff/CONTINUE-BUILD.md:61`) | scale-out |

**Recorded deviation.** `docs/handoff/CONTINUE-BUILD.md:52-53` orders milestone 5 (W2) before milestone 6. The
PQ and RV-A tracks touch only `tools/az_live.c`, `view/`, `demo/`, `tests/cli/`, `tests/test_view.c` and new
files; they change no world hash and no core file. Starting them before W2 ends is a deviation driven by the
owner's words of 2026-11-18 ("It's really slow", P51). Record it in `build/work/PROGRESS.md` as a decision with
that quote. Do not start ES work (core files) before W2 ends, except ES-0 and ES-15.

---

## 5. Track PQ: quick wins for the picture demo

All PQ tasks keep every reference hash. None touches `core/`.

### PQ-0 Measure the picture demo as it is

- Milestone 6. Effort S. Depends on: nothing.
- Inputs: `tools/az_live.c` (JSON fields `ms_tick`, `ms_render`, `ms_png`, `live`, `drawn`, `marched`,
  `stages`; `:586-618`).
- Create: `$A\build\work\perf\pq0\measure_live.py` (scratch script, not a project file). It starts
  `az_live.exe` with `subprocess.Popen(args, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE)`, reads the JSON
  lines, and writes medians per configuration to `$A\build\work\perf\pq0\summary.txt`.
- Configurations: lab seed 42; founders 5,000 and 20,000; sizes 640x360, 960x540, 1280x720; `--every 1` and
  `--every 10`; `--fps 60` (so the throttle does not bind); `--max-ticks 60`. Each run's frames go to its own
  folder, created first with `[IO.Directory]::CreateDirectory(...)` in PowerShell or `os.makedirs` in Python.
  Example of one run's arguments:
  `"$A\build\z\az_live.exe" --dir "$A\build\work\perf\pq0\f_20k_960" --lab --seed 42 --n 20000 --size 960x540 --every 1 --fps 60 --max-ticks 60`
- Command: `python -I "$A\build\work\perf\pq0\measure_live.py"`
- The 1M data point to re-measure against: the owner's own world,
  `build/work/logs/demo_owner_world_2026-11-18_f226.json` (section 2.5, frame 226: 414.3 ms per tick, 4,261.3 ms per
  1600x900 picture, 27.5 ms PNG at 1,000,416 creatures; frame 228 in `build/work/logs/demo_owner_world_2026-11-18.json`). Add one configuration with its options: wild,
  `--n 250000 --species 100 --cap 1004096 --size 1600x900 --every 60 --fps 30 --keep 10 --threads 0`, run in
  the background (`00-MASTER-PROMPT.md` 3.5) until `live` first passes 1,000,000 (the owner's run took about
  70 minutes, 10:31 to 11:41 local, to reach tick 13,560), then record the same medians over the next 5
  pictures. It locks about 8.8 GB (`arena_mb` 8,773.4): run it only when no other locked world runs. A wild run
  does not repeat exactly, so compare medians and the population, not hashes. Never read, stop or restart the
  owner's own demo for this.
- DoD (can fail): `summary.txt` holds, for each configuration, the median and the maximum of `ms_tick`,
  `ms_render`, `ms_png`, the median `live`, `drawn`, `marched`, and the measured pictures per second and ticks
  per second (wall clock). If any run ends with exit code other than 0 or `observer_fail` other than 0, the task
  fails and the failure is recorded.
- Evidence: PROGRESS.md entry (section 13.1) with the table and the machine load; a copy of one raw JSON line
  per configuration.
- Risk: the frames fill the disk (1.5 MB each, `--keep` default 8 limits it, `tools/az_live.c:312`).

### PQ-1 Timings for every part of a picture

- Milestone 6. Effort S. Depends on: PQ-0.
- Change `tools/az_live.c`: add to each frame line `ms_hash` (both `az_state_hash` calls together),
  `ms_encode` (`az_png_encode` alone), `ms_write` (open, write, close), `ms_stats` (`az_stats_collect` plus the
  slot walk at `:564-584`), `ms_period` (wall time since the previous frame line). Keep `ms_png` as
  `ms_encode + ms_write` so existing readers still work.
- Change `tests/cli/live_smoke.py`: one new check that every frame line has the five fields, each a number
  >= 0, and that `ms_png` equals `ms_encode + ms_write` within 0.02 ms of rounding.
- Commands: build `cmake --build "$A\build\z"`; test `ctest --test-dir "$A\build\z" -R cli_live_smoke --output-on-failure`;
  then rerun PQ-0.
- DoD: `cli_live_smoke` passes with 22 checks (21 + 1); PQ-0 rerun shows the share of each part per
  configuration; the 100k-class cost of `ms_hash` is known (run one configuration with `--n 100000 --cap 400000 --grid 6,6,5 --every 10 --max-ticks 30`).
- Evidence: PROGRESS.md entry; the `summary.txt` of the rerun.
- Laws: observer check unchanged (the hashes are still taken and compared).

### PQ-2 Free pacing: the world does not wait for pictures

- Milestone 6. Effort M. Depends on: PQ-1, PQ-5 (without the render thread, free pacing only moves the waiting
  around; see "Expected").
- Change `tools/az_live.c`:
  - new option `--pace picture|free` (default `picture`: today's behaviour, so `tests/cli/live_smoke.py:99`
    still holds);
  - new option `--tps T` (target ticks per second, 0 = as fast as possible; default 0) and `--pps P` (pictures
    per second at most, 1 to 60; in free pace it replaces `--fps`);
  - new commands `pace free|picture`, `tps T`, `pps P`;
  - in free pace: tick one tick at a time; after each tick, if `tps > 0`, wait until `t0 + k / tps` with a
    high-resolution waitable timer (`CreateWaitableTimerExW` with `CREATE_WAITABLE_TIMER_HIGH_RESOLUTION`;
    do not call `timeBeginPeriod`); if a picture is due and the render thread (PQ-5) is idle, capture the scene
    between ticks and hand it over; print statistics at most 4 times per second;
  - each frame line gains `pace`, `tps_target`, `ticks_since_prev`, `tps_measured`.
- Change `demo/live_server.py`: pass `--pace free --tps T --pps P` from the page; extend `valid_command`
  (`:678-680`) with `pace (free|picture)`, `tps \d{1,5}`, `pps \d{1,2}`.
- Change `demo/live.html`: replace "Ticks per picture" and "Pictures per second (max)" by "World speed (ticks
  per second, 0 = as fast as it can)" and "Pictures per second"; keep "Ticks per picture" in an "Exact steps"
  box for `--pace picture`. Show measured ticks per second and pictures per second side by side.
- Change `tests/cli/live_smoke.py`: new checks (keep all old ones):
  1. `--pace free --lab --seed 7 --n 2000 --max-ticks 200 --pps 5` exits 0 and its final hash equals
     `az_run --lab --seed 7 --n 2000 --ticks 200` with the same options;
  2. the same with `--pps 30` and with `--tps 50` gives the same hash;
  3. with `--tps 0` the mean `ticks_since_prev` over the run is above 1 (the world ran ahead of the pictures);
  4. with `--tps 20` the measured ticks per second is between 14 and 26 (a loose band; the test must not be
     flaky on a loaded machine; if it is, widen nothing silently: record the observed spread and decide).
- DoD: all new and old checks pass; PQ-0's 20k/960x540 configuration in free pace with `--tps 0 --pps 4`
  shows ticks per second at least 0.8 times az_run's for the same world (az_run measured in the same session).
- Expected: without PQ-5, a 0.48 s picture at 2 pictures per second leaves the world about 4 percent of the
  wall time (estimate), so PQ-2 alone does not help; with PQ-5 the world loses only the capture time.
- Risks: wall-clock pacing leaking into the world (D7). Held by checks 1 and 2.
- Laws: D7 (snapshot timing only), determinism (lab hash equality).

### PQ-3 A faster PNG writer, and an optional raw picture

- Milestone 6. Effort S. Depends on: PQ-1.
- Change `view/az_png.c`: CRC-32 with a 256-entry table (one lookup per byte) instead of two 16-entry lookups
  per byte (`:14-27`); compute the CRC and Adler-32 over the finished buffer with word-wide loops; write the
  stored blocks row by row with `memcpy` of packed RGB rows instead of one byte per loop step (`:92-116`).
  Keep stored deflate: `tests/test_view.c:3-6` parses stored blocks with an independent reader that shares no
  code with the writer, so compressed deflate in C would need a second, independent inflater in the test.
- PQ-3b (optional): `az_live --picture raw` writes `frame-NNNNNNNN.rgba` (w, h, then BGRA rows); the page
  draws it with `putImageData` into a canvas. No encode, no decode.
- Compression in the server (optional, low value on 127.0.0.1): the server may recompress with Python's
  `zlib` level 1 if a later measurement shows transfer time matters. Do not do it by default.
- Tests: `tests/test_view.c` must pass unchanged (its known answers for CRC-32 and Adler-32 and the round trip
  catch a wrong table).
- DoD: `test_view` passes; `ms_encode` at 960x540 at most 0.3 times its PQ-1 value (goal; record the real
  ratio).

### PQ-4 Paint with several threads

- Milestone 6. Effort M. Depends on: PQ-1.
- Change `view/az_view.c`, `view/az_view.h`:
  - new `void az_view_render_mt(AzView *v, const AzWorld *w, u32 n_threads)`; `az_view_render` calls it with 1;
  - split the picture into `n_threads` bands of rows. Each band thread runs `background` for its rows,
    `world_box` with lines clipped to its rows, and `bodies` with every body's pixel loops clipped to its rows.
    The overlay runs once, after the join;
  - before the threads start, run `census` and build every shape that `bodies` will need (`shape_get` writes
    `v->shape`, `view/az_view.c:513-525`, and is not thread-safe). The pre-pass uses the same test as
    `bodies` (`!passive && rpx >= 1.6f && c < VIEW_MAX_CLADES`, `view/az_view.c:792-793`);
  - `n_drawn` and `n_marched` must equal the single-thread values: a body that touches two bands counts once.
    Keep one bit per slot per band and OR them after the join, or count only in the band that holds the body's
    centre row (choose one and document it).
  - Why the pixels stay identical: `fragment` keeps the nearer depth and on a tie the lower uid
    (`view/az_view.c:617-619`), a total order that does not depend on drawing order; each pixel's arithmetic is
    the same code with the same inputs in every band.
  - Threads: Windows `CreateThread` and an event per band, created once per `AzView` (hosted C; the view is
    Windows-only like the rest).
- Change `tools/az_live.c` and `tools/az_snap.c`: option `--view-threads N` (default 1 in az_snap, 8 in
  az_live); az_snap prints the view hash.
- Tests: `tests/test_view.c` gains a part "5. Threads": on 3 lab worlds x 3 cameras (default, close-up with
  `dist` 8, side view pitch 0.05) the pixel hash `az_view_hash` and every `AzViewInfo` field are equal for
  1, 2, 3, 8 and 13 threads.
- DoD: test passes; `ms_render` at 960x540 with 8 view threads at most 0.3 times the 1-thread value on the 20k
  world (goal; record the real ratio).
- Risks: float differences between compilations of two code paths. Avoid by keeping one function per pixel
  operation and passing the band limits as arguments, not by duplicating code.

### PQ-5 Capture the scene, paint on the side

- Milestone 6. Effort M. Depends on: PQ-4.
- Change `view/az_view.c`, `view/az_view.h`:
  - new `AzViewScene` (owned by the view): copies of exactly what `az_view_render` reads: grid sizes and
    shifts, `tick`, `n_slots`, and per slot `flags`, `clade`, `uid`, `pos_x/y/z`, `mass`, `rot_w/a/b/c`, the
    112 `trait` bytes; per cell `c_light` and the `AZ_SP_ORG_Y` amount; the clade bodies
    (`w->clades[c].body`, `n_clades`). Check the list against every `w->` read in `view/az_view.c` and keep a
    comment that lists them;
  - `bool az_view_capture(AzViewScene *s, const AzWorld *w)` (grows its buffers as needed; returns false on
    out of memory, never crashes) and `void az_view_render_scene(AzView *v, const AzViewScene *s, u32 n_threads)`;
  - `az_view_render(v, w)` becomes capture + render_scene into a scene owned by the view.
- Change `tools/az_live.c`: a render thread that owns the `AzView`, renders a captured scene, encodes and
  writes the picture and returns the timings; the main thread keeps ticking. The JSON line is printed by the
  main thread only (one writer on stdout). The observer hash is taken before and after the capture.
- Tests: `tests/test_view.c` part "6. Scene": `az_view_render_scene(capture(w))` gives the same pixel hash as
  rendering the world directly, on 3 worlds and 2 cameras; the scene does not change when the world ticks after
  the capture (render after one more tick, same hash as before it).
- DoD: tests pass; in free pace (PQ-2) at 20k founders the capture costs at most 10 percent of one tick (goal).
- Estimate of the copy: about 150 bytes per slot (the 112 trait bytes dominate) plus about 33 bytes per cell:
  about 3 MB at 20k slots, about 37 MB at 250k slots.

### PQ-6 Show every picture, not 4 per second

- Milestone 6. Effort S. Depends on: nothing.
- Change `demo/live_server.py`: new `GET /stream/<run>` that answers `multipart/x-mixed-replace; boundary=azframe`
  and writes each new frame once as a part (`Content-Type: image/png`, `Content-Length`), waiting on a
  condition variable that the stdout reader signals. Same Host check as every GET (`:568-569`, `:591-593`).
  End the response when the run changes or the client goes away.
- Change `demo/live.html`: the viewer `<img>` uses `/stream/<run>`; the 250 ms poll stays for the numbers.
  Fallback when the stream fails: poll pictures every 100 ms.
- Test: `tests/cli/ws_selftest.py` (RV-A5) also drives the multipart writer over `socket.socketpair()` and
  checks the boundary and part lengths.
- DoD: test passes; in the owner's browser the "pictures shown per second" counter (new, in the page) equals
  the engine's pictures per second within 10 percent (owner check, decision O-3).

### PQ-7 Use the assembly kernels and the E cores in the demo

- Milestone 6. Effort S. Depends on: nothing.
- Change `tools/az_live.c`: options `--ecores` and `--no-pin`, mapped to `AZ_WIN_USE_ECORES` and
  `AZ_WIN_NO_PIN` (`platform/win/az_host_win.h:13-14`) as az_run does (`tools/az_run.c:282-283`); print them in
  the start line.
- Change `demo/live_server.py`: pass `--kernels 1` by default and `--ecores --no-pin` when asked
  (`world_options` gains `ecores`).
- Change `demo/live.html`: two checkboxes in "New world": "Assembly kernels (same world, faster)" on by
  default, "Use E cores too" off by default.
- Tests: `tests/cli/live_smoke.py` new check: `--lab --seed 7 --n 2000 --max-ticks 20 --kernels 1 --ecores
  --no-pin --threads 24` ends on the same hash as without these options and as az_run's.
- DoD: test passes; the start line shows `workers` 24 with `--ecores --threads 24`.
- Laws: the determinism matrix already proves kernels and E cores leave the hash unchanged
  (`docs/handoff/CONTINUE-BUILD.md:43`); the new check proves it for az_live.

### PQ-8 A fast view mode

- Milestone 6. Effort S. Depends on: PQ-4.
- Change `view/az_view.c`: `az_view_mode(v, AZ_VIEW_FULL | AZ_VIEW_SPHERES)`; in spheres mode every body is a
  shaded sphere (`draw_sphere`), no ray-march.
- Change `tools/az_live.c`: option and command `view full|spheres`; the JSON line names the mode.
- Change `demo/live.html`: a "Body detail" select.
- Laws: this is a level of detail in the viewer, not in the simulation; D7 forbids it only for the simulation.
  State it in the page's About text.
- DoD: `test_view` part "7. Modes": spheres mode gives `n_marched == 0` and a different pixel hash; full mode
  unchanged; measured `ms_render` ratio recorded.

### PQ-9 Defaults that fit the machine

- Milestone 6. Effort S. Depends on: PQ-2 to PQ-8.
- Change `demo/live.html` and `demo/live_server.py` defaults after measuring: picture size 960x540, free pace,
  `--tps 0`, 4 pictures per second, 8 view threads, kernels 1. Show a one-line explanation under the controls:
  "The world runs at its own speed. Pictures are drawings of it; the real-time view (when built) is smoother."
- DoD: a fresh start of the page with no settings touched shows measured ticks per second at least 0.8 times
  az_run's for the same founders (owner check or the PQ-0 script with the server's arguments).

---

## 6. Track RV-A: the real-time viewer in the browser

### 6.1 Shape of the design

```
 az_live (C, one process)                           live_server.py (Python stdlib)          browser (WebGL2)
 +------------------------------------+   stdout   +-----------------------------+   ws    +------------------------+
 | main thread: az_tick loop          |  framed    | reader thread: frames ->    |  binary | decode header          |
 |   between ticks, when due:         |  binary    |   latest CREATURES buffer   |  push   | upload records as-is   |
 |   - h0 = az_state_hash (sampled)   |----------->|   latest FIELDS buffer      |-------->| uid merge -> prev pos  |
 |   - extract creatures (uid order)  |            |   STATIC, SHAPES, JSON      |         | instanced bodies       |
 |   - extract fields (rarely)        |            | writes "ack N" to stdin     |         | terrain, ocean, vents  |
 |   - h1 = az_state_hash (sampled)   |<-----------| /ws: latest-wins per client |         | suns, HUD, picking     |
 | writer thread: writes one frame    |   stdin    | POST /api/cmd unchanged     |<--------| commands via POST      |
 |   at a time (double buffer)        | commands   | /api/snap long-poll fallback|  POST   | 60 FPS, interpolated   |
 +------------------------------------+            +-----------------------------+         +------------------------+
```

- The world runs flat out on its own clock (`--tps 0`) or at a target rate (`tps N`), with pause and step.
- The stream reads the world only between ticks, on the main thread, through `const AzWorld *`.
- The writer thread never touches the world; it only writes a finished buffer.
- At most one creature snapshot is in flight (credit flow, section 6.4). Every buffer is latest-wins: a slow
  reader gets fewer snapshots, never a growing queue, and never slows the world.
- Commands stay on `POST /api/cmd` (`demo/live_server.py:635-639`); the WebSocket carries only engine to page.
- The old picture path stays: the command `picture` asks for one engine-drawn PNG of the same moment and camera
  (the proof view, RV-A8).

### 6.2 Message formats (stream version 1)

All integers are little-endian. Every message on az_live's stdout in `--stream` mode is a frame:

| Offset | Type | Field |
|---|---|---|
| 0 | 4 bytes | magic: the bytes `41 5A 53 54` ("AZST") |
| 4 | u16 | message type (below) |
| 6 | u16 | stream version, 1 |
| 8 | u32 | payload length in bytes (at most 2^31 - 1) |
| 12 | u32 | sequence number, counted per message type from 0 |
| 16 | payload | |

Message types:

| Type | Name | When | Payload |
|---|---|---|---|
| 1 | JSON | start, events, statistics (at most 4 per second), inspect replies, errors | one UTF-8 JSON object, no newline; the same objects az_live prints today plus `{"event":"inspect",...}` |
| 2 | CREATURES | when a snapshot is due and the previous one was acknowledged | header (64 bytes) then `n` records of 24 bytes, ordered by uid, strictly increasing |
| 3 | STATIC | once after start and after a load | world geometry, vents, sun periods, bedrock, ground heights |
| 4 | FIELDS | at most once per second, and only on ticks where `tick % field_stride == 0` | water heights per column, 4 bytes per cell |
| 5 | SHAPES | when a clade appears that was not sent before | body-plan tables of new clades |
| 6 | PICTURE | answer to the `picture` command | a PNG made by az_view_render (the proof view) |

**CREATURES header (64 bytes)**

| Offset | Type | Field |
|---|---|---|
| 0 | u64 | tick of the world when read |
| 8 | u64 | state hash before the read, 0 if not checked on this snapshot |
| 16 | u64 | state hash after the read, 0 if not checked |
| 24 | u32 | n, number of records (= living creatures) |
| 28 | u32 | record size in bytes, 24 in version 1 |
| 32 | u8 x 4 | gx_bits, gy_bits, gz_bits, mode (0 lab, 1 wild) |
| 36 | u32 | ticks since the previous snapshot |
| 40 | u32 | mean microseconds per tick over those ticks (saturated at 2^32 - 1) |
| 44 | i8 x 4 | light direction at the top cell of column (0, 0): `c_lvec` of that cell (`core/az_cells.def:50`) |
| 48 | u32 | n_clades (`w->n_clades`, `core/az_abi.h:1002`) |
| 52 | u32 | observer checks failed so far |
| 56 | u32 x 2 | reserved, 0 |

**CREATURES record (24 bytes)**

| Offset | Type | Field | From |
|---|---|---|---|
| 0 | u32 | uid, low 32 bits | `uid` (u64, `core/az_columns.def:55`) |
| 4 | u32 | uid, high 32 bits | |
| 8 | u16 | x = `pos_x >> 16` | `pos_x` spans the world: `shift_x = 32 - gx_bits` (`core/az_world.c:305`) |
| 10 | u16 | y = `pos_y >> 16` | |
| 12 | u16 | z = `pos_z >> 16` | |
| 14 | u8 | yaw of the body's forward axis, 0..255 for 0..2 pi | rotor applied to the unit x axis with `az_rotor_apply`, as `view/az_view.c:677-685` does |
| 15 | i8 | pitch of the forward axis, -127..127 for -pi/2..pi/2 | same |
| 16 | u8 | size code `az_radius_code(mass)`, 0..15 (`core/az_abi.h:1152`) | `mass` |
| 17 | u8 | hue = `tag >> 8` | `tag` = (HUE << 8) plus PATTERN (`core/az_columns.def:65`) |
| 18 | u8 | pattern = `tag & 255` | |
| 19 | u8 | flags `AZ_F_*` (`core/az_abi.h:69-76`) | `flags` |
| 20 | u32 | clade index | `clade` (`core/az_columns.def:63`) |

Decoding: the position in cells is `x * gx / 65536` (and the same for y and z). At gx = 64 that is 1/1024 cell
of resolution, at gx = 128 1/512 cell. x and y wrap like the world: interpolate with the shortest signed
difference modulo 65536. Records are in uid order so that two snapshots can be matched by a linear merge.

Extraction in C: walk the slots, skip those without `AZ_F_ALIVE`, fill records, then sort by uid (LSD radix
sort, 16 bits per pass; when every high word is 0, two passes over the low word suffice). Goal: at most 5 ms
for 250,000 creatures on one thread (to be measured in RV-A3).

**STATIC payload**

| Offset | Type | Field |
|---|---|---|
| 0 | u32 x 3 | gx, gy, gz |
| 12 | u32 | n_cells |
| 16 | u32 | n_vents (cells listed below; at most 64, `core/az_abi.h:1034`) |
| 20 | u32 x 2 | sun periods in ticks (`w->sun_period`, `core/az_abi.h:1031`) |
| 28 | u32 | field_stride (`cfg.field_stride`) |
| 32 | u32 x n_vents | vent cell indices (`w->vent_cell`) |
| then | u8 x n_cells | bedrock fraction `c_solid` (`core/az_cells.def:41`); cell index = x + gx * (y + gy * z), as `view/az_view.c:341` |
| then | u16 x (gx * gy) | ground height per column, in 1/256 cell: `(zt + 1) * 256` where zt is the highest z with `c_solid == 255`, plus `c_solid` of the cell above scaled to 0..255; 0 if the column has no rock |

`c_solid` is written only by worldgen (`core/az_cells.def:41`), so STATIC is sent once per run and once after
a `--load`.

**FIELDS payload**

| Offset | Type | Field |
|---|---|---|
| 0 | u64 | tick |
| 8 | u32 x 3 | gx, gy, gz |
| 20 | u32 | parts present: bit 0 water heights, bit 1 cells |
| 24 | u64 | reserved, 0 |
| 32 | u16 x (gx * gy) | water height per column in 1/256 cell: `(zw + 1) * 256` where zw is the highest z whose solvent phase is liquid (`c_phase & 3 == AZ_PH_LIQUID`, `core/az_abi.h:350`) and whose solvent amount `c_sp[cell * 8 + AZ_SP_SOLV]` is above 0; 0 = no water |
| then | 4 bytes x n_cells | per cell: `c_phase & 255` (phases of species 0..3), `c_light`, temperature `min(255, c_temp >> 7)`, organic `min(255, 16 * bit_length(c_sp[ORG_Y]))` |

**SHAPES payload:** u32 count, then per clade: u32 clade index, u8 body[16] (`AzClade.body`,
`core/az_abi.h:589-590`), u16 elong x 256, u16 spike_amp x 256, u16 gmax x 256, then u8 base[48 x 48] and
u8 spike[48 x 48] (the octahedral radius map of `shape_build`, `view/az_view.c:470-511`, quantised: base
0.35..1.8 to 0..255, spike 0..1 to 0..255). The table is computed by a new exported function in the view,
`void az_view_shape_table(const u8 body[16], AzShapeTable *out)`, so the browser and the engine view share one
definition of the body plan. Known difference: the engine view also shifts hue, saturation, glow, sheen,
pattern and spike size by each creature's own appearance traits (`view/az_view.c:582-597`, `:669-671`); the
stream carries only the clade baseline and the tag. The page's About text says so.

**JSON additions:** `{"event":"inspect","uid":"<16 hex>","found":true,"tick":T,"slot_valid_this_tick":S,
"genes":[a,b],"neurons":H,"tiles":K,"parts":P,"units":U,"energy":E,"mass":M,"health":HL,"clade":C,
"lineage":L,"generation":G,"parent_a":"<hex>","parent_b":"<hex>","age":A,"colony":CO,"flags":F,
"pos":[x,y,z]}`, read with the same calls as the slot walk at `tools/az_live.c:569-583` plus the columns in
`core/az_columns.def`. `"found":false` when the uid is not alive.

**New stdin commands in `--stream` mode:** `ack N` (from the server only), `sps S` (snapshots per second
1..60), `tps T`, `pace free|picture`, `inspect UID` (16 hex digits), `picture` (one PNG with the current
camera), `camera YAW PITCH DIST` (the proof view's camera), `observer-ms M` (minimum time between full
observer checks), plus every existing command.

### 6.3 Bandwidth

Creature snapshot size = 16 (frame) + 64 (header) + 24 x n bytes.

| Living creatures | Bytes per snapshot | At 10 per second | At 15 per second | At 30 per second |
|---|---|---|---|---|
| 10,000 | 0.24 MB | 2.4 MB/s | 3.6 MB/s | 7.2 MB/s |
| 100,000 | 2.4 MB | 24 MB/s | 36 MB/s | 72 MB/s |
| 250,000 | 6.0 MB | 60 MB/s | 90 MB/s | 180 MB/s |
| 1,000,000 | 24 MB | 240 MB/s | 360 MB/s | 720 MB/s |
| 1,500,000 | 36 MB | 360 MB/s | 540 MB/s | 1,080 MB/s |

Fields at 64 x 64 x 32: STATIC about 140 KB once; FIELDS about 520 KB at most once per second. At
128 x 128 x 32: about 0.55 MB once and about 2.1 MB per second.

Interpretation: the 100k reference world reaches 227,469 living creatures within 60 ticks
(`build/work/logs/wg_ref100k_t8.txt:7`), so plan for 250,000 at 15 snapshots per second: 90 MB/s through a
pipe, Python and a local WebSocket. Python must never touch records one by one: it reads with `readinto` into
a preallocated `bytearray` and sends `memoryview` slices. Above about 250,000 creatures, Design A sends fewer
snapshots per second (the page interpolates longer) or a uid-hash subsample for display only (a viewer choice,
never a simulation one), and Design B (section 7) takes over.

The owner's own world (section 2.5) is the 1,000,000 row at the world's own rate: one snapshot per tick is
about 2.4 snapshots per second, about 58 MB/s (24,010,064 bytes x 2.4), less than the 90 MB/s planned for
250,000 creatures at 15 per second. More snapshots per second than ticks per second carry nothing new. RV-A9
target case 2 tests this world.

### 6.4 Flow control and pacing

- Credit flow for CREATURES: az_live keeps `inflight` = 0 or 1. It extracts a snapshot only when `inflight` is
  0, sets it to 1, and clears it when `ack N` for that sequence number arrives on stdin. The server writes
  `ack N` after it has read the whole frame. When stdin is not a pipe (console run, tests writing to a file),
  az_live does not wait for acks.
- Double buffer: the main thread fills buffer B between ticks and hands it to the writer thread; the writer
  writes it to stdout with `WriteFile` in a loop; if the writer is still busy when a snapshot is due, that
  snapshot is skipped (latest-wins). The world never blocks on a full pipe.
- Small frames (JSON, inspect) go through the same writer thread queue (a short fixed ring of 64 entries; a full
  ring drops statistics lines first, never events, and counts drops in the next statistics line).
- Pacing is the PQ-2 loop. Snapshot due when `now - last_snapshot >= 1 / sps`. Fields due as in 6.2.
- Pause: no ticks; the last snapshot stays on screen; `step K` runs K ticks and sends one snapshot.

### 6.5 The observer rule for the stream

- Every read of the world (snapshot, fields, inspect, picture capture) happens on the main thread between
  `az_tick` calls, through `const AzWorld *` (the compiler rejects writes through it in the extractor code).
- Full check: `h0 = az_state_hash(w)` before the read and `h1` after; a difference counts as an observer
  failure, is reported in every later CREATURES header (offset 52) and in the statistics, and makes az_live exit
  with code 1 at the end (as today, `tools/az_live.c:635`).
- Cost: the state hash covers the creature store (`core/az_abi.h:1346`), about 2.7 KB per creature
  (`build/work/logs/wg_ref100k_t8.txt:10`), so two hashes per snapshot may cost more than the read. PQ-1
  measures it. The option `observer-ms M` sets the minimum time between full checks: 0 = every read (the
  default for every test and for `--lab` runs), 500 = at most two checks per second (the server's default in
  wild mode). The first and the last read of a run are always checked. The HUD shows "observer checks: passed
  P, failed F, last at tick T".
- This is not a weakened check: the existing picture check stays on every picture; the ctest gate checks every
  snapshot; the sampled mode exists only for the interactive wild demo and is named in the HUD.
- Optional stronger and cheaper proof (RV-A2c): during each read, set the arena read-only with
  `VirtualProtect(PAGE_READONLY)` and restore it after; a stray write then faults. Measure the cost on a locked
  arena first, check that the pages stay locked (`GetProcessWorkingSetSize` and the lock flag before and after),
  and keep the hash checks anyway.
- Lab proof that pacing does not leak into the world: final hash equal to az_run's at `sps` 1, 15 and 60 and
  `tps` 0 and 30 (RV-A3).

### 6.6 The page

Files: `demo/viewer_core.js` (pure functions, no DOM, no WebGL: decoding, merge, camera math), `demo/viewer_gl.js`
(WebGL2), changes to `demo/live.html` (a "View" switch: "Real-time (GPU)" default, "Engine pictures (proof)").
No libraries, no internet. The server serves the two JS files from a fixed allowlist.

- **Bodies.** One instanced draw per level of detail. The 24-byte records are uploaded as they arrive
  (`bufferData` with `STREAM_DRAW` on the ArrayBuffer slice; no JavaScript loop over records) and read in the
  vertex shader with `vertexAttribIPointer`: uvec3 position (offset 8), yaw and pitch (offset 14), size, hue,
  pattern, flags (offset 16), clade (offset 20). A second per-instance buffer holds the previous position of
  each record, built by `viewer_core.mergePrev(prevUids, prevPos, newUids)`, a linear two-pointer merge over
  the uid-ordered arrays (newborns get their own position and fade in over 200 ms; the dead simply vanish).
  Near bodies: an icosphere of 42 vertices and 80 triangles, displaced in the vertex shader by the clade's
  shape tile (the SHAPES tables in one 4096 x 4096 RG8 atlas of 48 x 48 tiles: 7,225 tiles; least recently
  used tiles are reused; a clade without a tile is drawn as a sphere and counted in the HUD). Far bodies
  (projected radius under 3 px): a camera-facing quad with a sphere shaded in the fragment shader. The vertex
  shader collapses the instance in the pass it does not belong to.
- **Interpolation.** `alpha = clamp((now - arrival_of_newest) / interval_between_arrivals, 0, 1)`; positions
  mix from previous to newest. The display runs one snapshot interval behind the world. A "Raw ticks" switch
  sets alpha to 1 so the owner sees the true steps. The HUD says "smoothed between ticks" when smoothing is on.
- **Terrain.** A heightfield mesh of (gx + 1) x (gy + 1) vertices from the ground heights (STATIC), wrapping in
  x and y, shaded by height and by the organic amount of the top cell (FIELDS). Option "Voxels": instanced
  cubes for cells with `c_solid == 255`.
- **Ocean.** A translucent surface at the water height per column (FIELDS); fragments below the local water
  height get distance fog in the solvent colour; when the camera is under water the whole view is fogged.
- **Vents.** A marker at each vent cell (STATIC) coloured by the cell's temperature (FIELDS). No invented
  plumes or particles.
- **Suns.** The light direction from the CREATURES header; a disk in the sky in that direction; the scene's
  light follows it. Sky brightness from the mean `c_light` of the top layer.
- **Camera.** Orbit mode with exactly az_view's conventions: `dir = (cos pitch cos yaw, cos pitch sin yaw,
  sin pitch)` from target to camera, z up, 40 degree vertical field of view (`view/az_view.c:18`, `:274-282`),
  distance 0 = fit the world box (port `cam_setup`, `view/az_view.c:297-317`). Fly mode: WASD, Q/E up and down,
  mouse look, speed by wheel. Follow mode: the camera tracks the selected uid.
- **Picking.** On click, draw the instances into an `R32UI` texture with instance index + 1, read one pixel,
  map it to the uid of the shown snapshot, `POST /api/cmd` with `inspect <uid>`, show the reply in a panel,
  highlight the creature, offer "Follow".
- **Overlays.** Clade legend (from the statistics line `top`), field slices (temperature, light, solvent phase)
  at a chosen z level, a grid of cells, the world box.
- **HUD.** Display FPS (median of the last 120 frames), frame time p99, snapshots received per second, world
  ticks per second (from the headers), snapshot age in ms (arrival time minus the time the tick ended, using
  the statistics clock), creatures drawn, the GPU name from `WEBGL_debug_renderer_info`
  (`UNMASKED_RENDERER_WEBGL`) when the browser exposes it, the observer counts, the stream version.
- **Measure button.** "Measure 30 s" records frame times and counters and posts them to a new
  `POST /api/perf`; the server writes `build/demo/logs/perf-<run_id>-<time>.json`.

### 6.7 Tasks

#### RV-A1 Write the stream specification

- Milestone 6. Effort S. Depends on: nothing.
- Create `docs/STREAM.md` with section 6.2 to 6.5 as the normative text, and `tools/az_stream.h` with the
  constants (magic, types, version, record size, header offsets as `enum` values) and `_Static_assert`s on the
  struct sizes.
- DoD: the header compiles in az_live; the Python and JavaScript decoders (RV-A3, RV-A6) read their constants
  from the same numbers (a test compares them by parsing `tools/az_stream.h` with a regular expression).

#### RV-A2 az_live --stream

- Milestone 6. Effort L. Depends on: RV-A1, PQ-2 (pacing loop), PQ-5 (scene capture for `picture`).
- Change `tools/az_live.c`; create `tools/az_stream.h` as header-only code (static inline functions: frame
  writer, creature extraction with the uid radix sort, STATIC and FIELDS extraction, inspect), so that no
  CMake change is needed for a new source file. Change `view/az_view.c` and `view/az_view.h`: export
  `az_view_shape_table`.
- Behaviour: as sections 6.2 to 6.5. `--stream` turns stdout into frames; without it nothing changes.
  `--sps S` (default 15), `--observer-ms M` (default 0).
- RV-A2c (optional, after RV-A3): the read-only arena during reads (section 6.5), behind `--observer readonly`.
- DoD: RV-A3 passes; a 100k lab run with `--stream --sps 15 --tps 0 --kernels 1` writes valid frames to a file
  for 60 ticks (`--max-ticks 60`) with exit code 0.
- Risks: blocking writes (held by the writer thread), memory growth (buffers sized from `cap`, allocated
  once; a failed allocation is an error line, never a crash).

#### RV-A3 Stream smoke test (ctest gate `cli_stream_smoke`)

- Milestone 6. Effort M. Depends on: RV-A2.
- Create `tests/cli/stream_smoke.py` (with a small decoder of its own, sharing no code with the server) and
  register it in `CMakeLists.txt` beside `cli_live_smoke` (`CMakeLists.txt:301-305`; you, the lead, edit
  CMakeLists).
- Checks (each one can fail):
  1. every frame starts with the magic, has version 1 and a payload length that fits the file;
  2. the first frames are JSON start, STATIC, SHAPES, then CREATURES;
  3. in each CREATURES, `n` equals the `live` of the statistics line of the same tick, uids strictly increase,
     every position decodes inside the world box, every record's flags include `AZ_F_ALIVE`;
  4. lab mode: hash before equals hash after on every snapshot (`--observer-ms 0`);
  5. lab final hash equals az_run's for `--lab --seed 7 --n 2000 --ticks 200` at `--sps 1`, `15` and `60`,
     and at `--tps 0` and `--tps 30`;
  6. wild mode runs, books exact, observer failures 0;
  7. with a pipe and no acks, at most one CREATURES frame is written after the first (credit flow); after
     `ack N` the next one comes;
  8. `inspect` of 10 uids taken from a snapshot returns `found: true` and the same position as the record
     (two read paths agree); a uid that never existed returns `found: false`;
  9. `picture` returns a PNG whose size matches the requested `size`, and whose state hash line equals the
     snapshot's;
  10. STATIC: the ground heights agree with `c_solid` re-read through az_run's snapshot (`--save` of the same
      lab world and a reader in the test), or, simpler and enough: two STATIC messages of two runs with the
      same seed are byte-identical and a different seed changes them;
  11. `quit` ends with an end JSON line and exit code 0.
- DoD: the gate passes in `ctest --test-dir "$A\build\z" -R cli_stream_smoke --output-on-failure`, and a planted
  defect is caught: build a scratch copy with the uid sort removed (check 3 must fail) and one that writes the
  previous tick's records (check 4 or 5 must fail); record both in PROGRESS.md.

#### RV-A4 Server: stream reader, WebSocket, long-poll

- Milestone 6. Effort M. Depends on: RV-A2.
- Change `demo/live_server.py`:
  - start az_live with `--stream` when the page asks for the real-time view;
  - a reader thread that reads frames with `readinto` (header, then payload into a preallocated buffer that
    grows only when needed), keeps the latest CREATURES, FIELDS, STATIC and SHAPES (all shapes so far, for
    new clients), writes `ack N` to stdin after each CREATURES, and turns JSON frames into today's state
    (`LiveRun._read_out`, `:240-277`);
  - `GET /ws`: WebSocket per RFC 6455, server-to-client binary frames only (opcode 2, FIN set, unmasked,
    7-bit, 16-bit or 64-bit length); answers ping with pong; ends on close (opcode 8, masked from the client).
    Same Host check as every request (`:568-569`) and the Origin check of POST (`:621-623`). Per client a
    sender thread that waits on a condition variable and sends the newest message of each type it has not
    sent yet (latest-wins); STATIC and all SHAPES first;
  - `GET /api/snap?type=2&after=N`: long-poll fallback (waits up to 1 s for a sequence number above N, then
    204);
  - `POST /api/perf` (section 6.6), size-limited to 1 MB, written under `build/demo/logs/`;
  - extend `valid_command` (`:678-680`): `sps \d{1,2}`, `tps \d{1,5}`, `pace (free|picture)`,
    `inspect [0-9a-f]{16}`, `picture`, `observer-ms \d{1,5}`. `ack` is never accepted from the page.
- DoD: RV-A5 passes; the page works over both transports (owner check).
- Risks: hand-written protocol code on a socket. Keep it 127.0.0.1-only (`:712`), keep the Host and Origin
  checks, cap every read from the client (close frames at most 125 bytes, any other client frame closes the
  connection).

#### RV-A5 Server self-test without a listening server (ctest gate `cli_ws_selftest`)

- Milestone 6. Effort S. Depends on: RV-A4.
- Create `tests/cli/ws_selftest.py`; load `demo/live_server.py` by path
  (`importlib.util.spec_from_file_location`), never by import (Python `-I`, section 3). Register the gate in
  `CMakeLists.txt`.
- Checks:
  1. the handshake answer for the RFC 6455 example key `dGhlIHNhbXBsZSBub25jZQ==` is
     `s3pPLMBiTxaQ9kYGzzhZRbK+xOo=` (RFC 6455, section 1.3);
  2. frame headers for payload lengths 0, 125, 126, 65535, 65536 and 6,000,064 bytes, decoded back by an
     independent decoder in the test;
  3. over `socket.socketpair()`: a handler instance answers a valid upgrade with 101, refuses a wrong Host with
     403 and a foreign Origin with 403; a masked close frame from the "client" ends the session; an unmasked
     client frame closes it;
  4. latest-wins: three CREATURES posted while the "client" does not read reach it as at most two (the first
     in flight and the newest);
  5. the multipart writer of PQ-6 writes the boundary and the part lengths correctly.
- DoD: gate passes; no port is opened (the test asserts that the handler's socket is the socketpair end).

#### RV-A6 Pure JavaScript core and its test

- Milestone 6. Effort M. Depends on: RV-A1.
- Create `demo/viewer_core.js` (ES module: `decodeFrame`, `decodeCreatures` returning typed-array views,
  `mergePrev`, `wrapDelta16`, `orbitCamera` (port of `cam_place` and `cam_setup`), `projectPoint`) and
  `tests/cli/viewer_core_test.mjs`, run with node (node 25 is on PATH, `docs/agents/COMMON.md:53`).
- Checks: decoding of a frame written by az_live (the test runs az_live `--stream` for 5 ticks into a file
  first); `mergePrev` on random uid sets (newborns, deaths, both empty, 250,000 records in under 30 ms in
  node); `wrapDelta16` across the wrap; `orbitCamera` gives the same camera position as az_view for 20 random
  (yaw, pitch, dist) to 1e-4 cell (az_snap prints the camera when asked: add `--print-camera` to az_snap, or
  compute it in the test from the formula and compare to az_view's own arithmetic in a tiny C helper; choose
  one and document it).
- Register a ctest gate `cli_viewer_core` that runs only when `node` is found (`find_program`), like other
  optional parts.
- DoD: gate passes.

#### RV-A7 The WebGL2 page

- Milestone 6. Effort L to XL. Depends on: RV-A4, RV-A6.
- Create `demo/viewer_gl.js`; change `demo/live.html` (View switch, HUD, inspect panel, overlay controls,
  World speed and Snapshots per second controls, Measure button, About text: what is smoothed, what the view
  does not show, the known differences to the engine view).
- Everything in section 6.6.
- DoD (owner check, decision O-3): with a lab 20k world the page shows terrain, ocean, vents, the sun
  direction and every creature; orbit, fly, follow, pick and inspect work; the HUD numbers update; "Raw ticks"
  shows the steps.
- Risks: the browser may run on the Intel iGPU of the laptop (PB-13); WebGL context loss (handle
  `webglcontextlost` and `webglcontextrestored` by rebuilding the buffers from the latest messages).

#### RV-A8 The proof view next to the real-time view

- Milestone 6. Effort S. Depends on: RV-A7, PQ-5.
- In "Engine pictures (proof)" or by a "Proof picture" button, the page sends the current orbit camera with
  `camera YAW PITCH DIST`, then `picture`, and shows the engine's PNG beside the GL view, with the tick, the
  hash before and after, and "equal" or "DIFFERENT" in large letters.
- DoD: owner check; the stream smoke test (RV-A3 check 9) covers the engine side.

#### RV-A9 Acceptance run

- Milestone 6. Effort M. Depends on: RV-A7.
- World: `--lab --seed 42 --n 100000 --cap 400000 --grid 6,6,5 --kernels 1`, streamed at `--sps 15`,
  `--tps 0`; the owner clicks "Measure 30 s" while the whole world is in view at a 1920 x 1080 canvas, then
  again after 60 ticks (about 227,000 alive). Also the same world with the stream off for the speed reference
  (az_live without `--stream`, `--pace free`, no pictures: `--pps` minimum and `view spheres`, or az_run).
- Acceptance numbers (goals; each pass or fail is recorded with the GPU name):
  - display: median FPS at least 58 and p99 frame time at most 25 ms on the RTX 5070 Ti, at 100,000 and at
    about 227,000 creatures in view;
  - world speed with the stream on at 15 snapshots per second: at least 0.9 times the same run with the stream
    off;
  - snapshots: at least 14 of 15 per second reach the page at both sizes; median snapshot age at most 150 ms;
  - observer: 0 failures; the lab final hash of a 60-tick run equals az_run's (`73f1e9e013f8db50` for this
    world with `--ticks 60`, `tests/golden_hashes.txt`). Note: the golden line `:69` records this world
    with the default `--kernels 0` only; compare against the `--kernels 1` line that WP-P0-02 records
    (expected equal; until it exists, run az_run with `--kernels 1` and compare with that);
  - picking: a clicked creature's inspect reply names the uid under the cursor (owner check on 5 clicks).
- Target case 2, the owner's own world (section 2.5; `build/work/logs/demo_owner_world_2026-11-18_f226.json`):
  wild, 1,000,416 living creatures at a cap of 1,004,096, ticks of about 414 ms (about 2.4 per second).
  World: the PQ-0 1M configuration (run in the background until `live` passes 1,000,000), streamed at `--sps`
  equal to the tick rate (about 2.4). Goals, each pass or fail recorded with the GPU name and the load:
  - display: the 60 FPS viewer keeps the camera smooth while ticks take about 0.4 s: median FPS at least 58
    and p99 frame time at most 25 ms while orbiting, also over the frames in which a new snapshot arrives
    (decode, uid merge and GPU upload of 1,000,416 records must not stall a frame: spread them over frames
    or run them in a worker);
  - bandwidth: one snapshot is 16 + 64 + 24 x 1,000,416 = 24,010,064 bytes, about 24 MB; at one snapshot per
    tick (about 2.4 per second) that is about 58 MB/s through the pipe, Python and the WebSocket;
  - world stall: today `az_live` draws between ticks on the thread that ticks (`tools/az_live.c:527`, `:534`),
    so the world stands still for each picture, about 4.3 s at 1M (section 2.5). The stream must not add such
    a stall. Copy-cost budget per snapshot on the main thread (walk the slots, fill the records, sort by uid,
    hand the buffer to the writer thread): at most 20 ms at 1,000,416 creatures, that is at most 5 percent of a
    414 ms tick and the same rate as the 5 ms goal for 250,000 creatures in section 6.2. Measure it and record
    it. The observer hashes are budgeted apart from it (sampled by `observer-ms`, section 6.5); their cost at 1M
    is measured by PQ-1 first;
  - world speed with the stream on: at least 0.9 times the same world with the stream off.
- If the renderer string names the Intel iGPU, record the result, ask the owner to set the browser to the
  high-performance GPU in Windows graphics settings (his action, PB-13), and measure again.
- Evidence: the perf JSON files, the PROGRESS.md entry, a screenshot by the owner if he wants one.

#### RV-A10 Delta snapshots (only if RV-A9 shows the stream binds)

- Milestone 6. Effort M. Depends on: RV-A9 failing on bandwidth or Python CPU.
- Message type 7 CREATURES_DELTA: records of creatures whose position changed by at most 127/65536 world per
  axis as i8 deltas, keyed by the merge order; full CREATURES every 2 seconds and after any gap.
- DoD: the RV-A9 numbers pass where they failed, with the same stream smoke checks for the delta path.

#### RV-A11 Documentation

- Milestone 6. Effort S. Depends on: RV-A9.
- Update `demo/README.md` (the real-time view, its controls, what is smoothed), `docs/handoff/next-agent/03-DEVOPS-PIPELINE.md`
  (az_live's new options, the new gates), `docs/STREAM.md`.
- DoD: `check_text.py` passes on the changed files.

#### RV-A12 Python relay or a native socket (decision point)

- Milestone 6. Effort M if taken. Depends on: RV-A9.
- If RV-A9 shows that Python's relay is the bottleneck (its CPU above one core, or snapshots lost in the
  server), move the WebSocket into az_live (Winsock, 127.0.0.1 only, the same Host and Origin rules, SHA-1 and
  base64 for the handshake written in hosted C) and keep the Python server for the page and the commands.
- DoD: RV-A9 numbers pass; `cli_ws_selftest` has a C twin test of the handshake.

### 6.8 What Design A cannot do

- It will not show 1,000,000 creatures at 15 snapshots per second (360 MB/s through Python and a socket;
  section 6.3). Use fewer snapshots per second, a display subsample, or Design B. At the owner's own world's
  rate of about 2.4 ticks per second it needs about 58 MB/s (RV-A9 target case 2).
- It does not make the world faster; section 8 does.

---

## 7. Track RV-B: the native Windows viewer

The plan's own wording: "the Windows viewer (reads the world only between ticks; you build it, the owner opens
it)" (`docs/handoff/CONTINUE-BUILD.md:53`, `docs/handoff/STATUS_AND_PLAN.md:687`). Design B is that viewer,
built for millions of creatures: the world and the window in one process, the snapshot copied straight into GPU
buffers, no pipe, no Python, no browser.

### RV-B1 Feasibility probe

- Milestone 6. Effort S. Depends on: nothing (can run any time).
- Check, by reading and a tiny program, that the MinGW-w64 headers of the WinLibs gcc 16.1 include `d3d11.h`,
  `dxgi.h` and `d3dcompiler.h`, and that `C:\Windows\System32\d3dcompiler_47.dll` exists (load it with
  `LoadLibraryW`, get `D3DCompile`). If a header is missing, report it as missing (no download, decision O-5).
- Create `build/work/viewnative/probe.c`: create a D3D11 device of type WARP (no window), compile a trivial
  vertex and pixel shader from text with `D3DCompile`, draw one instanced triangle into an offscreen texture,
  read it back, count lit pixels.
- DoD: the probe prints the lit pixel count > 0 and exits 0; it opens no window.

### RV-B2 The program

- Milestone 6. Effort L. Depends on: RV-B1, RV-A2 (the extraction code in `tools/az_stream.h`).
- Create `tools/az_viewnative.c` (the CMake view prefixes include `view`, `CMakeLists.txt:222`; check that the
  name `viewnative` is matched and that the tool links the view component; adjust the list if not). Link
  `d3d11`, `dxgi`, `user32`, `gdi32` (system import libraries of MinGW); load `d3dcompiler_47.dll` at run time.
- Same world options as az_live (`--n`, `--lab`, `--seed`, `--grid`, `--cap`, `--species`, `--lesion`,
  `--param`, `--load`, `--threads`, `--kernels`, `--ecores`, `--no-pin`), plus `--tps`, `--sps` (default 30),
  `--offscreen WxH --frames K --out FILE.png` for tests (WARP or hardware device, no window).
- Threads: the simulation thread ticks and, between ticks, extracts the snapshot into one of three buffers
  (triple buffering, latest-wins); the render thread owns the window and the device, uploads the newest buffer
  with `Map(WRITE_DISCARD)` and draws at the display rate (vsync on by default).
- Observer: as section 6.5, with `--observer-ms`.

### RV-B3 Rendering for millions

- Milestone 6. Effort L. Depends on: RV-B2.
- HLSL shaders kept as text in the C file, compiled at start-up with `D3DCompile` (`vs_5_0`, `ps_5_0`).
- Bodies: the same records as Design A (24 bytes) in a structured buffer; impostor quads for all; near bodies
  as displaced icospheres from a shape atlas texture (the SHAPES tables); frustum culling and level of detail
  in a compute shader writing an indirect-args buffer (`DrawInstancedIndirect`), or per-instance collapse as in
  Design A for the first version.
- Terrain, ocean, vents, suns: as section 6.6.
- Camera and picking: as section 6.6 (picking reads one texel of an `R32_UINT` target).
- Later, after SC-3 (GPU-resident world): read positions from the device columns through CUDA-D3D11 interop
  (`cuGraphicsD3D11RegisterResource`), no host copy. Not before the world lives on the GPU.

### RV-B4 Tests without a window (ctest gate `cli_viewnative_offscreen`)

- Milestone 6. Effort M. Depends on: RV-B3.
- `az_viewnative --offscreen 640x360 --frames 3 --lab --seed 7 --n 2000 --max-ticks 20 --out FILE.png` on a WARP
  device: exit 0; lit pixel count above a floor; the state hash before and after every extraction equal; the
  lab final hash equal to az_run's. Pixel hashes are not compared across devices (floats on GPUs are not
  bit-exact across drivers); a WARP-to-WARP repeat on the same machine may be compared.
- DoD: gate passes; it opens no window.

### RV-B5 Acceptance (owner opens it)

- Milestone 6. Effort M. Depends on: RV-B4.
- World: `--lab --seed 42 --n 1000000 --cap 1500000 --grid 7,7,5 --kernels 1 --store-units 52000000` (the 1M
  reference config, `tests/golden_hashes.txt`).
- Goals: display at least 58 FPS median with every creature in view at 1920 x 1080 on the RTX; upload at most
  5 ms per snapshot; the world with the viewer at least 0.95 times its speed without it. Be honest in the HUD:
  the 1M world itself does about 0.75 ticks per second today (`build/work/logs/wg_ref1m.txt:7`), so the
  display interpolates over gaps of more than a second.
- Evidence: the program's own `--perf 30` report (frame times, upload times, ticks per second) written to
  `build/work/perf/rvb5/`.

### RV-B6 Documentation

- Effort S. `docs/VIEWNATIVE.md`, `demo/README.md` pointer, 03-DEVOPS entry.

---

## 8. Track ES: engine speed

### 8.1 How every speed change is measured

- **Reference runs** (commands from `docs/handoff/CONTINUE-BUILD.md:42`, Z: paths):

  ```
  $A = 'Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native'
  $O = "$A\build\work\perf\<task-id>"
  [IO.Directory]::CreateDirectory($O) | Out-Null
  & "$A\build\z\az_run.exe" --lab --n 100000 --ticks 60 --cap 400000 --grid 6,6,5 --threads 8 | Set-Content -LiteralPath "$O\ref100k_t8_k0.txt"
  & "$A\build\z\az_run.exe" --lab --n 100000 --ticks 60 --cap 400000 --grid 6,6,5 --threads 8 --kernels 1 | Set-Content -LiteralPath "$O\ref100k_t8_k1.txt"
  & "$A\build\z\az_run.exe" --lab --n 100000 --ticks 60 --cap 400000 --grid 6,6,5 --threads 24 --ecores --no-pin --kernels 1 | Set-Content -LiteralPath "$O\ref100k_t24_k1.txt"
  & "$A\build\z\az_run.exe" --lab --n 100000 --ticks 60 --cap 400000 --grid 6,6,5 --threads 1 | Set-Content -LiteralPath "$O\ref100k_t1_k0.txt"
  & "$A\build\z\az_run.exe" --lab --n 1000000 --ticks 20 --cap 1500000 --grid 7,7,5 --kernels 1 --store-units 52000000 | Set-Content -LiteralPath "$O\ref1m_t8_k1.txt"
  ```

  Each output has the `stages (ms/tick)` line, the `final hash` line and the `matter`/`energy` line. The 1M run
  takes about 30 s plus world creation; run it in the background with its log on Z: if a tool call could
  exceed the agent's limits (`docs/handoff/CONTINUE-BUILD.md:77`).
- **Repeat and report the median of 5 interleaved runs** (before, after, before, after, ...) for any change
  claimed below 10 percent; for larger changes 3 interleaved runs. Label the load: before each set, record the
  CPU use of other processes (`Get-Process | Sort-Object CPU -Descending | Select-Object -First 8 Name,CPU`)
  and write "idle", "lightly loaded" or "noisy, parallel load" (`docs/handoff/CONTINUE-BUILD.md:40`).
- **Primary metric: ns per live creature per tick** (az_run prints it, `build/work/logs/wg_ref100k_t8.txt:7`),
  with ms per tick and the live count beside it, because the 100k world more than doubles during its run.
- **The whole benchmark (E3):** `tools/az_bench.c` over populations, thread settings and kernel settings
  (ES-14).

### 8.2 How every speed change proves it does not change the world

1. The two reference hashes, before and after, equal: `73f1e9e013f8db50` and `5d9c02d2c7dd5fb1`
   (`tests/golden_hashes.txt`, the two worldgen lines), unless the task says the hash changes by design.
2. The determinism matrix on both reference configs. Read its usage first
   (`python -I "$A\tests\cli\determinism_matrix.py" --help`); the form used at milestone 3 was
   `python -I "<root>\tests\cli\determinism_matrix.py" "<root>\build\z\az_run.exe" "<dir>" --expect 73f1e9e013f8db50 -- --n 100000 --ticks 60 --cap 400000 --grid 6,6,5`
   and `--light` for the 1M config (`build/work/PROGRESS.md:180`, then with `N:` paths). It covers threads
   1, 2, 5, 8, 12, 16, 24 with and without `--ecores` and `--no-pin`, reorder periods, split seeds, kernels
   0 and 1 and a snapshot resume (`docs/handoff/CONTINUE-BUILD.md:43`).
3. The ctest gate `cli_matrix_engine` (the engine matrix hash `24c923001a46b122`, printed, not pinned).
4. The whole suite: `ctest --test-dir "$A\build\z" --output-on-failure -j 6` (46 of 46 today), in the
   background with its log on Z: (it takes about 4 minutes; `docs/handoff/CONTINUE-BUILD.md:77`).
5. For a change inside one stage, `--hash-every 1` on a short run before and after finds the first diverging
   tick if anything differs (`docs/handoff/CONTINUE-BUILD.md:43`).
6. A new twin (assembly or GPU) is held to bit identity against its C reference on at least 1,000,000 random
   inputs including edge cases, as the existing twins are (`gpu/README.md:138-171`).
7. The gcc and clang builds give the same hashes (`build/work/scripts/clang_check.ps1`).

### 8.3 Tasks

#### ES-0 Baseline on the current tree

- Milestone 6. Effort S. Depends on: nothing.
- Run the five reference commands of 8.1 into `$A\build\work\perf\es0\`, three times interleaved, idle machine
  if possible.
- DoD: a table in PROGRESS.md with ms per tick, ns per live creature per tick, live count, the stage line and
  the hash of every run; all hashes equal their references; the kernel 1 gain at 100k measured (it is not in
  the logs today: the 100k reference ran with the C brain, `build/work/logs/wg_ref100k_t8.txt:4`).

#### ES-1 M2: the per-stage cache table

- Milestone 6 (track M). Effort L. Depends on: W2 done (sense, physics, stats real), ES-0.
- Source of the task: `docs/handoff/STATUS_AND_PLAN.md:676`. Cache sizes: L1D 48 KB and L2 3 MB per P core,
  L2 4 MB per 4-E-core cluster, L3 36 MB (same line; `docs/agents/COMMON.md:45-47`).
- Method (no hardware counters: programming them needs a driver, `docs/handoff/STATUS_AND_PLAN.md:674`; WPR
  with PMU sampling may need elevation, which is a privilege change: ask first, decision O-6):
  1. **Bytes touched per creature per stage**, from the code and `tools/az_budget.c`: list every column and
     store segment each stage reads and writes on its common path (rule D8, `docs/ARCHITECTURE.md:195-206`),
     with element sizes from `core/az_columns.def`.
  2. **The curve**: ns per creature per stage at 1 worker for n = 4,096, 16,384, 32,768, 65,536, 131,072,
     262,144, 524,288, 1,048,576 (az_bench `--n` list, `--threads 1`, kernels 0 and 1). A knee in a stage's
     curve where its bytes times n crosses 48 KB x 128-blocks, 3 MB, about 40 MB and 76 MB names the level it
     falls out of.
  3. **The machine**: `tools/membench/membench.c` on a P core and an E core (`--core P`, `--core E`; build
     it with the command in its header into `$A\build\work\perf\es1\membench.exe`) for read, write and copy
     GB/s by working set; record the table as measured.
  4. **The hot loops**: llvm-mca (`Z:\Tools\LLVM-21\bin\llvm-mca.exe -mcpu=arrowlake-s`; check the CPU name
     with `-mcpu=help` first) on the assembly of the brain step, the scan range test, Philox and the kinematic
     axis loop: throughput per iteration and the bottleneck port.
  5. **Parallel efficiency per stage**: the same stage times at 1, 8 and 24 workers.
- Create `docs/eval/M2-CACHE-TABLE.md`: one row per stage: bytes touched per creature, ns per creature at
  1 worker for each n, the knee, cycles per creature where llvm-mca applies, parallel efficiency, and "the
  bottleneck" in one sentence (compute, latency, bandwidth, serial, dispatch).
- DoD: the table exists with commands and raw outputs under `$A\build\work\perf\es1\`; the corrected cache knee
  (section 2.4) is either confirmed or replaced by the measured one, and `docs/handoff/next-agent/04-GOALS-AND-BENCHMARKS.md:208-210`
  and `docs/handoff/CONTINUE-BUILD.md:29` get a dated note pointing to the table (lead edit).

#### ES-2 v61: one grid build on reorder ticks, not two

- Milestone 6 (M3). Effort S. Depends on: ES-1.
- Finding: `docs/handoff/STATUS_AND_PLAN.md:329` (`core/az_tick.c:312`): on every reorder tick az_grid_reorder
  runs a full az_grid_build and az_tick runs another one.
- Change: `core/az_tick.c`, `core/az_grid.c`.
- DoD: hashes and matrix equal (8.2); the grid stage mean drops by about one build per 16 ticks (measure).

#### ES-3 v67: a parallel cell histogram

- Milestone 6 (M3). Effort M. Depends on: ES-1.
- Finding: `docs/handoff/STATUS_AND_PLAN.md:278` (`core/az_grid.c:150`): at 100k the counting, the prefix over
  all cells and the scatter run serially between two parallel regions.
- Change: `core/az_grid.c`: per-worker histograms over slot ranges, a parallel prefix over cells, a parallel
  scatter that keeps the canonical (cell, uid) order (each worker writes its own slice of each cell's range;
  then the in-cell uid order is restored exactly as today).
- DoD: `test_grid` passes; hashes and matrix equal; grid ms per tick at 100k and 1M before and after.

#### ES-4 v69: the field stage scans fewer cells

- Milestone 6 (M3). Effort M. Depends on: ES-1.
- Finding: `docs/handoff/STATUS_AND_PLAN.md:282` (`core/az_tick.c:340`): four scans of every cell per tick; the
  second commit needs only the cells the strided step touched.
- DoD: `test_field`, `test_chem`, `test_weather` pass; hashes and matrix equal; field ms before and after.

#### ES-5 v70: reset only the scratch that carries state

- Milestone 6 (M3). Effort S. Depends on: ES-1.
- Finding: `docs/handoff/STATUS_AND_PLAN.md:344` (`core/az_tick.c:150`).
- DoD: `test_protocol` and `test_core` pass; hashes and matrix equal.

#### ES-6 v62: one work-splitting policy

- Milestone 6 (M3). Effort S. Depends on: ES-1.
- Finding: `docs/handoff/STATUS_AND_PLAN.md:328` (`core/az_tick.c:34`): two copies that already differ.
- DoD: the split-seed runs of the matrix still give the hash (splits must never matter); one copy left.

#### ES-7 The cost of an empty stage

- Milestone 6 (M3). Effort M. Depends on: ES-1.
- Source: "the 7 us empty-stage cost" (`docs/handoff/CONTINUE-BUILD.md:53`). At small worlds (the demo's 5,000
  to 10,000 creatures at 150 to 330 ticks per second, measured before worldgen with no log; quote 77 to 330
  until PQ-0 re-measures them on the worldgen world) dispatch and barriers are a visible share.
- Change: `platform/win/az_host_win.c` (the pool): measure dispatch with an empty job per stage (a test
  program or a `--probe` mode in az_bench), then reduce it (spin briefly before `WaitOnAddress`, fewer barriers
  per stage, merging stages that need no barrier between them is a core change and needs its own proof).
- DoD: `test_host_win` passes; the empty-stage cost before and after; hashes and matrix equal; 5k-creature
  ticks per second before and after.

#### ES-8 TR-17: build children in parallel

- Milestone 6 (M3). Effort L. Depends on: ES-1.
- Trigger met: the life stage is 12.74 of 73.70 ms at 100k (17 percent) and 313 of 1339 ms at 1M (23 percent),
  above the 10 percent of `docs/TRIGGER-REGISTER.md:37`.
- Change: `core/az_life.c` (and the commit code it calls): keep the serial part (uid assignment in
  (mother uid, n) order, slot from the free list, store allocation, clade founding) and move the building of
  each child (meiosis, development of 112 traits, brain copy and mutation, body) to a parallel pass that reads
  only its parents and its own Philox stream. The order of everything hashed stays the serial order.
- DoD: `test_life`, `test_protocol`, `test_genome`, `test_store` pass; hashes and matrix equal; life ms per tick
  at 100k and 1M before and after; update TR-17 with the run ids.

#### ES-9 Tier 1 at 1M: memory growth without block relocation per word

- Milestone 6 (M3). Effort L. Depends on: ES-1.
- Source: `build/work/PROGRESS.md:683-688` ("each word relocates the creature's whole block in the serial store
  commit"; "grow memory in chunks"). Tier 1 is 633 of 1339 ms at 1M (`build/work/logs/wg_ref1m.txt:8`).
- First establish the cause by measurement: count relocations per tick and the store refusals; run the 1M
  config with a larger `--store-units` if it still locks (the store is full in the reference run).
- Change candidates: grow memory requests in chunks; batch relocations; do relocation work in parallel when
  blocks do not overlap. Whether a candidate changes the hash: blocks' placement is not state (any store
  schedule gives the same hash, `docs/TRIGGER-REGISTER.md:34`), but a different number of memory words held by
  a creature may be. If the hash changes, this is a design change, not a speed change: record it in
  `tests/golden_hashes.txt` with the cause, show what changes in behaviour, and tell the owner.
- "Scale surprise by the creature's own running spread" (same source) changes behaviour: biology, the owner's
  call (decision O-7), not part of this task.
- DoD: tier1 ms per tick at 1M before and after with refusals and relocations counted; hashes equal or a
  recorded design change.

#### ES-10 The scan AVX2 twin

- Milestone 6 (W3). Effort L. Depends on: ES-1.
- Source: `docs/handoff/CONTINUE-BUILD.md:53`; the CMake twin name `scan_avx2` is reserved
  (`CMakeLists.txt:161`) and its file `kernels/az_scan_avx2.asm` is missing.
- Change: the lead adds a dispatch slot in `AzKernels` (`core/az_abi.h`, an ABI change, the lead's alone) for
  the inner range test of `core/az_scan.c` (eight records at a time to a bit mask, `core/az_scan.c:20-21`) or
  for the whole gather-and-test of one piece; the twin in NASM (Win64 ABI, `kernels/win64.inc`); dispatch in
  `kernels/az_kernels.c` by CPUID.
- Test: `tests/test_scan.c` compares the twin to the C function and to `core/stubs/az_scan.c` (the semantic
  definition, `core/az_scan.c:3-8`) on at least 1,000,000 random pieces including full buffers, empty cells,
  records on cell faces and range ties.
- DoD: test passes; hashes and matrix equal with kernels 0 and 1; perceive ms before and after.

#### ES-11 The physics twin

- Milestone 5 (W2 physics brief). Effort L. Depends on: the physics module.
- Owned by the physics brief (`docs/agents/W2-BRIEFS/physics.md:1-10`, `:44-65`): `az_kin_axis_avx2` equal to
  `az_kin_axis_c` on 1,000,000 random blocks. Listed here only because it is a speed item.

#### ES-12 TR-15: brains that share what they have in common

- Milestone 6 (M3). Effort XL. Depends on: ES-1.
- Trigger: "Perceive is the largest stage and the brain step's share of it is above one third ... and the owner
  wants the speed back" (`docs/TRIGGER-REGISTER.md:35`). Perceive is the largest stage (46 percent at 100k).
  The owner wrote on 2026-11-18: "It's really slow. I thought this would be fast." Record that quote as the
  trigger evidence in `docs/TRIGGER-REGISTER.md` (lead edit), measure the brain step's share in ES-1, and ask
  the owner to confirm in the next report (decision O-8). The default is to proceed, because TR-15 requires
  bit-identical motors and hashes: it changes storage, not behaviour.
- Facts: at ABI 4 the change from shared to per-creature brains took the tick from 9.1 to 16.8 ms at 100k and
  from 72 to 141 ms at 1M with the twins (`docs/TRIGGER-REGISTER.md:35`; `build/work/PROGRESS.md:427-428`).
- Change: copy-on-write brains or a child stored as its parent plus changes, reached only through `az_brain()`
  and stepped only through `AzKernels.brain_step` (the seams named in TR-15).
- DoD: bit-identical motors on the matrices; hashes equal; bytes read per brain step and ms per tick at 100k
  and 1M before and after.

#### ES-13 P cores and E cores per stage

- Milestone 6 (M3). Effort M. Depends on: ES-1.
- Allowed now: all 24 cores (`docs/handoff/CONTINUE-BUILD.md:29`). Measure each stage at `--threads 8`,
  `--threads 16 --ecores`, `--threads 24 --ecores`, pinned and `--no-pin`. If a stage is faster on P cores
  only and another gains from E cores, a per-stage worker count is a host change (`platform/win/`), not a core
  change, and needs only the hash proof. The general scheduler across GPU and NPU is SC-5 (after the gate).
- DoD: a table per stage; any change keeps the hashes; ms per tick at 100k before and after.

#### ES-14 E3: the benchmark from 1 to 1,000,000

- Milestone 8. Effort L. Depends on: A1 (`docs/handoff/STATUS_AND_PLAN.md:691`), ES-15.
- First extend `tools/az_bench.c` (it has neither `--cap` nor `--store-units`, `tools/az_bench.c:165`, and no
  page-fault count): `--store-units`, `--cap`, page faults during the measured ticks (`GetProcessMemoryInfo`,
  `PageFaultCount` before and after; link `psapi`), store refusals and cap refusals per row, the load label.
- Command (the population list may be split into runs that each stay under the tool-call limits):
  `& "$A\build\z\az_bench.exe" --n 1000,10000,50000,100000,262144 --threads 1,P,PE --kernels 0,1 --ticks 200 --warmup 50 --repeats 3 --csv "$A\build\work\eval\e3\bench_small.csv" | Set-Content -LiteralPath "$A\build\work\eval\e3\bench_small.txt"`
  and for 524,288 and 1,000,000 the same with `--ticks 20 --warmup 5 --store-units 52000000` (check the usage
  text of the extended tool first; do not guess flags).
- Create `docs/eval/BENCH-SCALE.md`: one row per (n, threads, kernels): ticks per second, ns per live creature
  per tick, ms per stage, arena MB and locked, page faults after start-up, refusals, load label; rows above the
  measured knee (ES-1) labelled "DRAM-bound".
- DoD: the document with commands and raw outputs; every row's arena locked; page faults after start-up 0 or
  explained.

#### ES-15 docs/BENCH.md (missing)

- Milestone 6. Effort S. Depends on: nothing.
- `tools/az_bench.c:15` says "docs/BENCH.md says how to read the output and what makes a measurement valid";
  the file does not exist (`docs/` holds `agents`, `handoff`, `requirements`, `research`, `ARCHITECTURE.md`,
  `GROKBOT-FACTCHECK.md`, `TRIGGER-REGISTER.md`). Write it from `tools/az_bench.c` (its output columns and
  the flags `L B U T`, `:298`, `:583`) and section 8.1.
- DoD: the file exists and every flag of az_bench's legend is explained.

#### ES-16 2 MB pages (TR-03)

- Milestone 9, or earlier if the owner grants the privilege. Effort S once granted.
- Needs "Lock pages in memory" (`docs/handoff/CONTINUE-BUILD.md:85`, D1c); today `SeLockMemoryPrivilege` is not
  held (`docs/agents/COMMON.md:67`). `--large-pages` exists and fails cleanly (`docs/TRIGGER-REGISTER.md:23`).
- DoD (from TR-03): the same hash with and without large pages; at least 5 percent less ms per tick at
  1,000,000 (proposed threshold).

---

## 9. Track SC: scaling after the gate

Start only after the foundation gate (milestones 1 to 8 done, no stubs, every finding closed or answered,
A1 clean, the section-3 invariants of CONTINUE-BUILD, M2 and E3 measured) and the owner's yes on the law-2
wording for phase 2 (`docs/handoff/CONTINUE-BUILD.md:61`, `:87`). Without that yes, stop after milestone 8 and
ask. Every substrate enters as a twin of the CPU C reference: the same `az_state_hash` every tick, exact matter
and energy, an off switch, a measured gain, and an update of its row in `docs/TRIGGER-REGISTER.md`
(`docs/TRIGGER-REGISTER.md:8-14`).

### SC-0 The GPU fixes first (milestone 6, W3)

- Effort S each. Depends on: nothing. They are prerequisites of S2 (`docs/TRIGGER-REGISTER.md:24`).
- v23 (`docs/handoff/STATUS_AND_PLAN.md:294`, `gpu/az_gpu.c:276`): each handle makes its own context current
  (`cuCtxSetCurrent` or push/pop around every call); test: two handles opened on one thread, each runs its own
  kernel, results correct.
- v26 (`:344`, `gpu/az_gpu.c:262`): check the CUresult of the four driver queries; a failure reported as such
  in `info->msg`; test with a fake failing entry point.
- v57 (`:331`, `gpu/az_gpu_ref.c:17`): one small gpu header for `az_memset` and `az_memcpy`, included by both
  translation units.
- v25 (`:332`, `gpu/az_gpubench.c:14`): the header text says the simulation runs the C reference unless
  `--kernels 1` is given.
- DoD: `test_gpu` passes (135 checks today, `gpu/README.md:138-150`) plus the new checks.

### SC-1 S1: locked DDR5 as a streaming tier (TR-01, TR-02)

- Effort XL. Depends on: the gate; TR-01's trigger: E3 shows ns per creature per tick at 1,000,000 at least 1.5
  times the value at 262,144 (`docs/TRIGGER-REGISTER.md:21`).
- Design: the arena stays one locked allocation; hot loops walk 128-creature blocks and prefetch the next block
  (and its store blocks) while computing the current one; populations above 1,000,000 up to the largest arena
  that locks (TR-02 proposes at most half of the 48 GB).
- DoD (TR-01, TR-02): the same hash as the plain arena on both reference configs, exact conservation, an off
  switch, a measured gain in ms per tick; above 1M: arena locked, 0 page faults after start-up, hash equal
  across thread counts, rows labelled DRAM-bound.

### SC-2 2 MB pages (TR-03)

- See ES-16.

### SC-3 S2: the GPU whole tick (TR-04, TR-05, TR-18)

- Effort XL (several weeks). Depends on: the gate, SC-0.
- The design exists and is not built: `gpu/README.md:263-444`. Follow its order of work exactly
  (`gpu/README.md:416-431`):
  1. device image of the arena: upload, `az_world_rebase`, download, `memcmp` after a round trip with no
     kernel;
  2. state hash and the two totals computed on the device, equal to the CPU's;
  3. one stage at a time in pipeline order (counting sort for the grid, `gpu/README.md:349-367`; tallies and
     events by block reductions or atomics, `:369-379`; the life commit first as a one-thread kernel,
     `:381-400`): start CPU and GPU from the same snapshot, run the stage, download what it writes, `memcmp`,
     on several seeds and sizes including n not a multiple of the block;
  4. the whole tick on the device with the hash equal to the CPU's after every tick of a long run;
  5. only then parallel replacements of serial fallbacks and tuned kernels, each measured.
- Needs from the lead (`gpu/README.md:433-439`): the per-entity bodies of the tick jobs in an inline header
  shared by `core/az_tick.c` and `gpu/az_gpu_dev.c`; tally and event updates through a call the device build
  can redefine.
- Measured facts for expectations: the brain kernel alone takes 13 to 14 ns per creature on the GPU against 42
  to 58 ns on one CPU thread with the AVX-VNNI twin and 220 to 500 ns with the C reference; with copies 19 to
  30 ns from page-locked memory (`gpu/README.md:192-243`). No whole-tick number exists. Promise none.
- No CUDA toolkit: device code is clang-built PTX that the driver compiles at load (`gpu/README.md:43-61`).
- DoD (TR-04, TR-05): the GPU state hash equals the CPU hash every tick for both reference configs over all
  their ticks; exact conservation; off switch; measured ms per tick; then the GPU-resident world and a
  deterministic CPU+GPU split with the hash equal for every split tested.

### SC-4 S3: the NPU probe (TR-06)

- Effort M. Depends on: the gate.
- Present: `ze_loader.dll`, `DirectML.dll` 1.15.6, `npu_level_zero_umd.dll`, `npu_d3d12_umd.dll`,
  `openvino_intel_npu_compiler.dll`; no Level Zero headers under `Z:\Tools` (`docs/TRIGGER-REGISTER.md:26`).
- Question: does the integer brain step run bit-exact through them over 1,000,000 steps? NPU code is a graph the
  driver compiles, not C23, NASM or PTX: no NPU result enters world state without the owner's yes (decision
  O-9); default: probe only, analytics outside the hashed state if not bit-exact.

### SC-5 S4: one placement scheduler (TR-07)

- Effort L. Depends on: at least two substrates past their twin rows (TR-04 and TR-06 or TR-01).
- One scheduler that places each stage on P cores, E cores, GPU or NPU by measured cost (`prof_ns` per stage
  exists), with a placement-independent hash.
- DoD: every placement gives the CPU hash; a table of placement against ms per tick.

### SC-6 R2: audit and E1 to E3 per substrate (TR-08)

- Effort L. Depends on: SC-5.
- A reproduce-first audit of the new host, GPU and NPU code; E1 to E3 re-run per substrate.
- DoD: no confirmed finding open; E1 to E3 tables per substrate.

---

## 10. What is honest to promise

| What | Today (measured) | Goal | How sure, and why |
|---|---|---|---|
| Demo world speed at 5k to 10k creatures | 4 to 12 ticks/s (capped by C1; writer's session) | at least 0.8 x az_run's 150 to 330 ticks/s (PQ-2, PQ-5) | likely: the cap is in the demo loop, not the engine |
| Pictures per second at 960x540 | 2 to 6 (0.1 to 0.5 s each) | at least 0.3 x today's render time with 8 view threads (PQ-4), more with spheres mode (PQ-8) | likely; to be measured |
| Display FPS, Design A, 100k to 250k creatures in view, RTX | does not exist | median >= 58, p99 frame time <= 25 ms (RV-A9) | likely on the RTX for instanced impostors and low-poly meshes; unknown on the iGPU; to be measured |
| Snapshots per second, Design A, 250k creatures | does not exist | >= 14 of 15 delivered, median age <= 150 ms (RV-A9) | plausible: 90 MB/s on 127.0.0.1 with no per-record Python work; to be measured; RV-A12 is the fallback |
| World slowdown from streaming | does not exist | <= 10 percent (RV-A9) | plausible if the observer check is sampled in the wild demo; the hash cost is unknown (PQ-1) |
| Display FPS, Design B, 1M creatures, RTX | does not exist | median >= 58 (RV-B5) | plausible; to be measured |
| The owner's wild world, 1,000,416 creatures, 1600x900 | 414.3 ms per tick (about 2.4 ticks/s); 4,261.3 ms per picture with the world stopped meanwhile (section 2.5, logged) | camera smooth at 60 FPS while ticks take about 0.4 s; at most 20 ms of world stall per snapshot; about 58 MB/s (RV-A9 target case 2) | plausible for the display and the stream; the tick rate itself needs track ES and has no goal yet |
| Engine at 100k founders, 8 P cores | 73.7 ms, 13.6 ticks/s, 324 ns per live creature per tick | 30 ticks/s (33 ms) after M3 | uncertain, see the arithmetic below |
| Engine at 100k founders, 24 cores | 59.9 ms, 16.7 ticks/s | 30 ticks/s or better after M3 | plausible with TR-15 |
| Engine at 1M founders | 1339 ms, 0.75 ticks/s (full store) | 3 ticks/s (about 330 ms) after ES-8, ES-9, ES-12 | uncertain: dominated by a full store today |
| GPU whole tick | does not exist | no number | the design is unbuilt; only the brain kernel is measured |

**The arithmetic behind the 100k goal (estimates).** From 73.7 ms on 8 P cores:
- life 12.7 -> about 3 ms with parallel child building (ES-8): saves about 9.7 ms;
- grid 7.5 -> about 3 ms with v61 and v67 (ES-2, ES-3): saves about 4.5 ms;
- kernels 1 (the AVX-VNNI brain twin): at the econ landing it saved 9.4 of 49.7 ms (`build/work/PROGRESS.md:864-865`);
  assume about 10 ms now;
- scan twin (ES-10): unknown; assume 3 to 6 ms of the 34 ms perceive stage;
- total: about 73.7 - 9.7 - 4.5 - 10 - 4 = about 45 ms, that is about 22 ticks per second.

So 30 ticks per second at 100k founders on 8 P cores needs more than the listed changes: TR-15 (ES-12; at ABI 4
per-creature brains cost +7.7 ms at 100k founders, `build/work/PROGRESS.md:427`, and the world is larger now)
and probably all 24 cores (ES-13; -19 percent today). And the three modules still to land (sense, physics,
stats) add cost of unknown size. Tell the owner exactly this: 30 ticks per second at 100k is a goal, likely
only with all 24 cores and TR-15, measured after M3, not promised.

**What the owner will see.** After PQ: the demo's world runs at the engine's speed and pictures come several
times faster. After RV-A: smooth 60 FPS motion of every creature in a real terrain with ocean, vents and suns,
while the world steps 14 to 17 times per second at 100k founders (today's engine) and 150 to 330 times per
second at 5k to 10k. In his own million-creature world the camera is smooth, but the world steps about 2.4
times per second until track ES makes it faster. After ES: the world steps faster, by the measured amounts.

---

## 11. Problems, holes, blind spots and dilemmas

| Id | Problem | Evidence | Handled by |
|---|---|---|---|
| PB-01 | The world waits for pictures: the main cause of the slowness | `tools/az_live.c:504-507`, `:525-529` | PQ-2, PQ-5 |
| PB-02 | The page shows at most 4 pictures per second | `demo/live.html:721` | PQ-6 |
| PB-03 | The demo always runs the C reference brain | `demo/live.html:538-540`; `demo/live_server.py:186-187` | PQ-7 |
| PB-04 | az_live cannot use the E cores | `tools/az_live.c:384` (flags 0) | PQ-7 |
| PB-05 | Two whole-world hashes per picture; cost unknown; grows with the store (about 2.7 KB per creature) | `tools/az_live.c:532-536`; `core/az_abi.h:1346`; `build/work/logs/wg_ref100k_t8.txt:10` | PQ-1, section 6.5 |
| PB-06 | A statistics pass over every slot per picture | `tools/az_live.c:564-584` | PQ-2 (rate limit) |
| PB-07 | The cache knee in the plan counts 140 bytes per creature, not the per-creature brain read every tick; by the same arithmetic both reference worlds are beyond the caches | `docs/handoff/CONTINUE-BUILD.md:29`; `docs/TRIGGER-REGISTER.md:35` | ES-1; section 2.4 |
| PB-08 | Tier 1 at 1M is 47 percent of the tick and grows about 100x for 5.3x creatures | `build/work/logs/wg_ref1m.txt:8`; `build/work/PROGRESS.md:683-688` | ES-9 |
| PB-09 | TR-17's trigger is met (life above 10 percent of the tick) and nobody acted | `build/work/logs/wg_ref100k_t8.txt:8`; `docs/TRIGGER-REGISTER.md:37` | ES-8 |
| PB-10 | "100k" speeds mix world sizes: the world more than doubles in 60 ticks | `build/work/logs/wg_ref100k_t8.txt:7` | report ns per live creature per tick (8.1) |
| PB-11 | The 1M reference measures a full store (380,865 refusals) | `build/work/logs/wg_ref1m.txt:11` | ES-9, ES-14 (refusals per row) |
| PB-12 | az_bench has no `--cap`, `--store-units` or page-fault count; docs/BENCH.md is missing | `tools/az_bench.c:15`, `:165` | ES-14, ES-15 |
| PB-13 | On this laptop the browser may render on the Intel iGPU, not the RTX | `docs/agents/COMMON.md:49-50` (both GPUs present) | RV-A9 records the renderer; the owner sets the Windows graphics preference (his action; it is a system setting) |
| PB-14 | The agent may not start servers, browsers or windows, but the demo is a server and a page | `docs/agents/COMMON.md:106-107`; `docs/handoff/CONTINUE-BUILD.md:36` | headless tests (RV-A5, RV-A6, RV-B4); owner checks; decision O-3 |
| PB-15 | `live_smoke.py` expects one frame per 5 ticks; a new default pace would break it | `tests/cli/live_smoke.py:99` | new pace is opt-in (PQ-2) |
| PB-16 | Python's standard library has no WebSocket; hand-written protocol code on a socket | Python 3.9 stdlib | RV-A4 rules, RV-A5 tests, 127.0.0.1 only |
| PB-17 | A blocked stdout write would stall the world | Windows pipes block when full | writer thread, credit flow (6.4) |
| PB-18 | Interpolated motion is not what the world computed between ticks | design | "smoothed" label and "Raw ticks" switch (6.6) |
| PB-19 | `uid` is 64-bit in the columns, but ARCHITECTURE D6 says a world ends after 2^32 - 2 births | `core/az_columns.def:55`; `docs/ARCHITECTURE.md:189-191` | the stream sends 64 bits; report the doc conflict to the lead (one of the two texts is stale) |
| PB-20 | The physics stub lets bodies pass through rock; a terrain viewer will show it | `build/work/PROGRESS.md:1603` | HUD note until physics lands |
| PB-21 | The engine view keeps shapes for at most 4,096 clades | `view/az_view.c:16` | the GL viewer uses an LRU atlas of 7,225 tiles (6.6); record the engine view's limit in its About text |
| PB-22 | The picture shows no terrain, ocean, vents or suns | `view/az_view.c:322-400` | RV-A7; optional later: teach az_view the terrain too (not required: the proof view proves the observer, not the scenery) |
| PB-23 | Slot order changes at every reorder | `docs/ARCHITECTURE.md:183-191` (D6) | the stream is keyed by uid |
| PB-24 | Snapshot size grows with the population up to the cap | section 6.3 | lower `sps`, display subsample, Design B |
| PB-25 | Sleep granularity breaks tick pacing | Windows timer | high-resolution waitable timer (PQ-2); no `timeBeginPeriod` |
| PB-26 | An agent harness may give az_live a pipe at its end: it quits at once | `tools/az_live.c:237-243`, `:464` | measure from Python with `stdin=DEVNULL` (section 3) |
| PB-27 | Python `-I` blocks sibling imports in the demo and the tests | Python docs, isolated mode | keep server code in one file; tests load by path (section 3) |
| PB-28 | Saved worlds from before worldgen no longer load, but the server lists every `.azsnap` | `docs/handoff/next-agent/04-GOALS-AND-BENCHMARKS.md:220`; `demo/live_server.py:683-699` | mark unloadable worlds in the list (load the header with `az_snapshot_config` through az_live `--check FILE`, a new tiny option), or show az_live's refusal clearly |
| PB-29 | No hardware counters for M2 without a driver; WPR sampling may need elevation | `docs/handoff/STATUS_AND_PLAN.md:674` | ES-1 method (curves, llvm-mca, membench); decision O-6 |
| PB-30 | The GPU fixes v23, v26, v57, v25 block S2 | `docs/TRIGGER-REGISTER.md:24` | SC-0 in milestone 6 |
| PB-31 | Timings are noisy when other agents run | `docs/agents/COMMON.md:113-114` | load labels, interleaved medians (8.1) |
| PB-32 | The engine view and the GL view colour creatures differently (individual trait shifts) | `view/az_view.c:582-597` | stated in the page's About text (6.2 SHAPES) |
| PB-33 | `az_live` frames are pruned while the server may still serve them | `tools/az_live.c:262-271`; `demo/live_server.py:611-614` | already handled (404 "frame gone"); the multipart stream (PQ-6) reads each frame once right after its line |
| PB-34 | Pinned E-core placement has never been measured | `build/work/PROGRESS.md:1589` (only `--ecores --no-pin`) | ES-13 |
| PB-35 | The 100k reference ran with the C brain, so the twin gain at the current world is unknown | `build/work/logs/wg_ref100k_t8.txt:4` | ES-0 |
| PB-36 | `az_live` draws between ticks on the thread that ticks: in the owner's 1M world the world stands still about 4.3 s per picture, about 15 percent of a picture period of about 29 s (arithmetic; the period is not measured, PQ-1 adds `ms_period`) | `tools/az_live.c:527`, `:534`; `build/work/logs/demo_owner_world_2026-11-18_f226.json` (section 2.5) | PQ-5 now; RV-A2 with the copy-cost budget of RV-A9 target case 2 |
| PB-37 | About 31 percent of the owner's 1M tick (127.2 of 414.3 ms) is not in the named stage times; a candidate from the code: the two serial whole-world sums after every tick (`tools/az_live.c:528`, `core/az_world.c:628`, `:649`) | section 2.5 | ES-1 (M2) finds where it goes |

**Dilemmas, with the default this document takes.**

- **DL-1 Browser viewer (JavaScript, a development-tool language) or native viewer (C, closer to laws 1 and
  10).** Default: Design A first (it reuses the existing demo and reaches the owner in about two weeks), Design B
  after it for millions. The viewer is outside the simulation either way.
- **DL-2 The cost of the observer proof against the snapshot rate.** Default: every snapshot checked in tests
  and lab runs; in the interactive wild demo at most two full checks per second, named in the HUD; the picture
  check unchanged; the read-only arena (RV-A2c) as a cheap extra if it measures well.
- **DL-3 Smooth motion that the world did not compute.** Default: smoothing on, labelled, with "Raw ticks" one
  click away.
- **DL-4 Per-creature brains (each creature its own plastic brain) against law 2 and speed.** Default: TR-15
  (copy-on-write brains, bit-identical) in M3, with the owner's confirmation asked (O-8).
- **DL-5 Python relay against a native socket in az_live.** Default: Python; RV-A12 if measured too slow.
- **DL-6 Starting the viewer work before W2 ends.** Default: yes for PQ and RV-A (no core files, no hash
  change), recorded as a deviation (section 4); no ES work in core before W2 ends.

---

## 12. Decisions for the owner

| Id | Question | Default until he answers |
|---|---|---|
| O-1 | Start the viewer work (PQ, RV-A) before wave 2 ends? | yes, recorded as a deviation (section 4) |
| O-2 | Browser viewer first or native viewer first? | browser first (DL-1) |
| O-3 | May the agent start the demo server and open the page once to check it, or does the owner always open it? | the owner opens it; the agent ships headless tests and a checklist |
| O-4 | Set the browser to the high-performance GPU in Windows graphics settings (a system setting: his action)? | not changed; the renderer string is recorded |
| O-5 | Installs (Windows SDK fxc, PIX, WPA, CUDA toolkit) if a probe shows a gap? | nothing installed; list what is missing (`docs/handoff/CONTINUE-BUILD.md:85`) |
| O-6 | Elevation for Windows Performance Recorder PMU sampling in M2? | no; M2 by curves, llvm-mca and membench |
| O-7 | Tier 1 "scale surprise by the creature's own running spread" (behaviour change)? | not done; only hash-neutral or recorded storage changes in ES-9 |
| O-8 | Confirm that "It's really slow. I thought this would be fast." is the TR-15 trigger ("the owner wants the speed back")? | treated as the trigger; ask for confirmation in the next report |
| O-9 | Any NPU result in world state? | no; probe only (`docs/TRIGGER-REGISTER.md:26`) |
| O-10 | Law-2 wording for phase 2 (gate for milestone 9) | phase 1 holds (`docs/handoff/CONTINUE-BUILD.md:87`) |
| O-11 | "Lock pages in memory" for 2 MB pages | not granted; VirtualLock without large pages |
| O-12 | A quiet-machine window for M2 and E3 | measure and label the load |

---

## 13. Evidence formats

### 13.1 PROGRESS.md entry (one per task)

```
### <date> <time> UTC: <task id> <title>
- Files changed: <list>.
- Commands (exact, Z: paths): <command 1>; <command 2>.
- Output tails: "<last lines that prove it>".
- Hashes: 100k <hash> (expected 73f1e9e013f8db50), 1M <hash> (expected 5d9c02d2c7dd5fb1), matrix <pass/fail>.
- Speed: <ms per tick before -> after>, <ns per live creature per tick>, live <n>, load <idle|lightly loaded|noisy>.
- Tests: <ctest line>, new checks <n>, planted defect caught: <yes/no, which>.
- DoD: met / not met (which line failed).
- Decisions by default: <list>.
- Next: <task id>.
```

### 13.2 tests/golden_hashes.txt line (only when a hash changes by design)

`<date> | <old> -> <new> | <full command, Z: path> | <the change and its cause> | <thread counts checked; ms per tick; log path>`
(the format of the existing lines, `tests/golden_hashes.txt`, the two worldgen lines).

### 13.3 Performance logs

- Raw outputs under `$A\build\work\perf\<task-id>\` (one file per run, named by world, threads, kernels and
  repeat), CSV from az_bench beside them.
- Viewer measurements: `build/demo/logs/perf-<run_id>-<time>.json` with `{"gpu": "...", "canvas": [w, h],
  "creatures": n, "fps_median": x, "frame_ms_p99": y, "snapshots_per_s": s, "snapshot_age_ms_median": a,
  "ticks_per_s": t, "observer": {"passed": p, "failed": f}, "transport": "ws|poll", "smoothing": true}`.
- A trigger row update in `docs/TRIGGER-REGISTER.md` closes only with a run id and a command
  (`docs/TRIGGER-REGISTER.md:6`).

---

## 14. Checklist of every task

| Id | Milestone | Effort | Depends on | Definition of done, short |
|---|---|---|---|---|
| PQ-0 | 6 | S | - | medians of every picture cost per configuration recorded, the 1M configuration included |
| PQ-1 | 6 | S | PQ-0 | five new timing fields; cli_live_smoke 22 checks pass |
| PQ-2 | 6 | M | PQ-1, PQ-5 | free pace; lab hash equal at several rates; world >= 0.8 x az_run |
| PQ-3 | 6 | S | PQ-1 | test_view passes; ms_encode <= 0.3 x before (goal) |
| PQ-4 | 6 | M | PQ-1 | view hash and info equal for 1, 2, 3, 8, 13 threads; ms_render <= 0.3 x (goal) |
| PQ-5 | 6 | M | PQ-4 | scene render equals direct render; capture <= 10 percent of a tick (goal) |
| PQ-6 | 6 | S | - | every picture shown; multipart test passes |
| PQ-7 | 6 | S | - | kernels 1 and E cores in the demo; same hash |
| PQ-8 | 6 | S | PQ-4 | spheres mode, n_marched 0; ratio recorded |
| PQ-9 | 6 | S | PQ-2..8 | defaults give >= 0.8 x az_run's ticks per second |
| RV-A1 | 6 | S | - | docs/STREAM.md and tools/az_stream.h; constants cross-checked |
| RV-A2 | 6 | L | RV-A1, PQ-2, PQ-5 | --stream works; RV-A3 passes |
| RV-A3 | 6 | M | RV-A2 | cli_stream_smoke 11 checks pass; two planted defects caught |
| RV-A4 | 6 | M | RV-A2 | reader, /ws, long-poll, /api/perf, commands |
| RV-A5 | 6 | S | RV-A4 | cli_ws_selftest passes; RFC 6455 known answer; no port opened |
| RV-A6 | 6 | M | RV-A1 | cli_viewer_core passes under node |
| RV-A7 | 6 | L-XL | RV-A4, RV-A6 | the WebGL2 page with terrain, ocean, vents, suns, camera, picking, HUD (owner check) |
| RV-A8 | 6 | S | RV-A7, PQ-5 | proof picture beside the GL view |
| RV-A9 | 6 | M | RV-A7 | FPS, snapshot, slowdown, observer and picking numbers recorded with the GPU name; target case 2 (the owner's 1M world) recorded |
| RV-A10 | 6 | M | RV-A9 fails on bandwidth | delta snapshots; RV-A9 passes |
| RV-A11 | 6 | S | RV-A9 | docs updated; check_text passes |
| RV-A12 | 6 | M | RV-A9 fails in Python | WebSocket in az_live; RV-A9 passes |
| RV-B1 | 6 | S | - | WARP offscreen probe draws |
| RV-B2 | 6 | L | RV-B1, RV-A2 | az_viewnative ticks and draws; observer rule |
| RV-B3 | 6 | L | RV-B2 | instanced rendering for millions |
| RV-B4 | 6 | M | RV-B3 | cli_viewnative_offscreen passes without a window |
| RV-B5 | 6 | M | RV-B4 | 1M creatures: FPS, upload and slowdown recorded |
| RV-B6 | 6 | S | RV-B5 | docs |
| ES-0 | 6 | S | - | baseline table; kernel 1 gain at 100k measured |
| ES-1 | 6 (M2) | L | W2, ES-0 | docs/eval/M2-CACHE-TABLE.md with the bottleneck per stage |
| ES-2 | 6 (M3) | S | ES-1 | v61; hashes equal; grid faster |
| ES-3 | 6 (M3) | M | ES-1 | v67; hashes equal; grid faster |
| ES-4 | 6 (M3) | M | ES-1 | v69; hashes equal; field faster |
| ES-5 | 6 (M3) | S | ES-1 | v70; hashes equal |
| ES-6 | 6 (M3) | S | ES-1 | v62; one split policy; hashes equal |
| ES-7 | 6 (M3) | M | ES-1 | empty-stage cost before and after |
| ES-8 | 6 (M3) | L | ES-1 | TR-17 parallel child building; hashes equal |
| ES-9 | 6 (M3) | L | ES-1 | tier1 at 1M measured and reduced; hash equal or recorded design change |
| ES-10 | 6 (W3) | L | ES-1 | scan twin bit-exact on 1,000,000 pieces; hashes equal |
| ES-11 | 5 (W2) | L | physics | physics twin per its brief |
| ES-12 | 6 (M3) | XL | ES-1, O-8 | TR-15 bit-identical shared brains; speed before and after |
| ES-13 | 6 (M3) | M | ES-1 | per-stage P and E table |
| ES-14 | 8 (E3) | L | A1, ES-15 | docs/eval/BENCH-SCALE.md, every row locked, refusals and faults |
| ES-15 | 6 | S | - | docs/BENCH.md |
| ES-16 | 9 or on grant | S | O-11 | TR-03 numbers |
| SC-0 | 6 (W3) | S each | - | v23, v26, v57, v25 fixed with tests |
| SC-1 | 9 (S1) | XL | gate, O-10 | TR-01 and TR-02 done-when |
| SC-3 | 9 (S2) | XL | gate, SC-0 | TR-04, TR-05 done-when |
| SC-4 | 9 (S3) | M | gate | TR-06 probe recorded |
| SC-5 | 9 (S4) | L | two substrates | TR-07 table |
| SC-6 | 9 (R2) | L | SC-5 | TR-08 done-when |
