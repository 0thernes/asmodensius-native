# GeoRefine — audit for engine reuse (key: georefine)

Audited 2026-11-17. Source read-only at `T:\mirrors\GeoRefine`; experiments in `S:\tsotchke\georefine\`.

## 1. Repo facts

| Item | Value |
|---|---|
| HEAD (`git -C T:/mirrors/GeoRefine log -1 --format=%h_%cs`) | `a7f1f70_2026-11-01` |
| License (LICENSE file) | Apache License 2.0; NOTICE: "GeoRefine codec software, Copyright 2026 Tsotchke Corporation" |
| Main language | Python (260 files, about 5.0 MB) with CUDA (`.cu/.cuh`, 50 files, about 0.73 MB) and C++ (24 `.cpp`, about 0.19 MB) kernels |
| Size | 392 tracked files, about 7.2 MB of tracked source and JSON; 32 MB on disk with `.git` |
| What it is | A bit-exact codec and serving runtime for BF16 LLM weights (Qwen 27B): TBE (fixed-rate exponent packing), BCTX (context rANS frames), PPCX (reference-conditioned rANS frames), CUDA GEMM that reads the packed form, an HTTP chat server, and a separate CPU verifier that emits a JSON receipt |

Most of the repo is LLM serving (weights, tokenizer use, FastAPI chat endpoint, MTP head). That part is
excluded by owner law 3 and is not discussed further. The entropy-coding kernels are plain integer code
with no model content; those are what this note evaluates, as **tooling for snapshots only**.

## 2. Candidate kernels

All four native candidates are integer-only, have no global state (re-entrant; safe to call from several
threads on disjoint buffers), allocate from the heap (`std::vector` / `malloc`), and depend only on the
C++ standard library. All export a C ABI (`extern "C"`).

| # | File (under `T:\mirrors\GeoRefine`) and lines | Signatures | Pure / stateful | Notes |
|---|---|---|---|---|
| K1 | `context-runtime/georefine_context_runtime/bitexact_predictive_ans.cpp` 1-13 (minified; `norm` l.9, `p_encode` l.10, `p_decode` l.11) | `int p_encode(const uint16_t*v,const uint16_t*cx,uint64_t n,uint32_t nc,uint32_t vocab,uint32_t**freqp,uint8_t**streamp,size_t*sn)`; `int p_decode(const uint8_t*st,size_t bytes,uint64_t n,const uint16_t*cx,uint32_t nc,uint32_t v,const uint32_t*f,uint16_t*out)`; `void p_free(void*)` | Pure (output depends only on inputs) | Generic static rANS: any symbol array `v[i] < vocab <= 65536` with any caller-supplied context `cx[i] < nc`. 32-bit state, 16-bit probability scale, byte renormalisation, lower bound 2^23. Deterministic largest-remainder frequency normalisation with index tie-breaks. `n <= 100,000,000`. Decoder checks final state and exact stream length. |
| K2 | `context-runtime/georefine_context_runtime/bitexact_context_ans.cpp`: `normalize` 17-67, `rans_encode` 75-109, `rans_decode` 110-160, `rans_decode_compact` 162-234, exports 235-359 | `int bcx_encode(const uint16_t*w,uint64_t n,uint32_t rows_n,uint32_t cols_n,const uint8_t*rc,const uint8_t*cc,uint32_t nr,uint32_t nc,uint32_t kind,uint32_t*freq,uint8_t**stream,size_t*stream_n)`; `int bcx_decode(...,const uint16_t*resctx,const uint32_t*freq,uint16_t*out)`; `bcx_decode_compact` (same arguments); `bcx_frequencies`; `bcx_pack6`/`bcx_unpack6`; `bcx_free` | Pure | Same rANS, but the 16-bit word split is hard-wired to BF16: main symbol = bits 14..5 (10 bits) coded under a (row class x column class) context, `nr <= 64`, `nc <= 16`; residual = bit 15 and bits 4..0 (6 bits) coded under the main symbol (1024 contexts). `bcx_decode` builds a `nctx x 65536` uint16 lookup (128 MB for the residual stream); `bcx_decode_compact` replaces it with 256 buckets per context plus a short binary search. |
| K3 | `context-runtime/glc_loader/codec_v21_native.cpp`: `t21_advance_row` 48-54, `t21_plan` 57-82, `t21_fill` 84-116, `t21_decode` 188-225 | `int t21_plan(const uint16_t*words,uint64_t words_n,uint64_t n,uint64_t k,const uint8_t*lut,uint64_t lut_n,uint32_t ncb,const uint8_t*sel,uint64_t sel_n,uint32_t*rowoff,uint64_t rowoff_n,uint64_t*stats,uint64_t stats_n)`; `t21_fill(...)`; `t21_decode(...)` | Pure | TBE21: fixed 10 bits per word (8 raw bits = sign + low 7, plus a 2-bit code) and a per-row overflow bit stream with four escape tiers (2, 2, 3, 8 bits). Up to 2 codebooks of 16 values for bits 14..7, one selector per row, `k % 128 == 0`, row offsets give random access per row. Refuses to compile without `-DT21_SOURCE_SHA256=...` (binds the source hash into the binary). Codebook selection is not in C (it is Python upstream). |
| K4 | `context-runtime/georefine_context_runtime/bitexact_context_gpu_index.cpp` 1-138 | `int bcx_checkpoint(..., uint32_t stride, uint32_t*states, uint32_t*offsets, uint16_t*out)` | Pure | Decodes once and records (rANS state, byte offset) every `stride` symbols, so later decodes can start at any checkpoint (parallel or partial decode). Read only, not built here. |
| K5 | `release/georefine-verify/georefine_verify/tbe.py` 1-205 and `verify.py` 319-380, 510-580; receipt example `proofs/qwen38-27b/bit_exact_receipt.json` | Python (numpy) | n/a | Independent decoder written without importing the codec, plus the receipt format. Python only, so idea only. |

## 3. What was built and run here

Sources copied unchanged to `S:\tsotchke\georefine\` (SHA-256 of the copies: `bitexact_context_ans.cpp`
483a6784..., `bitexact_predictive_ans.cpp` 0339ac11..., `codec_v21_native.cpp` 6d238370...). Test harness:
`S:\tsotchke\georefine\test_codecs.c` (C2x, calls the three kernels through their C ABI).

Test data: a synthetic integer snapshot, 8192 creatures x 128 uint16 fields (2,097,152 raw bytes), 8 field
classes (Q8.8 energy, age counter, sparse flags, signed velocity, RNG-like word, per-species constant, slow
ramp, wide field) and 4 species; plus a "previous tick" copy that differs by small per-class steps. The
generator is in the harness; it is synthetic, not real engine state.

Build (ran, no warnings printed):

```
cd /s/tsotchke/georefine
g++ -std=c++17 -O2 -Wall -Wextra -c bitexact_context_ans.cpp -o bcx.o
g++ -std=c++17 -O2 -Wall -Wextra -c bitexact_predictive_ans.cpp -o ppcx.o
g++ -std=c++17 -O2 -Wall -Wextra -DT21_SOURCE_SHA256=\"6d23837008b5966f483e49314ccd1b0205b16e46e985e90c001da436071a4fbc\" -c codec_v21_native.cpp -o t21.o
gcc -std=c2x -O2 -Wall -Wextra -c test_codecs.c -o test_codecs.o
g++ -O2 -static test_codecs.o bcx.o ppcx.o t21.o -o test_codecs.exe
./test_codecs.exe; echo "exit=$?"
```

Output (complete, last run):

```
data: 1048576 uint16 words = 2097152 raw bytes; order-0 entropy 11.034 bits/word = 1446297 bytes
T1 BCTX: main 504397 B + residual 590672 B streams; tables 5669+58454 nonzero entries (4 B each = 256492 B)
T1 BCTX: total 1351561 B, ratio 1.552x, roundtrip BIT-EXACT; enc 19 ms, dec(compact) 25 ms, dec kind1 full-LUT 143 ms (noisy, parallel load)
T4 corruption (1 byte flipped in main stream, 200 trials): decoder rejected 197, accepted wrong data 3, accepted identical 0
T2 delta+ctx rANS: hi 179600 B + lo 312183 B streams; tables 267+1210 nonzero entries (5908 B)
T2 delta+ctx rANS: total 497691 B, ratio 4.214x vs raw snapshot, roundtrip BIT-EXACT; enc 12 ms, dec 33 ms (noisy, parallel load)
T3 TBE21: tiers n1=558502 n2=437113 n3=376044 raw=281921 pad_bits=125110 overflow_words=171870
T3 TBE21: total 2032008 B, ratio 1.032x, roundtrip BIT-EXACT; plan+fill 6 ms, decode 8 ms (noisy, parallel load); src sha 6d23837008b5
T3b TBE21 class 0: 217994 B of 262144, ratio 1.203x, BIT-EXACT
T3b TBE21 class 1: 164938 B of 262144, ratio 1.589x, BIT-EXACT
T3b TBE21 class 2: 163922 B of 262144, ratio 1.599x, BIT-EXACT
T3b TBE21 class 3: 163922 B of 262144, ratio 1.599x, BIT-EXACT
T3b TBE21 class 4: 397662 B of 262144, ratio 0.659x, BIT-EXACT
T3b TBE21 class 5: 163922 B of 262144, ratio 1.599x, BIT-EXACT
T3b TBE21 class 6: 192594 B of 262144, ratio 1.361x, BIT-EXACT
T3b TBE21 class 7: 332070 B of 262144, ratio 0.789x, BIT-EXACT
ALL ROUNDTRIPS BIT-EXACT
exit=0
```

Reading the numbers (all on the synthetic data above; real engine state will differ):

- **K1 (generic context rANS) on tick-to-tick deltas: 4.21x**, bit-exact. Symbols were the high and low
  byte of `(cur - prev) mod 2^16`; high byte coded under the field class (8 contexts), low byte under
  (field class x {high byte 0, 255, other}) (24 contexts). One eighth of the test data is uniform random
  words that cannot be compressed (262,144 B of the 497,691 B total), so the compressible fields average
  about 2 bits per word. The table cost is counted (4 bytes per nonzero entry, a simple sparse layout I
  assumed, not a format from the repo).
- **K2 (BCTX as shipped) on one snapshot: 1.55x**, bit-exact, slightly better than the 1.45x a context-free
  order-0 coder could reach. Its BF16 bit split is a poor fit for integers: the residual table alone has
  58,454 entries and the full-table decoder for the residual stream allocates 128 MB.
- **K3 (TBE21): 1.03x on the mixed snapshot**, and between 0.66x and 1.60x per homogeneous field. 1.6x is
  its hard ceiling (10 bits per 16-bit word) and it expands data whose bits 14..7 take many values; there
  is no raw fallback in the kernel. Sparse flags (90 % zeros) still cost 10 bits per word.
- **Corruption (T4):** with one byte of the stream flipped, the rANS decoder's own end checks rejected
  197 of 200 trials and returned wrong data with a success code in 3. The decoder's exit status is not an
  integrity check; a hash over the frame is required (the repo does this in Python with SHA-256 over each
  frame and over the decoded tensor).
- One harness bug was found and fixed during the run (my codebook builder, not repo code): unused codebook
  slots were overwriting the rank of value 0. The output above is after the fix.
- Not tested: inputs larger than 1 M words, adversarial or truncated streams beyond single-byte flips,
  clang or MSVC builds, K4, and any CUDA code (no nvcc on this machine).

## 4. Mapping to engine needs

| Need | Result |
|---|---|
| (j) codecs for snapshots | K1 yes (adapt). K2 only as a worked example of splitting a word into a context-coded part and a conditioned residual. K3 only if fixed-rate random access matters more than ratio. |
| (j) verification receipts for evidence | K5 ideas, see section 6. |
| (g) tensor / GEMV kernels | None usable. The GEMM code is CUDA (and Metal) BF16 matrix multiply that decodes packed LLM weights inside the kernel; it needs nvcc, is float, and exists for LLM inference. |
| (a) (b) (c) (d) (e) (f) (h) (i) (k) | None. The repo has no RNG, fixed-point math, quantum simulation, representation theory, spin models, samplers, PDE solvers or rendering. |

Causal role: none inside the simulation. A snapshot codec changes no creature outcome; it is evidence and
storage tooling. It must stay out of Tier 0/1/2 loops.

## 5. Proposed integration

Where it runs: **tooling only** (snapshot writer/reader on an I/O thread, and the offline replay checker).
Never per tick.

Port K1 to C23 rather than linking the C++: about 120 lines, no `std::vector`, no exceptions, caller-owned
buffers from the arena, explicit capacity arguments. Proposed interface:

```c
// Static context rANS, 32-bit state, 16-bit scale, byte renormalisation. Integer only, re-entrant.
// freq is [nctx * vocab] uint32, each active row sums to 65536. Returns 0 on success.
int snap_rans_build(const uint16_t *sym, const uint16_t *ctx, uint64_t n,
                    uint32_t nctx, uint32_t vocab, uint32_t *freq);
int snap_rans_encode(const uint16_t *sym, const uint16_t *ctx, uint64_t n,
                     uint32_t nctx, uint32_t vocab, const uint32_t *freq,
                     uint8_t *dst, size_t dst_cap, size_t *dst_len);
int snap_rans_decode(const uint8_t *src, size_t src_len, const uint16_t *ctx, uint64_t n,
                     uint32_t nctx, uint32_t vocab, const uint32_t *freq, uint16_t *out);

// Snapshot frame for one SoA column block: codes (cur - prev) per byte plane under a
// (field id, byte plane, previous-plane bucket) context; falls back to raw when not smaller.
int snap_frame_write(const void *cur, const void *prev /* may be NULL */, uint32_t elem_bytes,
                     uint64_t count, uint16_t field_id, uint8_t *dst, size_t cap, size_t *len);
int snap_frame_read(const uint8_t *src, size_t len, const void *prev, void *out, uint64_t count);
```

Design points taken from the repo:

1. Contexts must be things the decoder already has (field id, species, the previously decoded byte plane,
   the previous snapshot). PPCX does exactly this with a reference tensor.
2. Try several forms per frame (context-coded, bit-packed, raw) and keep the shortest, so a frame never
   grows by more than its header (`scripts/bitexact_predictive_codec.py` 133-154).
3. Each frame header carries the hash of the reference it depends on and the hash of the decoded target,
   and ends with a hash of the frame itself (same file, 147-159). The engine should use its existing
   state hash here; the repo has no native hash code (SHA-256 is Python `hashlib`).
4. For parallel decode of large snapshots, store (state, offset) checkpoints every fixed stride (K4), or
   simply cut the snapshot into independent frames per L2-sized block; the second is simpler.
5. Decode tables must be the compact form (K2 `rans_decode_compact`, or K1's prefix array plus binary
   search); the direct inverse table is 128 KB per context.

If code is copied rather than rewritten, Apache-2.0 requires keeping the LICENSE and NOTICE text. rANS
itself is a published public technique (Duda's ANS; byte-wise rANS as in public-domain reference
implementations), so no novelty should be claimed for the port.

## 6. The verification-receipt approach (ideas, K5)

What the repo does, and what transfers to the engine's evidence rule (law 8):

- **Second implementation.** The verifier decodes with its own code and imports nothing from the codec or
  the serving engine. Engine equivalent: an offline checker that re-reads snapshots and recomputes state
  hashes without linking the simulation's fast path.
- **Receipt contents** (`proofs/qwen38-27b/bit_exact_receipt.json`): schema name, input manifest hash,
  per-item reference hash and decoded hash, `match`, `mismatched` count, `first_mismatch`, a coverage
  block (missing and extra items), a list of caveats that names every check that was skipped, the
  verifier's version and a hash of its own source files, argv, platform, wall time, and a single verdict.
- **Fail closed and say what was skipped.** A run in a weaker mode records that in `caveats` instead of
  passing silently.
- **Source hash bound at compile time.** `codec_v21_native.cpp` line 7 stops the build unless the source
  hash is passed in, and exports it (`t21_pack_source_sha256`). Engine equivalent: the simulation binary
  exports the hash of its sources and every run receipt quotes it.
- **Resumable.** Per-item results are written as they finish and reused only when the run key (manifest
  hash, reference hash, options) matches.

A run receipt for the engine would hold: seed, thread count, binary source hash, tick range, per-snapshot
state hash from the engine and from the independent checker, conservation totals, skipped checks, verdict.

## 7. Not usable, and why

- **All Python** (codec drivers, packagers, loader, server, verifier, tests): interpreted, and most of it is
  LLM serving. Excluded by laws 1 and 3. Only the ideas in sections 5 and 6 carry over.
- **All CUDA and Metal kernels** (`bi_gemm.cuh`, `tbe_mma_kernel.cu`, `context_*_gemm.cu`, `fastdec_kernels.cu`,
  `metal_steel_0304/*.h`; `.cu`, `.cuh` and `.h` files total 16,914 lines): BF16 float GEMM over packed LLM weights, tuned for one
  server GPU; cannot be built here (no nvcc) and has no use in an integer simulation.
- **K2 as is:** the bit split is BF16-specific, contexts are limited to 64 row and 16 column classes of a
  2-D matrix, and row/column class fitting lives in Python.
- **K3 as is:** at most 1.6x, expands high-entropy fields, at most 2 codebooks, 16-bit words only, row
  length must be a multiple of 128, codebook selection lives in Python. Its fixed-rate random access is
  the only attraction; no engine use for that has been identified.
- **Alias/exponent/tiled packet decoders** (`context_alias_ans_native.cpp`, `context_exponent_native.cpp`):
  GPU-oriented table layouts for the same BF16 symbols; read only in part, not built, no advantage on CPU
  snapshots over K1.
- **Speed and ratio claims in the README** (1.53x storage, tok/s figures) concern LLM weights on a server
  GPU and say nothing about integer snapshot data; none were reproduced here.
- The heap allocation, exceptions and 128 MB tables make every kernel unsuitable for cache-resident loops
  even after porting. This is acceptable only because the proposed use is off-tick tooling.

## 8. Verdict

**adapt** — port the generic context rANS (K1, about 120 lines) to C23 as snapshot tooling and adopt the
frame and receipt conventions. Everything else in the repo is reference only or excluded by law.
