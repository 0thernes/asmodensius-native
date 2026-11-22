# Tsotchke audit: gpt2-basic

Audited 2026-11-17. Everything marked "ran" was run in this session; commands and
output tails are below. Timings are **noisy, parallel load** (other agents on the machine).

## Repo facts

| Item | Value |
|---|---|
| Path | `T:\mirrors\gpt2-basic` (read-only; nothing built or changed there) |
| HEAD | `f1d51a4_2026-05-21` (`git -C T:/mirrors/gpt2-basic log -1 --format=%h_%cs`) |
| License | MIT License, "Copyright (c) 2025 tsotchke" (from `LICENSE`) |
| Main language | FreeBASIC: 21 tracked `.bas` files, 20 in `src/` with 21,413 lines. 75 Python scripts (training, export, evaluation tooling). **No C, no assembler files.** Inline 32-bit x86 `ASM` blocks exist inside `src/asm_optimizations.bas`. |
| Size | 220 MB working tree + 101 MB `.git` (`du -sh`). 1,282 tracked files; 683 are exported model files under `assets/gpt2_basic/`, 404 are logs and reports under `qemu/evidence/`. |
| What it is | A GPT-2-style transformer text generator for 386/486 DOS PCs with a byte/BPE tokenizer, pretrained weight files, and a "DOS assistant" front end. |
| Built here | The repo itself: **no**. `fbc` (FreeBASIC compiler) is not on this machine and nothing may be installed. Selected numeric kernels were ported by hand to C23 and those ports were built and run. |

## Verdict

**excluded_by_law** for the repo as a whole (law 3: it is an LLM with a tokenizer and
pretrained weights; and none of it is C or NASM). Three small non-LLM numeric kernels
are worth keeping **as C ports** (about 300 lines of C in `S:\tsotchke\gpt2-basic\`);
the rest is reference only or defective.

## Candidate kernels

Engine-use letters refer to the task list: (b) deterministic fixed-point math,
(g) tensor/matrix kernels, (h) samplers, (j) codecs.

| # | Kernel | Source file and lines | Signatures in the source | Depends on | Pure / stateful | Float / int | Thread safety (source) | Engine use |
|---|---|---|---|---|---|---|---|---|
| 1 | Q20.12 scalar math: saturating mul/div, Newton sqrt, table `exp(-x)`, tanh, GELU | `src/real_gpt.bas` 1861-1942, 2201-2243 | `TinyGPTFixedClamp(value AS LONGINT) AS LONG`, `TinyGPTFixedMul(a, b)`, `TinyGPTFixedDiv(a, b)`, `TinyGPTFixedSqrt(value)`, `TinyGPTFixedExpNeg(x)`, `TinyGPTFixedTanh(x)`, `TinyGPTFixedGELU(x)` (all `LONG` in, `LONG` out) | 513-entry `exp(-i/32)` table (`GPT2EXP.BIN`, built by `scripts/convert_tiny_gpt_fixed.py` 46-51 with Python `math.exp`) | pure, apart from reading the global table | integer only (int32 values, int64 intermediates) | safe: read-only global table | (b) |
| 2 | Integer Boltzmann top-k / top-p draw | `src/real_gpt.bas` 2766-2842 (inside `TinyGPTFixedSample`, 2715-2843) | `TinyGPTFixedSample(logits() AS LONG, context() AS INTEGER, context_len, temperature AS SINGLE, top_p AS SINGLE, top_k AS INTEGER) AS INTEGER` | kernel 1; FreeBASIC `RND`; token rules (2742-2764, LLM-specific) | stateful in the source (global `RND` stream, `REDIM` heap arrays, writes into `logits()`) | integer weights, but `temperature`, `top_p` and `RND` are floats | not safe in the source (global RNG, globals) | (h) and (b) |
| 3 | Q20.12 GEMV and layer norm | `src/real_gpt.bas` 1997-2050 | `TinyGPTFixedLayerNormVec(input_vec() AS LONG, output_vec() AS LONG, emb_dim, gamma_arr() AS LONG, beta_arr() AS LONG, param_base AS LONG)`, `TinyGPTFixedLinearVec(input_vec(), output_vec(), in_dim, out_dim, weight_arr(), weight_base, bias_arr(), bias_base)` | kernel 1; global scratch `g_tiny_fx_linear_acc` | stateful (global accumulator) | integer only | not safe in the source (shared scratch) | (g) |
| 4 | "q4-log" 4-bit packed weights + decode-LUT GEMV | quantizer: `scripts/quantize_gpt2_basic.py` 31-71; decode table: `src/real_gpt.bas` 519-533; GEMV: `src/real_gpt.bas` 2131-2165 (`TinyGPTFixedHeadQ4LinearVec`, 2052-2166) | `nearest_log_q4_code(value: int, scale: int) -> tuple[int, int]`, `pack_nibbles(codes) -> bytes`, `TinyGPTFixedHeadQ4LinearVec(input_vec() AS LONG, output_vec() AS LONG)` | kernel 1; many globals; the first half (2073-2129) reads rows from disk with `SEEK`/`GET` | stateful (globals, file I/O variant) | integer only | not safe in the source | (g); as a weight representation, not as an exact codec |
| 5 | Shortlist GEMV (compute only listed output rows) | `src/real_gpt.bas` 2168-2199 | `TinyGPTFixedHeadShortlistLinearVec(input_vec() AS LONG, output_vec() AS LONG)` | globals | stateful | integer only | not safe in the source | idea only: skip outputs that cannot be chosen |

Format of kernel 4: one weight is 1 sign bit + a 3-bit level index; value =
sign x scale[output] x level / 4096 with levels (0, 256, 512, 1024, 1536, 2048, 3072, 4096),
i.e. {0, 1/16, 1/8, 1/4, 3/8, 1/2, 3/4, 1}; one int32 scale per output (the largest
absolute weight of that output); two weights per byte.

## What was built and run

All files are in `S:\tsotchke\gpt2-basic\`: `fx12.h` (kernels 1, 2, 3 plus three small
additions of ours, marked in the header), `q4.h` (kernel 4), `test_fx12.c`,
`test_q4_sampler.c`, `test_swar.c`, and `ref\GPT2EXP.BIN` (a copy of the repo's 2,052-byte
exp table, used only to compare; it is a table of `exp`, not model weights).

Additions of ours in `fx12.h`, not present in the repo: `fx12_exp_table_int` (integer-only
table generator: Taylor series for exp(-1/32) in Q62 with `__int128`), `fx12_exp_neg_lerp`
(linear interpolation on the 7 index bits the repo discards), `fx12_sqrt_floor`
(exact floor square root), `fx12_sigmoid`.

### Test 1: Q20.12 kernel (candidates 1 and 3) — ran

```
cd /s/tsotchke/gpt2-basic
gcc -std=c2x -O2 -Wall -Wextra -o test_fx12.exe test_fx12.c -lm     (no warnings)
./test_fx12.exe ref/GPT2EXP.BIN
```

```
exp table: int-generated vs repo GPT2EXP.BIN: compared, mismatches=0 of 513
exp table: int-generated vs libm round(): mismatches=0 of 513
exp table: tab[0]=4096 tab[32]=1507 tab[512]=0 fnv=bfd70c449bc7e5bf
sqrt: 2000000 inputs; repo Newton == floor+1 in 130 cases, other deviation 0; fx12_sqrt_floor wrong 0
max abs error over x in [-8,8] (131073 points):
  exp_neg 0.030530  tanh 0.015615  sigmoid 0.007991  gelu(vs exact erf form) 0.007352
  tanh monotonicity violations 0, sigmoid 0, tanh oddness violations 0
linear 32x16: max abs error vs double 0.003298 (bound 32 truncations * 2^-12 = 0.007812)
layernorm n=32: max abs error vs double 0.000916
linear+layernorm state hash 7432c1490354943d
timing (noisy, parallel load): fx12_tanh 2.62 ns/call over 1638400 calls (sink -819200)
timing (noisy, parallel load): fx12_linear 32x16 233.7 ns/call (0.46 ns per weight)
RESULT: PASS
exit=0
```

Rebuilt with `gcc -std=c2x -O3 -march=native -Wall -Wextra -o t1_o3.exe test_fx12.c -lm`:
same table hash `bfd70c449bc7e5bf`, same state hash `7432c1490354943d`, `RESULT: PASS`;
`fx12_linear 32x16 81.7 ns/call` (noisy, parallel load).

What this shows:
- The exp table can be produced with integer arithmetic only and matches the repo's
  shipped table in all 513 entries, so the engine does not need libm for it.
- The repo's Newton square root is not exactly floor: it returned floor+1 for 130 of
  2,000,000 inputs (it stops on "x unchanged", which can stop on the upper value of a
  two-value cycle). Never worse than +1. `fx12_sqrt_floor` was exact on all inputs.
- The table lookup truncates the index, so `exp_neg` is off by up to 0.0305 (3 % of
  full scale) and tanh by up to 0.0156. Monotone and odd, so usable, but coarse.

### Test 2: q4-log GEMV and integer Boltzmann draw (candidates 4 and 2) — ran

```
gcc -std=c2x -O2 -Wall -Wextra -o test_q4_sampler.exe test_q4_sampler.c -lm     (no warnings)
./test_q4_sampler.exe
```

```
q4 32x16: packed GEMV vs dense GEMV on dequantized weights: 0 mismatches of 80000 outputs
q4 32x16: storage 320 B (codes 256 + scales 64) vs int32 2048 B vs int8 512 B; decode LUT 1024 B
q4 32x16: weight rel RMS error 0.1025; output rel RMS error 0.0973, max abs 0.2095; hash 46b4b806248b668f
timing (noisy, parallel load): q4_linear 32x16 437.4 ns/call, fx12_linear 231.5 ns/call (sink 1122591426)
q4 33x15: packed GEMV vs dense GEMV on dequantized weights: 0 mismatches of 75000 outputs
q4 33x15: storage 324 B (codes 264 + scales 60) vs int32 1980 B vs int8 495 B; decode LUT 960 B
q4 33x15: weight rel RMS error 0.1008; output rel RMS error 0.0947, max abs 0.2078; hash c17c59a5d591ef8a
sampler: 2^22 stratified draws, worst |count - weight share| = 0.705 draws
sampler: T=0.25 share of best action 0.8322 (true softmax 0.8328)
sampler: T=1.00 share of best action 0.3484 (true softmax 0.3501)
sampler: T=4.00 share of best action 0.1782 (true softmax 0.1786)
sampler: max abs probability error vs true softmax, truncated (as in repo) table: 0.00400
sampler: max abs probability error vs true softmax, interpolated table: 0.00013
exp_neg_lerp: max abs error over [-17,0] = 0.000426 (truncated lookup: 0.030530)
sampler: top-k=3 draws outside the 3 best: 0 of 1000000; counts 484962 293071 221967
sampler: top-p=0.5 counts 622560 377440 0 0 (rest must be 0: 0)
sampler: 1e6-draw decision hash 0e88e34c0db2233b
timing (noisy, parallel load): fx12_sample_topk n=8 k=8 28.2 ns/draw (sink 3221707)
RESULT: PASS
exit=0
```

Rebuilt with `gcc -std=c2x -O3 -march=native ... -o t2_o3.exe test_q4_sampler.c -lm`: the
three hashes are identical (`46b4b806248b668f`, `c17c59a5d591ef8a`, `0e88e34c0db2233b`),
`RESULT: PASS`; `q4_linear 217.9 ns/call` against `fx12_linear 89.4 ns/call`,
`fx12_sample_topk 13.8 ns/draw` (noisy, parallel load).

What this shows:
- The packed 4-bit GEMV gives bit-identical outputs to a dense GEMV over the dequantized
  weights. Storage is 6.4 times smaller than int32 and 1.6 times smaller than int8.
- The cost is accuracy and time: weights are off by about 10 % RMS (random
  bell-shaped weights), and the scalar packed GEMV was about twice as slow as the dense
  int32 one in both builds. It saves memory, not time.
- The draw is exact with respect to its integer weights (within one draw in 4,194,304),
  top-k and top-p cut correctly, and the same 32 random bits always give the same choice.
  With the interpolated table the probabilities are within 0.00013 of a true softmax;
  with the repo's truncated lookup within 0.004.
- Temperature has a measurable effect on behaviour: share of the best action 0.83 at
  T=0.25, 0.35 at T=1, 0.18 at T=4.

### Test 3: the repo's "SIMD-like" packed-word arithmetic — ran, and it is defective

```
gcc -std=c2x -O2 -Wall -Wextra -o test_swar.exe test_swar.c     (no warnings)
./test_swar.exe
```

```
repo SIMD_Add_8bit      wrong in 984570 of 1000000 random word pairs
repo SIMD_Subtract_8bit wrong in 875605 of 1000000 random word pairs
repo DotProduct_8bit    wrong in 993268 of 1000000 random 8-element vectors
example: add8(0x00000100, 0) repo=00000000 reference=00000100
textbook swar_add8x8 wrong in 0, swar_sub8x8 wrong in 0 of 1000000
RESULT: PASS (textbook forms); repo forms are defective
exit=0
```

These are C ports of `src/simd_ops.bas` 224-246 and 310-340, compared with four
independent wrapping 8-bit lanes. The add replaces the lowest bit of bytes 1 to 3 by
`a AND b`; the subtract clears those bits; the dot product keeps only the low 8 bits of
each product in its 4-wide path but full products in its tail loop. The textbook
carry-isolating forms (Hacker's Delight, section 2-18) are in `test_swar.c` and passed.
This machine has AVX2, so word-packed lanes are not needed in any case.

## Proposed integration

Names are proposals. All are integer-only and take no global mutable state; the exp
table is a `static const int32_t[513]` (2,052 bytes, 33 cache lines) emitted by a build
tool that runs `fx12_exp_table_int`, or filled once at start-up.

| Proposed C signature | From | Where it runs | Causal role |
|---|---|---|---|
| `int32_t an_fx12_mul(int32_t a, int32_t b);` `int32_t an_fx12_div(int32_t a, int32_t b);` `int32_t an_fx12_sqrt_floor(int32_t v);` | kernel 1 (sqrt replaced by the exact version) | Tier 0, inlined | arithmetic under every fixed-point update |
| `int32_t an_fx12_exp_neg(int32_t x_q12);` (interpolated) `int32_t an_fx12_sigmoid(int32_t x_q12);` `int32_t an_fx12_tanh(int32_t x_q12);` | kernel 1 + our interpolation | Tier 1 (every 32 to 64 ticks) as written, because tanh and sigmoid each cost one 64-bit divide; for Tier 0 use a direct tanh table built from these at start-up | neuron activation; decay factors `exp(-dt/tau)` for traces, hunger, pheromone evaporation |
| `int an_action_sample(const int32_t *score_q12, int n, int k, int32_t temp_q12, int32_t top_p_q12, uint32_t u);` | kernel 2, token rules removed | Tier 0 or Tier 1, once per creature decision; `u` = one Philox4x32-10 word keyed by (seed, creature id, tick) | action selection. Lesion test: set `temp_q12` to 1 (pure argmax) or raise it, and behaviour statistics change as in Test 2 |
| `void an_fx12_linear(const int32_t *in, int32_t *out, int in_dim, int out_dim, const int32_t *w, const int32_t *bias, int64_t *acc);` `void an_fx12_layernorm(const int32_t *in, int32_t *out, int n, const int32_t *gamma, const int32_t *beta);` | kernel 3 | Tier 1 reference implementation; Tier 0 needs the planned int8/AVX-VNNI GEMV instead | brain layer evaluation; layer norm as a homeostatic gain control |
| `void an_q4_quantize(const int32_t *w, int in_dim, int out_dim, uint8_t *codes, int32_t *scale, int32_t *deq);` `void an_q4_linear(const int32_t *in, int32_t *out, int in_dim, int out_dim, const uint8_t *codes, const int32_t *decode, const int32_t *bias, int64_t *acc);` | kernel 4 | Tier 2 / development time: expand genome-encoded innate weights into the working int8 or int16 weights when a creature is born. Not in the tick loop as written | compact heritable weight format (4 bits per synapse plus one scale per neuron) |

Notes on the last row. [PROPOSAL, not run] If the 4-bit form is ever wanted in a hot
loop, accumulate `sum_i x_i * level(code_ij)` with a 16-entry `pshufb` level table and
multiply by `scale_j` once per output; that changes the rounding from the repo's
truncate-per-product, so it would need its own reference test. The format is lossy
(10 % RMS here), so it must not be used for snapshots or receipts, which have to be exact.

## NOT usable, and why

| Item | Where | Why not |
|---|---|---|
| Transformer forward pass, attention, KV decode cache, embeddings | `src/real_gpt.bas` (most of it), `src/transformer_components.bas`, `src/model.bas`, `src/main*.bas` | LLM. Law 3. |
| Tokenizer and vocabulary files | `src/tokenizer.bas`, `scripts/gpt2_basic_tokenizer.py`, `VOCAB.BIN` | Tokenizer. Law 3. |
| Model files | `assets/gpt2_basic/**` (683 files) | Pretrained weights. Law 3. Only the 513-entry exp table was read, to compare. |
| N-gram "quality prior", assistant, knowledge packs | `src/quality_prior.bas`, `src/assistant.bas`, `data/` | Text generation and datasets. Law 3. |
| Training, export and evaluation scripts | `scripts/*.py` (75 files) | Python and PyTorch tooling for the LLM. Laws 1 and 3. Not run. |
| Packed-word "SIMD" add, subtract, dot product | `src/simd_ops.bas` 224-246, 310-340 | Wrong results (Test 3). The 4-bit and 16-bit variants (344-447, 570-646) use the same masking idea; they were read, not run. |
| Q16.16 fixed-point module | `src/asm_optimizations.bas` 48-165 | Division falls back to floats (119-124); the 64-bit square root is `SQR` on a float (133); `FloatToFixed` rounds through floats. Not deterministic integer math. The inline `ASM` blocks (189-388) are 32-bit x87/DOS code. |
| Table "fixed-point" exp and softmax | `src/softmax_fixed.bas` 64-132, 196-232 | `FixedExp` converts to float to find its index; `FixedExpFast` calls `EXP`. `FixedExpInteger` (135-188) is integer-only but uses a 3-term polynomial whose coefficients sum to 1.989 instead of 2; read, not run. Kernel 1's table is the better form. |
| 4+4-bit log quantizer | `src/quantization.bas` 16-286 | Needs a 65,536-entry lookup table (line 30) built through float conversions; too large for an L1-resident loop. Kernel 4 covers the same need in 64 bytes per output. Read, not run. |
| Block-sparse matrices | `src/block_sparse.bas` 23-70, 282-348, 519-527 | Float data in a heap-allocated linked list of blocks with a temporary matrix allocated per multiply: pointer chasing and allocation in the loop (law 2), and it exists only to mask attention. Idea only. |
| Float matrix library | `src/matrix_ops.bas`, `src/data_structures.bas` | Plain float triple loops, Gauss-Jordan inverse, float GELU and layer norm. Nothing the engine lacks; not deterministic across compilers. |
| Seed-to-weight helpers | `src/dos_gpt2_basic.bas` 42-55 | `(seed*37 + (seed\7)*11 + 13) MOD 17`: a toy pattern, not a mixer. No use for (a). |
| Block-position and n-gram hashes | `src/block_sparse.bas` 519-527, `src/quality_prior.bas` 357-368 | Small modular hashes for tiny tables. No use for (a). |
| Memory manager, file I/O, CPU detection, benchmark, visual trace | `src/memory_manager.bas`, `src/file_io.bas`, `src/simd_ops.bas` 56-160, `src/benchmark.bas`, `src/visual_trace.bas` | DOS-specific plumbing. |

Nothing in the repo maps to (a) RNG or mixers, (c) quantum circuits, (d) SO(3)
representations, (e) quantum geometric tensor, (f) spin or Hopfield networks,
(i) PDE solvers, (k) rendering.

## Caveats

- The FreeBASIC originals were not compiled or run (no `fbc`). Faithfulness of the C
  ports rests on reading the source, on the exp table matching the shipped file in all
  513 entries, and on FreeBASIC `\` and C `/` both truncating toward zero. The repo's
  own test vectors (`GPT2VEC.TXT`) exercise the whole transformer and were not used.
- One observation by reading only, not run: the in-memory q4 path at
  `src/real_gpt.bas` 2133 starts each input row at byte `(in_idx * vocab_size) \ 2`,
  which is only right when `vocab_size` is even. The C port pads each row to whole bytes.
- `fx12_linear` and `q4_linear` omit the per-product clamp to +/-2,000,000,000 that the
  source applies in the q4 path (2142-2143); it cannot trigger for Q12 values below
  about 2^21 in magnitude and was not reached in the tests.
- The splitmix64 generator in the tests only produces test inputs. The engine's
  generator stays Philox4x32-10.
- No novelty is claimed. Fixed-point table exponentials, top-k softmax sampling and
  4-bit sign-plus-level weight codes are all standard techniques; no prior-art search
  was done or needed for this audit.
