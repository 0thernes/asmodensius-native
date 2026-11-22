# Tsotchke audit: quantum-quake

Agent key: `quantum-quake`. Date: 2026-11-17. Source read-only at `T:\mirrors\quantum-quake`;
scratch at `S:\tsotchke\quantum-quake\`. Everything marked "ran" was run in this session.

## 1. Repo facts

| Item | Value |
|---|---|
| HEAD | `904094a_2026-10-04` (`git -C T:/mirrors/quantum-quake log -1 --format=%h_%cs`); 5 commits in the mirror |
| Licence | **GNU GPL version 2** (`quake/LICENSE.txt`, 338 lines; there is no root LICENSE file). `world.c` and `gl_model.c` carry the "any later version" wording; the QGE files (`qge/*.c`, `qge_hooks.c`, `snd_quantum.c`) have no header at all, and README line 129 states the QGE layer is "distributed under the same terms". Treat all of it as GPL-2.0. |
| What it is | A fork of **QuakeSpasm** (the Quake 1 engine, not id Tech 3) under `quake/`, plus a "Quantum Game Engine" layer `qge/` (16 C/H files and one Objective-C++ file, 12,433 lines by `wc -l`) that calls the Moonlab state-vector simulator. |
| Main language | C (212 `.c`, 282 `.h`), 87 Python tools (evidence/audit scripts), 1 `.mm` (Metal). |
| Size | 105 MB on disk, 834 tracked files (most of it vendored SDL frameworks and codec libraries). |
| Submodule | `deps/moonlab` is **empty in this mirror** (not checked out). `qge_ai.c`, `qge_physics.c`, `qge_vis.c`, `qge_rng.c`, `qge_render.c`, `qge_audio.c`, `qge_init.c` include `../deps/moonlab/src/...` and cannot be compiled here as they stand. |
| Validated platform | macOS / Apple Silicon (Metal, Accelerate) per README; the Linux branch is "not routinely verified"; no Windows path. |

**Legal consequence.** GPL-2.0 code cannot be copied, ported line by line, or linked into an
MIT-licensed engine. Verdict for the repo: **reference_only**. Every item below is an idea to
re-implement from a written description, never a source to vendor. The copies in my scratch
folder exist only to run tests and must not be moved into `N:\`.

## 2. What the QGE "quantum-classical physics" actually computes (from reading the code)

- **Projectiles** (`qge_physics.c:82-700`). No quantum state is involved. The "branch state"
  is three float-weighted candidates (classic result, QGE prediction, impact observation).
  Weights come from a shadow-error telemetry formula (`coherence = 1 - 0.35*avg_err_norm -
  0.65*max_err_norm`), the largest weight wins, and a measured impact always wins. It is a
  gated arbiter between two classical integrators with a 64-bit state hash for the trace.
- **Particles** (`qge_physics.c:746-1249`). Each particle is integrated classically (gravity,
  drag, Euler step). In parallel an 18-qubit state (64x64x64 grid, 262,144 complex doubles =
  4 MB) receives a Gaussian packet per spawn, a height-dependent phase, a small `Ry` on every
  qubit ("spreading"), a uniform decay and a renormalisation. That is not a Schrödinger
  kinetic step (no Laplacian, no FFT split-operator); the state is `O(2^18)` per particle
  spawn and per frame and is decoration over the classical positions.
- **Visibility** (`qge_vis.c:411-735`). The oracle is the classical answer (frustum test or
  the host's already-computed visible set). A simulated Grover iteration amplifies the marked
  indices; when the visible set is the majority, a hand-written "amplitude redistribution"
  overwrites amplitudes (visible set to a common magnitude, invisible to 1 %) and renormalises,
  which is not a unitary. No information is produced that the classical oracle did not already
  hold. The one correct numerical detail: diffusion is done over the active `2^k` subspace
  (`grover_diffusion_subspace`, 446-474), i.e. "reflect amplitudes about their mean".
- **AI** (`qge_ai.c:467-697`). Per decision a 3-qubit register gets `H` on each qubit,
  `Ry(theta0)` on q0, `Ry(theta1)` on q1, optionally a second `H` on q2, optionally `Rz` on
  q1, then is measured and the 3 bits index an action table. There is no entangling gate in
  the decision path (the CNOT/CZ in `qge_ai_entangle`, 718-744, act on a separate 24-qubit
  register that the decision path does not read). **Ran:** the distribution equals a product
  of three independent biased coins to 6.7e-16 (section 4, test B).
- **RNG** (`qge_rng.c`, `qge_quantum_runtime.c:48-64, 389-395, 688-752`). Primary source is
  Moonlab's `qrng_v3` behind a pthread mutex with a 256-entry batch; fallback and the
  deterministic mode are splitmix64 on a single mutable state that is XOR-salted with
  `(domain << 56) ^ (subject * FNV prime)` before each draw. It is **sequential and
  order-dependent**, not counter-based (ran: test A).
- **Runtime/trace** (`qge_quantum_runtime.c`, `qge_trace.c`). The sound engineering in this
  repo: every entropy draw, measurement and AI decision is written to a binary trace with
  (frame, server time, domain, subject, request id, offset); a replay mode feeds the values
  back and, in strict mode, counts any metadata mismatch and falls back.
- **Ownership ladder** (`qge_physics.c:82-215`, `qge_vis.c:253-370, 953-1198`). A new path
  runs in shadow against the classical path; it may write back only after N warm-up frames,
  M samples, and error under thresholds; any breach drops it back with a named reason.

## 3. Candidate table

All "ours" versions are clean-room re-implementations. Line ranges are in the mirror at HEAD.

| # | Idea | Where (file:lines) | Signatures there | Deps | Pure / stateful | Float / int | Thread safety | Maps to |
|---|---|---|---|---|---|---|---|---|
| 1 | Zero-run-length visibility rows + leaf lookup (Quake PVS) | `quake/Quake/gl_model.c:108-200` | `mleaf_t *Mod_PointInLeaf(vec3_t, qmodel_t*)`, `static byte *Mod_DecompressVis(byte*, qmodel_t*)`, `byte *Mod_LeafPVS(mleaf_t*, qmodel_t*)` | whole Quake model loader | decompress writes one global buffer (stateful) | integer bytes; leaf lookup is float plane tests | not thread-safe (shared static buffer) | (j) codec, (l) sector visibility |
| 2 | Shadow -> advisory -> authoritative gate | `qge/qge_physics.c:82-215`, `qge/qge_vis.c:253-370` | `qge_projectile_authority_state_t qge_projectile_authority_evaluate(const gate*, const telemetry*)`, `bool qge_vis_get_writeback_decision(...)` | none beyond `qge.h` types | physics gate is pure; vis gate uses file statics | float thresholds, int counters | physics: safe; vis: not | (j) receipts, (l) kernel validation |
| 3 | Entropy/measurement trace with strict replay | `qge/qge_quantum_runtime.c:250-420, 507-620, 688-752`; `qge/qge_trace.c` (244 lines) | `uint64_t qge_quantum_entropy_u64(rt, domain, subject_id)`, `int qge_quantum_runtime_load_replay_entropy(rt, path)`, `void qge_quantum_runtime_set_replay_strict(rt, bool)` | libc only | stateful (runtime object, FILE) | integer | not thread-safe (one runtime, no lock) | (j) receipts |
| 4 | 3-qubit decision register | `qge/qge_ai.c:273-306, 467-492, 626-697` | `ai_action_t qge_ai_decide_traced(const qge_ai_decision_input_t*, qge_ai_decision_trace_t*)` | Moonlab state/gates/measurement | stateful (static registers, per-enemy slots) | double complex | not thread-safe | (c), but see section 6 |
| 5 | BSP hull point query, swept hull trace, fixed-depth area tree | `quake/Quake/world.c:195-245, 431-502, 520-543, 605-735` | `areanode_t *SV_CreateAreaNode(int depth, vec3_t mins, vec3_t maxs)`, `int SV_HullPointContents(hull_t*, int num, vec3_t p)`, `qboolean SV_RecursiveHullCheck(hull_t*, int, float, float, vec3_t, vec3_t, trace_t*)` | Quake server globals | point query pure; area tree is global linked lists | float (double dot product for non-axial planes) | queries read-only; linking not | (l) spatial partition ideas |
| - | Fixed-point wire coordinates | `quake/Quake/common.c:762-800` | `void MSG_WriteCoord16(sizebuf_t*, float)` (13.3), `MSG_WriteCoord24` (16.8), int32 x16, 8/16-bit angles | none | pure | float in, int out | safe | (b) idea only; we are already integer |
| - | Subspace Grover diffusion | `qge/qge_vis.c:446-474` | `static void grover_diffusion_subspace(quantum_state_t*, uint64_t)` | Moonlab state | mutates state | double complex | not | (c) idea only |

Not candidates: `mathlib.h:69` holds the well-known reciprocal-square-root bit trick and
`mathlib.c:466` a 16.16 inverse helper; both are float-domain renderer helpers, GPL, and our
engine state is integer.

## 4. Builds and runs (this session)

Copied for test only: `qge/qge_quantum_runtime.{c,h}`, `qge/qge_trace.{c,h}` into
`S:\tsotchke\quantum-quake\src\`. Tests B and C contain no repo code.

### Test A: copied QGE runtime (candidate 3)

```
cd /s/tsotchke/quantum-quake
gcc -std=gnu2x -O2 -Wall -Wextra -o t_runtime.exe t_runtime.c src/qge_quantum_runtime.c src/qge_trace.c -lm
./t_runtime.exe trace.jsonl
```

Compiled with no warnings (gcc 16.1.0, mingw-w64). Output:

```
same_seed_identical=1
frames_where_subject1_value_changes_when_call_order_swaps=250/250
replay_load_rc=0 loaded=1000 consumed=1000 mismatches=0 exhaustions=0 replay_identical=1
swapped_replay mismatches=500 (expect 500)
first_values d88db3be47df017d c888784765048f49 db870d48b23a1fd2
qubits_for_basis_count(1,2,3,8,9,1024)=0 1 2 3 4 10
PASS (fails=0)
```

Reading: same seed gives the same stream; swapping the order of two requests in a frame
changes the value subject 1 receives in all 250 frames, so the generator is order-dependent
and would break our "same hash for any thread count" law; strict replay reproduces all 1000
values from a 68,048-byte binary trace (68 bytes per event) and flags all 500 out-of-order
requests.

### Test B: what the AI circuit computes (candidate 4), clean room

```
gcc -std=c2x -O2 -Wall -Wextra -o t_ai_equiv.exe t_ai_equiv.c -lm
./t_ai_equiv.exe
```

```
cases=572
max |P_statevector - P_product_of_3_coins| = 6.661e-16
max |P_statevector - freq(integer threshold sampler, 200000 draws)| = 0.0028
PASS
```

A from-scratch 3-qubit state vector applying the gate sequence of `qge_ai.c:634-651` over 572
input combinations gives exactly `P(q0=1)=(1+sin t0)/2`, `P(q1=1)=(1+sin t1)/2`, and
`P(q2=1)=0` when the second Hadamard fires (H.H = I) or 1/2 otherwise; `Rz` has no measurable
effect. Three 16-bit integer threshold compares on one hashed counter reproduce it within
sampling noise.

### Test C: clean-room zero-run-length bitset codec (candidate 1)

```
gcc -std=c2x -O2 -Wall -Wextra -o t_pvs.exe t_pvs.c
./t_pvs.exe
```

```
clusters= 1  raw=819200  enc=10219  ratio=80.2x
clusters= 2  raw=819200  enc=14173  ratio=57.8x
clusters= 4  raw=819200  enc=21003  ratio=39.0x
clusters= 8  raw=819200  enc=35442  ratio=23.1x
clusters=16  raw=819200  enc=63362  ratio=12.9x
clusters=32  raw=819200  enc=118579  ratio=6.9x
clusters=64  raw=819200  enc=216842  ratio=3.8x
all-zero row: 4096 bytes -> 34 bytes
rows=1400 total raw=5734400 enc=479620  mismatches=0
PASS
```

1,400 synthetic 32,768-bit rows (clustered set bits, my own generator, not Quake map data)
round-trip exactly; `popcount(row AND mask)` computed directly on the compressed row matches
the dense reference on every row; a truncated run and an over-long run are rejected (the
original only warns on overrun). Ratios are for this synthetic clustering only. No timing taken.

### Not built

The full QGE library, `tests/test_qge.c`, the Quake host and the Metal path: the Moonlab
submodule is empty in the mirror, the Makefile targets macOS/Linux with clang and GNU make
(GNU make is not available here), and SDL/Metal are out of scope.

## 5. Proposed integration (all clean room, C23, integer)

| Ours | Signature | Where it runs | Causal role |
|---|---|---|---|
| Sector visibility rows | `size_t an_zrle_encode(const uint8_t *src, size_t n, uint8_t *dst, size_t cap);` `size_t an_zrle_decode(const uint8_t *src, size_t len, uint8_t *dst, size_t n);` `uint32_t an_zrle_and_popcount(const uint8_t *row, size_t len, const uint8_t *mask, size_t n);` | **World fields / Tier 2.** Rows are rebuilt per world sector when terrain changes (slow path); read in the Tier 1 perception phase to skip sectors a creature cannot see. Same codec for sparse occupancy bitmaps in snapshots (tooling). | Lesion: replace rows with all-ones and perception cost and predator detection range change measurably. Rows are static per epoch, so thread count cannot affect results. |
| Kernel promotion gate | `typedef struct { uint32_t warm_left, clean_run, mismatches, false_neg; uint8_t ready, reason; } an_gate;` `bool an_gate_step(an_gate *g, uint32_t mism, uint32_t false_neg, uint32_t need_clean);` | **Tooling and Tier 1 boundary.** Each optimised kernel (AVX2, NASM, GPU) runs in shadow against the C reference on a sampled block; it may own the write-back only after `need_clean` consecutive bit-exact blocks, and one mismatch demotes it and logs the reason. | Not simulation physics; it is the evidence mechanism that keeps fast paths bit-identical to the reference. Because our state is integer, the threshold is zero mismatches, not a float error bound. |
| Divergence receipt | `typedef struct { uint64_t tick; uint32_t domain, subject; uint64_t ctr, value; } an_draw_rec;` `int an_receipt_check(const an_draw_rec *expect, const an_draw_rec *got);` | **Tooling only** (debug builds, sampled creatures). | With Philox the values never need replaying; the useful part is the strict metadata comparison, which pinpoints the first tick, domain and subject where two runs diverge. |
| Three-threshold action draw | `static inline uint8_t an_decide3(uint64_t draw, uint16_t t0, uint16_t t1, uint16_t t2);` (bit i = `((draw >> 16*i) & 0xffff) < t_i`) | **Tier 0**, one Philox draw per decision. | Only if the thresholds come from genome and plastic state. This is the honest classical equivalent of the repo's "quantum AI"; it is three biased coins and nothing more. |

Spatial partition (candidate 5): do **not** adopt a BSP. A static plane tree fits a fixed
level, not a deformable world with 10^5 to 10^6 movers; a uniform or Morton-ordered grid with
SoA cell lists is the cache-resident choice. Two ideas are worth keeping in mind: (i) the
axis-aligned fast path, where a split on a coordinate axis is a single integer compare with
no dot product (in an integer grid every split is axial, so the whole point query is compares
and shifts); (ii) the fixed-depth area tree (depth 4, 31 nodes, split on the longer of x/y at
the midpoint, objects linked at the deepest node that fully contains them) as a coarse
broad-phase for objects much larger than a grid cell (colony structures, fleets).

## 6. NOT usable, and why

1. **All source text.** GPL-2.0 throughout; nothing may be vendored or transliterated into an
   MIT engine. Applies equally to the QuakeSpasm host and the QGE layer.
2. **The RNG path.** `qrng_v3` lives in Moonlab (another repo, absent here, not examined by
   me), sits behind a global mutex, and `qge_rng.c:94-99` runs a "Bell test" on a simulated
   state at init and warns if CHSH < 2.4; the deterministic mode is sequential splitmix64 with an order-dependent
   salt (ran: test A). It violates law 6 (counter-based, thread-count invariant) and its
   "quantum" labelling would violate law 7. Philox4x32-10 stays.
3. **The quantum particle field.** 4 MB of complex doubles per system, `O(2^18)` work per
   spawn and per frame, float, no conservation, and it does not drive the particle positions
   (classical Euler does). Fails laws 2, 4, 5, 6.
4. **Grover visibility.** The oracle is the classical answer; the simulation costs `O(N)` per
   iteration on top of the `O(N)` classical test, and the majority-case "transfer" is not
   unitary. No causal content; claiming a speed-up would violate law 7.
5. **The AI register as a "quantum module".** Proven here to be three independent coins. A
   2 to 6 qubit per-creature module is only worth its bytes if the circuit contains entangling
   gates so that the joint distribution is not a product, and if a lesion (removing the
   entangler) changes behaviour. This repo provides no such circuit in its decision path.
6. **Projectile branch weights.** Float heuristics with magic constants (0.35/0.65, 0.15, 1.5,
   4.0); nothing to port. Only the gate pattern (candidate 2) is of interest.
7. **Render, audio, Metal, wavelet scene encoding** (`qge_render.c`, `qge_audio.c`,
   `qge_metal.mm`). Not read in depth; macOS/Metal specific, float, and tied to Quake assets.
   No creature-skin shader idea was extracted. Unknown beyond that.
8. **Quake float math helpers and wire quantisation.** Our state is already integer fixed
   point; the 13.3 / 16.8 coordinate packing only confirms common practice.
9. **The 87 Python tools.** Evidence and release-gate scripts for this specific game; the
   habit (machine-readable claims ledger, fail-closed gates) is good, the code is not relevant.

## 7. Verdict

**reference_only.** Licence blocks reuse; the quantum layer has no kernel with a causal role
for us. Three ideas are worth a clean-room implementation: zero-run-length visibility rows
(tested here), the zero-mismatch promotion gate for optimised kernels, and strict-metadata
divergence receipts.

Files in scratch: `S:\tsotchke\quantum-quake\t_runtime.c`, `t_ai_equiv.c`, `t_pvs.c`,
`src\` (GPL copies, test only), `trace.jsonl` (binary trace despite the name).
