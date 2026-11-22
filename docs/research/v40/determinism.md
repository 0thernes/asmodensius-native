# v40 lab: deterministic machinery (RNG, stream keying, binary artifacts, WASM)

Agent key: `determinism`. Date: 2026-11-17. Source tree read: `V:\` (read-only, nothing modified).
Scratch: `S:\v40\determinism\`. Everything marked **ran** was run in this session with node v25.8.2;
commands and output tails are in section 7.

## 1. Philox4x32-10

**Where.** `V:\creatures\001\organism\cleanslate_foundation.ts` lines 25-61 (`mulHiLo32`, `philox4x32_10`).
Other copies: a bundled copy in `V:\lab\affect-panel\panel.js` line 5965; a WASM version assembled in memory by
`V:\creatures\001\organism\wasm_kernels.ts` (section 5); an independent Python port
`philox4x32_10_py` in `V:\pipeline\independent_scientific_audit.py` line 251.

**Algorithm as written in the lab** (identical in structure to Random123 `philox4x32` with 10 rounds):

```
x = ctr[0..3]; k = key[0..1]
repeat 10 times:
    (hi0, lo0) = 0xD2511F53 * x0      (32x32 -> 64)
    (hi1, lo1) = 0xCD9E8D57 * x2
    x = { hi1 ^ x1 ^ k0,  lo1,  hi0 ^ x3 ^ k1,  lo0 }
    k0 += 0x9E3779B9;  k1 += 0xBB67AE85     (mod 2^32)
return x
```

The lab bumps the key after every round, including the last. The tenth bump is never used, so the output equals the
reference (which bumps between rounds only). The 64-bit product is built from 16-bit halves in float64
(`mulHiLo32`) because JavaScript has no 64-bit integer multiply; the WASM version uses `i64.mul`.

**Known answers (ran).** The lab function reproduces all three published Random123 Philox4x32-10 answers:

| counter | key | lab output | published | result |
|---|---|---|---|---|
| 00000000 x4 | 00000000 x2 | 6627e8d5 e169c58d bc57ac4c 9b00dbd8 | same | MATCH |
| ffffffff x4 | ffffffff x2 | 408f276d 41c83b0e a20bc7c6 6d5451fd | same | MATCH |
| 243f6a88 85a308d3 13198a2e 03707344 | a4093822 299f31d0 | d16cfe09 94fdcceb 5001e420 24126ea1 | same | MATCH |

"Published" here means the three values given in the task brief; I did not fetch the Random123 `kat_vectors` file
(no downloads allowed).

**Test vectors.** `N:\tests\vectors\philox_v40.json`, 16 entries, format
`[{"ctr":[4 hex],"key":[2 hex],"out":[4 hex]}]` (8 lower-case hex digits per word, word 0 first).
Produced by `S:\v40\determinism\philox_v40.mjs`, which contains the lab function text with only the TypeScript type
annotations removed. Each vector was also checked against a second implementation in the same script that uses
BigInt 64-bit products and the reference key schedule: 0 of 16 differ.

Contents: 1-3 the Random123 inputs; 4-7 a single 1 in counter word 0, counter word 3, key word 0, key word 1;
8 all high bits; 9 a half-word carry pattern; 10-11 the first and last iteration (i = 0, i = 511) of the lab's own
WASM-parity loop; 12-16 real lab stream keys from `PhiloxStream` (section 2), including the largest field values.

## 2. How streams are keyed

There are two generators in the lab, and they are used very differently.

### 2a. `PhiloxStream` (counter-based, stateless)

`cleanslate_foundation.ts` lines 16-82. Construction: `new PhiloxStream(masterSeed)` stores `masterSeed >>> 0`.
A draw is addressed by five integers and has no state:

```
draw(domain, entityId, organId, tick, drawIdx):
    key0 = masterSeed XOR (domain * 0x85EBCA6B mod 2^32)
    key1 = (entityId + 1) * 0xC2B2AE35 mod 2^32
    ctr  = { tick, organId, drawIdx, domain }
    out  = philox4x32_10(ctr, key)
uint32()  returns out[0]
uniform() returns out[0] / 2^32        (float64 in [0,1))
```

Domains (`PRNG_DOMAINS`): `WORLD_EXOG = 1`, `ENTITY_INT = 2`, `BENCH_EVAL = 3`, `UI_RENDER = 4`.

Facts a port must keep or consciously change:

- Only word 0 of each 128-bit block is used. Words 1-3 are discarded, so every draw costs a full 10-round block.
- `domain` appears twice (in key0 and in counter word 3). `entityId` is only in key1; seed is only in key0.
- `(entityId + 1) * 0xC2B2AE35` is an odd multiplier, so it is a bijection on 32 bits: no two entity ids share key1.
  `entityId = 0xFFFFFFFF` gives key1 = 0.
- All fields are 32-bit. `tick` wraps at 2^32. There is no generation, species or world-cell field.
- **Usage in the lab is test-only.** `Organism` constructs `this.csf = new CleanSlateFoundation(o.seed)`
  (organism.ts line 754), which owns a `PhiloxStream`, but a search for `.philox.` across `creatures\001` and
  `lab\src` finds no call outside `apex_ultimate_suite.ts` (APEX_15 isolation check, seed 401) and
  `scientific_rigor_suite.ts` (CHSH game, seed 2026050; causal-inference checks, seed 90901). The living
  creature's dynamics do not draw from Philox.

### 2b. `mulberry32` (sequential, 32-bit state) - the generator the creature actually runs on

Definitions: `V:\lab\src\rng.ts` line 1 and `V:\creatures\001\organism\organism.ts` line 21 (same arithmetic;
about 25 further inline copies in test and prototype files, found by the constant `0x6D2B79F5`).

```
state a (uint32) = seed >>> 0
next():  a += 0x6D2B79F5
         t = a;  t = (t ^ (t >>> 15)) * (t | 1)
         t ^= t + (t ^ (t >>> 7)) * (t | 61)
         return ((t ^ (t >>> 14)) >>> 0) / 2^32
getState()/setState(): the single uint32 a   (used by snapshot()/restore())
```

Streams are separated by giving each consumer its own generator seeded with `seed * P + c`, with a different
(prime, offset) pair per consumer. From `organism.ts` (constructor and lazy initialisers):

| consumer | seed expression |
|---|---|
| main stream `this.r` | `seed*7919 + 17` |
| entropy stream `this.ent` (default "seeded") | `seed*104729 + 3` |
| developmental genome / growth | `seed*5167 + 23` |
| synapse kinds, `Mind` init | `seed*31337 + 5` |
| cortical module wiring (v3.4) | `seed*7777 + 11` |
| cross-module E->E (v3.5) | `seed*6113 + 29` |
| signed cross-module input (v4.0) | `seed*4409 + 23` |
| patchy long-range axons (v3.9) | `seed*3301 + 17` |
| corticothalamic feedback (v3.6) | `seed*9973 + 5` |
| `this.rd` | `seed*4441 + 9` |
| affect stream / persona at birth | `seed*4051 + 11` / `seed*4049 + 7` |
| sensory bus, self-model (v4.0) | `seed*6007 + 41` |
| EFM / TPC / CFM / L5 / thalamocortical / dendritic-multiplication modules | `seed*7919+13`, `seed*7919+11`, `seed*6007+29`, `seed*3491+7`, `seed*5153+11`, `seed*5099+41`, `seed*2741+3` |
| Gaussian fallback when `noise != "quantum"` | `seed*7919 + 40` (re-created per call) |

Lab side (`V:\lab\src`): world dice `mulberry(seed*2654435761)` (`god.ts`), Moonlab-probability dice
`mulberry(seed*2654435761 + 17)`, dome `mulberry(cfg.seed)`, ecology passes `mulberry(dome.cfg.seed*4999 + offset)`,
adapter streams `seed*4073+3`, `seed*9137+29`, `seed*8831 + i*97 + 13`, `seed*7901+19`, `seed*6151 + i`.

Properties of this scheme:

- Isolation is by convention only. Two consumers collide if `seed1*P1 + c1 == seed2*P2 + c2 (mod 2^32)`;
  nothing checks this. One exact collision exists in the source: `seed*7919+17` seeds both `this.r`
  (organism.ts line 713) and the v6 projection initialiser (line 420), so those two start from the same state.
- Each stream is order-dependent: one extra or missing draw shifts every later value in that stream. This is the
  opposite of the Philox contract and is why it cannot give thread-count-independent results.
- The seed expression is evaluated in float64 and then wrapped with `>>> 0`. For `seed*2654435761` the product
  passes 2^53 once `seed > 3393263` (2^53 / 2654435761), and can then be rounded before the wrap. **ran:**
  `4000001*2654435761` gives `...760` in float64 (exact `...761`), wrapped `0x90abb2b0` instead of `0x90abb2b1`.
  A native port that wants the same stream must reproduce the float64 multiply, not a 64-bit integer multiply.
- Entropy source switch (`o.entropy`): `"seeded"` (default, the `seed*104729+3` stream); `"tsotchke"` reads
  pre-generated bytes from `$TSOTCHKE_QRNG_DIR/<seed>.bin`, 4 bytes little-endian per draw, XOR
  `wraps*0x9E3779B9` on wrap-around, falling back to the seeded stream when the file is missing
  (`tsotchke.ts`; the file header itself says this is a classical simulation, not hardware randomness);
  `"crypto"` uses `crypto.getRandomValues` and is **not** reproducible.
- "Quantum" noise (`noise: "quantum"`, the default): a 3-qubit state vector `Q3` gives 8 Born probabilities;
  `measure(u)` picks an outcome by inverse CDF with `u` taken from the entropy stream. The randomness is the
  seeded classical stream; only the outcome distribution comes from the circuit.

Reference values for a port (**ran**, `S:\v40\determinism\mulberry_v40.mjs`; outputs shown as the uint32 before
the division by 2^32):

| call | initial state | first four outputs |
|---|---|---|
| `mulberry(0)` | 00000000 | 4434b462 00159c37 39285b08 256d8104 |
| `mulberry(1)` | 00000001 | a087eaf3 00b349c9 8706c4eb fb2627fd |
| `mulberry(12345)` | 00003039 | facf78c5 4e875100 7bf4e2f2 d1642650 |
| organism main stream, seed 1000 | 0078d5a9 | e81a8345 7281660f 0fb93c38 cac04769 |
| organism entropy stream, seed 1000 | 063e09ab | 691e3099 0dc2209e 262f4faf 2dc1ee6b |
| `seededDice(7)` | 538453d7 | e63e78d1 091e5113 8816f47d 3a3f771d |

## 3. `soa_snapshot.alifebin` (2060 bytes)

Writer: `SoAArena.toAlifeBin()` (`cleanslate_foundation.ts` lines 289-299). Reader: `SoAArena.fromAlifeBin()`
(lines 301-311). The committed file is produced by `scientific_rigor_suite.ts` lines 1333-1337:
`new SoAArena(32)`, `stepMultirate(400)`, `toAlifeBin()`.

All fields little-endian.

| offset | size | type | field | value in the committed file |
|---|---|---|---|---|
| 0 | 4 | u32 | magic `0x414C4633` | bytes on disk `33 46 4C 41` = ASCII "3FLA" (the code comment says 'ALF3'; that is the big-endian reading) |
| 4 | 4 | u32 | `count` N | 32 |
| 8 | 4 | u32 | `tickCount` | 400 |
| 12 | 8N | f64[N] | column 0 `x` | bytes 12..267 |
| 12 + 8N | 8N | f64[N] | column 1 `y` | 268..523 |
| 12 + 16N | 8N | f64[N] | column 2 `vx` | 524..779 |
| 12 + 24N | 8N | f64[N] | column 3 `vy` | 780..1035 |
| 12 + 32N | 8N | f64[N] | column 4 `vMem` | 1036..1291 |
| 12 + 40N | 8N | f64[N] | column 5 `uRec` | 1292..1547 |
| 12 + 48N | 8N | f64[N] | column 6 `atp` | 1548..1803 |
| 12 + 56N | 8N | f64[N] | column 7 `phase` | 1804..2059 |

Total length `12 + 64*N`. Column-major (structure of arrays): the body is the raw `ArrayBuffer` behind eight
`Float64Array` views, so it is a plain memory image of the arena.

Things the lab's own docs get wrong or leave out (checked against the code and the file):

- **No checksum in the file.** `V:\docs\API-AND-EVENTS.md` describes "+ FNV-1a verification". The FNV-1a value
  (`c49cdf0f`, 32-bit FNV-1a over all 2060 bytes) lives outside the file, in
  `results\sc-port\nobel_fields_alife_proofs.json` (`alifeBinHashFNV1a`), and is compared by the Python audit.
- **The reader does not reject truncated input.** `fromAlifeBin` checks only the magic. **ran:** a buffer cut to
  112 bytes is accepted and the missing entities keep the constructor's default values (x[31] = 8.75).
- No version field, no column count, no column names, no endianness marker. `tickCount` wraps at 2^32.
- The values are float64 and the step uses `Math.cos`/`Math.sin`, so the bytes depend on the JavaScript engine's
  math library. **ran:** node v25.8.2 regenerates the committed file with 0 differing bytes, so it is stable
  across the node version that wrote it and this one; that is an observation, not a guarantee.
- Related: `encodeXorCheckpoint` (lines 313-333) stores a delta against a baseline `.alifebin` as records
  `[offset u24 LE][len u8][len XOR bytes]`, with no header and no checksum.

SHA-256 of the committed file: `703458c24dc88c5e48568d4c3d71c9c1c5f46477538324d7c986faacda6ed861`.

## 4. `omni_stream.sensorlog` (2184 bytes)

Writer: `OmniHarnessBus.exportSensorLog()` (`cleanslate_foundation.ts` lines 1088-1102), fed by
`transduce(tick, raw16, record=true)` (lines 1066-1086). Reader: `replaySensorLogHash()` (lines 1104-1122).
The committed file is produced by `scientific_rigor_suite.ts` lines 1339-1347: 32 frames, tick t = 0..31,
input `cos(0.15*t + 0.5*j)` for channel j = 0..15, all 8 modalities online.

All fields little-endian.

| offset | size | type | field |
|---|---|---|---|
| 0 | 4 | u32 | magic `0x534C4F47` (bytes on disk `47 4F 4C 53` = "GOLS") |
| 4 | 4 | u32 | `frameCount` n (32 in the committed file; at most 256) |
| 8 + 68k | 4 | **f32** | frame k: `tick`, stored as a float |
| 12 + 68k | 64 | f32[16] | frame k: channels s0..s15 |

Frame size 68 bytes (17 float32 words). Total length `8 + 68*n`.

- **The lab's doc table is wrong about this format.** `API-AND-EVENTS.md` says each frame is
  `(tick: u32, activeMask: u32, channels[32]: f32)`. The code and the file say `(tick: f32, channels[16]: f32)`,
  with no active-mask word. File check: frame 1 tick word is `0x3F800000` (1.0f), frame 31 is `0x41F80000` (31.0f);
  2184 = 8 + 32*68.
- Tick as float32 is exact only up to 2^24 = 16,777,216.
- Channel value = `clamp(raw * modalityGain[m] * sqrt(boost), -4, 4)` rounded to float32, where modality
  `m = (i >> 1) & 7` (two channels per modality), `boost = min(1.6, 8/activeCount)`; a dropped modality writes 0.
  Non-finite input is replaced by 0.
- The source is a ring of 256 frames; export writes the most recent n in chronological order.
- No checksum in the file. The Python audit compares a SHA-256 stored in the proofs JSON
  (`0ffd065de386e0d032207e1a7acc416beb4b9448a8163c336813803153ab60e1`, **ran**: matches the file).
- The reader checks the magic only. A truncated buffer fails by accident (**ran:** `RangeError` from `DataView`),
  an over-long one is accepted. The replay "state hash" is a 32-bit FNV-1a over a float64 leaky integrator
  `acc = 0.85*acc + 0.15*tanh(s)`, so it depends on the platform `tanh`; **ran:** it reproduces `475bd8c9` here.
  The lab's `docs\RISKS.md` R-03 already records this cross-runtime `libm` risk.
- **ran:** node v25.8.2 regenerates the committed file with 0 differing bytes.

## 5. The WASM kernels

There are two `.wasm` files in the tree and one module that never touches disk.

### 5a. `V:\creatures\001\wasm\moonlab.wasm` (1,472,827 bytes)

Emscripten build of tsotchke/moonlab (README: commit cd3b234, MIT), a quantum-circuit **simulator** on a classical
CPU. 451 exported functions (**ran**: listed with `WebAssembly.Module.exports`): state-vector init/reset/clone,
single- and two-qubit gates, QFT, measurement probabilities, entanglement measures, noise channels, MPS/tensor
network, Grover, VQE, QAOA, DMRG, Bell tests. Its WebGPU imports are scaffolding (README).

What the lab actually computes with it:

- `backends.ts` `moonlabBackend`: a 3-qubit state `RY(th_q)` on each qubit, `CNOT 0->1`, `CNOT 1->2`,
  `RZ(0.7*th_q)` on each qubit; returns the 8 Born probabilities via `quantum_state_get_probability`.
  Opt-in replacement for the creature's own TypeScript `Q3` circuit. Every measurement draw still comes from the
  seeded mulberry32 entropy stream.
- `lab\src\biosphere\moonlabDice.ts`: three `RY(theta)` qubits, 8 probabilities, outcomes drawn with
  `mulberry(seed*2654435761 + 17)`. Its own header labels it "simulated quantum, not a hardware QRNG".
- Recorded equivalence (`results\wasm_equiv_v36.json`, not re-run by me): max |probability difference| vs `Q3`
  6.66e-16 over 5000 circuits. That is agreement to rounding, **not** bit-identity; the two-day whole-organism
  runs were state-identical for seeds 1000-1003 because only an inverse-CDF comparison consumes the probabilities.
- **ran** (copy in scratch): `smoke.mjs` prepares a Bell pair and prints P(q0=1) = 0.500, P(q1=1) = 0.500,
  P(q2=1) = 0.000.

Under the owner's law 7 this is a classical simulation and must never be described as quantum randomness.

### 5b. `V:\creatures\001\wasm\eshkol-vm.wasm` (1,350,364 bytes)

Emscripten build of the tsotchke/eshkol bytecode VM / REPL (README: commit 341ac81, MIT; the interpreter, not the
LLVM native compiler). 13 exported functions, the useful ones being `repl_init`, `repl_reset`, `repl_eval`,
`run_program`. Four builtins are aborting stubs supplied as imports: `eshkol_qrng_uint64`, `eshkol_qrng_double`,
`eshkol_linear_solve`, `eshkol_capability_require`.

What the lab computes with it: one rule, the sleep/wake flip-flop, evaluated every tick as an Eshkol program
(`backends.ts` `FLIPFLOP_SRC`):

```
(define (flipflop asleep drive thr forced)
  (if asleep (if (or (< drive 0.2) forced) 0 1)
             (if (and (> drive thr) (not forced)) 1 0)))
```

Arguments are passed as text with 17 significant digits; the result is read back from captured stdout ("0"/"1").
Recorded equivalence: 0 mismatches over 2000 cases vs the TypeScript rule. **ran:** `smoke.mjs` prints `144`, `6`,
`(2 4 6)` for `(sq 12)`, `(+ 1 2 3)` and a `map`.

Both are opt-in; by default the creature uses neither. Both are interpreters/VM code and are excluded from the
native simulation by law 1 regardless.

### 5c. In-memory 518-byte module (`wasm_kernels.ts`), not a file

`assembleAsmodensiusWasmBytes()` emits a WebAssembly 1.0 module byte by byte. **ran:** 518 bytes, SHA-256
`3e4ebd678cfe3c734aa891a0420cd9becf8acd69f4c10a96445fd8072894df23` (equals `wasmSha256` in the lab's proofs JSON).
It imports `env.memory` (1 page) and exports three functions:

| export | signature | computes |
|---|---|---|
| `philox4x32_10` | `(outPtr, c0, c1, c2, c3, k0, k1: i32)` | the section 1 algorithm with `i64.mul`; writes 4 x u32 at `outPtr` |
| `dot_f64` | `(ptrA, ptrB, n: i32) -> f64` | `acc = 0; for i<n: acc = acc + A[i]*B[i]` in index order, no FMA, no reordering |
| `chemoton_net_flux` | `(concPtr, kPlusPtr, kMinusPtr, outPtr: i32)` | five net fluxes for c = [X, A1, A2, M, V, W] (f64): `k+0*A1*X - k-0*A2*W`; `k+1*A2 - k-1*A1*A1`; `k+2*A1 - k-2*M`; `k+3*A2 - k-3*V*W`; `k+4*M - k-4*W` |

**ran:** WASM Philox vs TypeScript Philox, 0 mismatches over the lab's 512-vector loop; WASM output for the zero
input equals the Random123 answer. These three are the only kernels in the lab with exactly specified arithmetic
(integer, or float64 add/sub/mul in a fixed order with no transcendental calls).

## 6. What this means for the native rebuild

1. The Philox core can be ported as is; `N:\tests\vectors\philox_v40.json` is the acceptance test. Vectors 12-16
   pin the lab's key derivation if the native code wants to stay compatible with `PhiloxStream`.
2. The lab's `PhiloxStream` wastes 3 of 4 output words and has only 32-bit tick/entity fields. For millions of
   creatures the native key/counter layout should be redesigned (which fields go where is an open design choice;
   the lab layout is a starting point, not a constraint, since nothing in the living creature depends on it).
3. The creature's real randomness is about 25 order-dependent mulberry32 streams seeded `seed*P + c` in float64.
   Bit-compatibility with v40 trajectories would require reproducing every stream, its draw order, and float64
   state throughout; that conflicts with laws 5 and 6 (integer state, thread-count independence). Treat v40
   trajectories as not reproducible by the native build; reproduce mechanisms, not bytes.
4. Neither binary format is fit to carry forward unchanged: no version, no embedded checksum, no length check on
   read, float64/float32 payloads, tick stored as float32 in `.sensorlog`. The lab's own format table
   (`API-AND-EVENTS.md`) disagrees with its code for `.sensorlog` and overstates validation for both.
5. The float pipelines reproduce on this machine across node versions, but they depend on `Math.cos`, `Math.sin`,
   `Math.tanh`; only the integer and plain add/mul kernels are exact by construction.

## 7. Evidence (commands run and output tails)

All from `S:\v40\determinism\` (Git Bash). Lab sources were copied there first; SHA-256 of the copies equals the
originals (`cleanslate_foundation.ts` cf68866a..., `wasm_kernels.ts` b91f1bba...).

`node philox_v40.mjs N:/tests/vectors/philox_v40.json`

```
16 ctr=ffffffff ffffffff ffffffff 00000004 key=e850d653 3d4d51cb out=62a79840 9f1207c4 d2adfbc9 57a0d2fc bigint_ref=same  # PhiloxStream max seed, UI_RENDER, entity 2^32-2, max organ/tick/draw
KAT 1: lab=6627e8d5 e169c58d bc57ac4c 9b00dbd8 published=6627e8d5 e169c58d bc57ac4c 9b00dbd8 MATCH
KAT 2: lab=408f276d 41c83b0e a20bc7c6 6d5451fd published=408f276d 41c83b0e a20bc7c6 6d5451fd MATCH
KAT 3: lab=d16cfe09 94fdcceb 5001e420 24126ea1 published=d16cfe09 94fdcceb 5001e420 24126ea1 MATCH
vectors=16 kat_match=3/3 bigint_ref_mismatch=0/16
wrote N:/tests/vectors/philox_v40.json
```

`node inspect_artifacts.mjs` (imports the copied, unmodified lab `.ts` files directly; node 25 strips types)

```
alifebin: bytes=2060 magic=0x414c4633 ascii_on_disk="3FLA" count=32 tick=400 expected_len=2060 fnv1a=c49cdf0f sha256=703458c2...
  regenerated with node v25.8.2: {"lenA":2060,"lenB":2060,"diffBytes":0,"first":-1} fnv1a=c49cdf0f
sensorlog: bytes=2184 magic=0x534c4f47 ascii_on_disk="GOLS" frames=32 expected_len=2184 sha256=0ffd065d...
  frame 1 offset=76 tick(f32)=1 tick_raw_u32=0x3f800000 ...
  frame 31 offset=2116 tick(f32)=31 tick_raw_u32=0x41f80000 ...
  replay hash (lab replaySensorLogHash) = {"frames":32,"stateHash":"475bd8c9"}
  regenerated with node v25.8.2: {"lenA":2184,"lenB":2184,"diffBytes":0,"first":-1} sha256=0ffd065d...
  fromAlifeBin(truncated to 112 bytes): accepted, count=32, x[31]=8.75 (constructor default kept)
  replaySensorLogHash(truncated): threw RangeError: Offset is outside the bounds of the DataView
inline wasm: bytes=518 sha256=3e4ebd67... exports=[philox4x32_10, dot_f64, chemoton_net_flux] imports=[env.memory]
  wasm philox vs TS philox mismatches=0/512; wasm KAT zeros=6627e8d5 e169c58d bc57ac4c 9b00dbd8
  wasm dot_f64(...) = -8.71875 (expected -8.71875)
eshkol-vm.wasm: bytes=1350364 sha256=bcf33682... exports=15 (functions=13) imports=37
moonlab.wasm: bytes=1472827 sha256=6976f231... exports=469 (functions=451) imports=37
```

`node mulberry_v40.mjs` - output is the table in section 2b.

`node smoke.mjs` (in `S:\v40\determinism\wasm_copy\`, a copy of `V:\creatures\001\wasm\`)

```
init 0
moonlab P1(q0) 0.500 P1(q1) 0.500 P1(q2) 0.000
144
6
(2 4 6)
eshkol:  |  |
```

`node -e "... 4000001*2654435761 ..."` -> `10617745698435760 10617745698435761 90abb2b0 90abb2b1`

**Not run / unknown:** the lab's own suites (`scientific_rigor_suite.ts`, `apex_ultimate_suite.ts`,
`wasm_equiv_v36.ts`) and the Python audit were read, not executed; their recorded results are quoted as recorded.
The Random123 `kat_vectors` file was not fetched. No timing was taken.
