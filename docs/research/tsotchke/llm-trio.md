# Tsotchke audit: llm-trio (llm-arbitrator, OBLITERATUS, bonsai-fidelity-study)

Audited 2026-11-17. Read-only on `T:`; scratch in `S:\tsotchke\llm-trio\`.
Bottom line: all three are LLM tooling or LLM studies. None of their code may enter
the simulation (law 3, and law 1: every source file is TypeScript/JavaScript or Python).
Two generic statistics ideas are worth keeping as **tooling-only** instruments; both were
re-written clean-room in C here and run.

## Repo facts

| Repo | HEAD (`git log -1 --format=%h_%cs`) | Licence (LICENSE file) | Main language | Size (no .git) | Verdict |
|---|---|---|---|---|---|
| llm-arbitrator | `5672fbd_2025-04-24` | MIT (c) 2025 tsotchke | TypeScript / JS (15 ts, 17 js) | 2.5 MB | excluded_by_law |
| OBLITERATUS | `a5a1ffa_2026-06-17` | GNU AGPL v3 (pyproject: AGPL-3.0-or-later) | Python (133 py), PyTorch | 9.3 MB | excluded_by_law |
| bonsai-fidelity-study | `09f59af_2026-09-28` | Apache-2.0 (+ NOTICE) | Markdown report + 3 py + 2 csv | 1.7 MB | excluded_by_law |

- **llm-arbitrator**: an MCP server that routes prompts between LM Studio / Ollama
  providers (`src/providers`, `src/enhancers`, `src/mcp-adapter.ts`). No numeric kernel at all.
- **OBLITERATUS**: refusal-direction removal ("abliteration") toolkit for transformer
  LLMs. `obliteratus/analysis/` holds 27 PyTorch analysis modules (10,649 lines):
  LEACE-style/Fisher direction (`leace.py`), whitened SVD, Wasserstein-optimal direction,
  spectral certification, sparse autoencoder training (`sae_abliteration.py`), probes, lenses.
- **bonsai-fidelity-study**: a written evaluation of third-party low-bit LLMs against
  their parents. Code is `scripts/recompute_ci.py` (57 lines, paired bootstrap) and
  `scripts/make_figures.py`; data is `paired_outcomes.csv` (5,403 rows of hashed item id +
  two correctness bits) and `sae_layers.csv` (63 layers of rare-feature survival + SE).

## Candidates (ideas only; nothing is copied from the repos)

| # | Idea | Source file : lines | Source signature | Deps | Pure / stateful | Float / int | Thread safety | Engine category |
|---|---|---|---|---|---|---|---|---|
| 1 | Paired bootstrap CI on per-item outcome differences | `bonsai-fidelity-study/scripts/recompute_ci.py` : 16-46 | `compute(path: Path, draws: int = 10_000) -> dict` | Python stdlib `random` (Mersenne Twister) | pure given seed | integer sums, float only for the final ratio | source: single-threaded; C port: counter RNG, any thread order | (l) tooling: evidence for lesion A/B runs |
| 2 | Fisher discriminant direction `v = Sw^-1 (mu1 - mu0)` + rank-1 projection lesion | `OBLITERATUS/obliteratus/analysis/leace.py` : 63-186 (`LEACEExtractor.extract`); the file itself says it is FLD, not true LEACE | `extract(harmful_acts, harmless_acts, layer_idx) -> LEACEResult` | torch | pure | float | n/a (torch) | (l) tooling: find and lesion a linear feature in creature state |
| 3 | "Rare-feature survival" per layer with SE, gate = minimum over layers | `bonsai-fidelity-study/METHODS.md` : 39; `data/sae_layers.csv` | none public (the scoring script is not in the repo) | n/a | n/a | float | n/a | reference only: gate-on-worst-layer pattern |
| 4 | Sparse autoencoder (tied weights, expansion 4) + feature statistics | `OBLITERATUS/obliteratus/analysis/sae_abliteration.py` : 59-262 (`SparseAutoencoder`, `train_sae`) | `train_sae(activations, hidden_dim, expansion=4, ...)` | torch, autograd, Adam | stateful training | float | n/a | none (see NOT usable) |
| 5 | Whitened SVD / Wasserstein-optimal / spectral certificate of a direction | `whitened_svd.py` : 48-180, `wasserstein_optimal.py` : 96-197, `spectral_certification.py` : 119-310 | `extract(...)`, `certify(...)` | torch `linalg.eigh`/`svd` | pure | float | n/a | none beyond #2 |

AGPL note: candidate 2 was written from the textbook formula (Fisher 1936), not from the
AGPL source; no OBLITERATUS text or code was copied to scratch or to this project.
Candidate 1 is Apache-2.0 and trivially re-derived; the C version shares no code with it.

## What was run (2026-11-17, this session)

Files: `S:\tsotchke\llm-trio\paired_boot.c`, `S:\tsotchke\llm-trio\fld_lesion.c`;
copied inputs in `S:\tsotchke\llm-trio\bonsai\` (`paired_outcomes.csv`, `recompute_ci.py`).

### 1. Paired bootstrap, integer, counter-RNG (C) versus the repo's Python

```
cd /s/tsotchke/llm-trio
gcc -std=c2x -O2 -Wall -Wextra -o paired_boot.exe paired_boot.c && ./paired_boot.exe bonsai/paired_outcomes.csv
```
One `-Wformat-truncation` warning (snprintf key buffer), otherwise clean. Output:
```
off/gsm8k/flexible           n=1319 delta=-0.0212 ci95=[-0.0303,-0.0121] repeat_equal=1
off/gsm8k/strict             n=1319 delta=-0.0205 ci95=[-0.0296,-0.0121] repeat_equal=1
off/ifeval/prompt_strict     n= 541 delta=-0.0333 ci95=[-0.0665,+0.0000] repeat_equal=1
on/gsm8k/flexible            n= 300 delta=-0.0033 ci95=[-0.0200,+0.0133] repeat_equal=1
on/gsm8k/strict              n= 300 delta=+0.0733 ci95=[+0.0200,+0.1267] repeat_equal=1
on/humaneval/plus            n= 164 delta=+0.0183 ci95=[+0.0000,+0.0427] repeat_equal=1
on/ifeval/prompt_loose       n= 541 delta=-0.0037 ci95=[-0.0222,+0.0129] repeat_equal=1
on/ifeval/prompt_strict      n= 541 delta=+0.0000 ci95=[-0.0185,+0.0166] repeat_equal=1
on/mbpp/plus                 n= 378 delta=-0.0212 ci95=[-0.0503,+0.0106] repeat_equal=1
```
Reference, the repo's own script on the same copy
(`python -I bonsai/recompute_ci.py bonsai/paired_outcomes.csv`, reformatted):
```
off/gsm8k/flexible 1319 -0.0212 [-0.0303, -0.0121]
off/gsm8k/strict 1319 -0.0205 [-0.0296, -0.0114]
off/ifeval/prompt_strict 541 -0.0333 [-0.0665, 0.0]
on/gsm8k/flexible 300 -0.0033 [-0.02, 0.0133]
on/gsm8k/strict 300 0.0733 [0.0233, 0.1233]
on/humaneval/plus 164 0.0183 [0.0, 0.0427]
on/ifeval/prompt_loose 541 -0.0037 [-0.0222, 0.0148]
on/ifeval/prompt_strict 541 0.0 [-0.0185, 0.0185]
on/mbpp/plus 378 -0.0212 [-0.0529, 0.0106]
```
Reading: n and delta agree exactly on all nine groups. Interval end points agree exactly
on 4 groups and differ by at most 0.0034 on the rest; that is expected because the RNG
streams differ (splitmix64 counter versus Mersenne Twister) and the C run used one seed
for every group. It is not a bit-exact port and is not claimed to be.
`repeat_equal=1` = same seed gave the same interval twice in-process.

### 2. Fisher direction + rank-1 lesion on synthetic 8-D data (C)

```
gcc -std=c2x -O2 -Wall -Wextra -o fld_lesion.exe fld_lesion.c -lm && ./fld_lesion.exe
```
No warnings. Output:
```
fld dir  : +0.000 +1.000 +0.001 -0.001 +0.002 -0.006 +0.008 -0.002
dmean dir: +0.710 +0.703 -0.016 +0.002 -0.004 -0.012 +0.036 -0.031
d' along fld=6.857  along diff-of-means=0.172
after rank-1 lesion: best remaining d'=0.143 (old dir d'=0.288)
```
Reading: with a planted signal on a low-variance axis and a high-variance distractor axis,
the Fisher direction recovers the planted axis (d' 6.86) where plain difference-of-means
is pulled onto the distractor (d' 0.17). Projecting that one direction out drops the best
linearly recoverable separation to 0.14. The "old dir d'=0.288" figure is a ratio of
rounding residues (the projection is ~1e-15 after the lesion) and carries no meaning.
Synthetic data only (2 x 2000 uniform-noise vectors); nothing here was tested on
creature state.

## Proposed integration (tooling only; nothing in Tier 0/1/2 or world fields)

```c
/* tools/stats/paired_boot.h -- offline evidence tool, never linked into the sim */
void an_paired_boot_ci(const int8_t *diff, uint32_t n, uint32_t draws, uint64_t seed,
                       int32_t *lo_sum, int32_t *hi_sum, int32_t *scratch /* draws */);
```
Use: lesion experiments required by law 4. Run the same seeds with a mechanism on and off,
take a per-seed (or per-colony) outcome difference, and report the delta with a paired
interval instead of two unpaired means. For non-binary outcomes widen `diff` to `int32_t`
and the sums to `int64_t`. Replace splitmix64 with the project's Philox4x32-10 keyed by
`(seed, draw, i)` so tooling and simulation share one RNG family.

```c
/* tools/stats/fld_probe.h -- offline probe over snapshot dumps */
int an_fld_direction(const float *x0, uint32_t n0, const float *x1, uint32_t n1,
                     uint32_t dim, float ridge, float *v_out /* dim, unit */);
void an_project_out(float *x, uint32_t n, uint32_t dim, const float *v);
```
Use: an indicator-only probe (law 7). Take two sets of creature state vectors from a
snapshot (for example "about to flee" versus "not"), find the direction, and report d'.
The lesion test belongs in the simulation only as an **experiment switch** that zeroes a
component of integer state through the normal lesion path; the float projection itself
stays offline because float state would break law 6.

Pattern worth copying from the study's write-up (no code): register a threshold before
the run, gate on the **worst** unit (their "minimum survival over 63 layers"), always
carry a same-run identity anchor (1.0) and a mild control (their Q8_0 at 0.982), and
publish the per-unit table with standard errors.

## NOT usable, and why

- **llm-arbitrator, all of it**: MCP server + prompt routing to LLM providers in
  TypeScript. Violates laws 1 and 3; contains no numeric method.
- **OBLITERATUS, all code**: Python/PyTorch operating on pretrained transformer weights,
  tokenizers, prompt sets and remote model hubs (laws 1 and 3). Separately, AGPL-3.0 code
  must not be copied or ported line-by-line into this project without accepting AGPL
  terms for the result; only uncopyrightable textbook math was taken.
- **Sparse autoencoder (candidate 4)**: needs float gradient training (Adam, autograd) on
  large activation corpora; as a per-creature mechanism it would be non-deterministic
  float learning with no causal role, and as an analysis tool it is far heavier than the
  FLD probe for creature state of a few dozen integers. Not recommended now.
- **Whitened SVD / Wasserstein / spectral certificate (candidate 5)**: all need dense
  eigendecompositions of covariance matrices and exist to choose a direction to delete
  from an LLM. Candidate 2 covers the one reusable case; the "certificates" are
  heuristics tied to the LLM use and were not verified here.
- **Rare-feature survival numbers (candidate 3)**: the scoring script and the SAE
  dictionaries are not in the repository, so the statistic cannot be reproduced; only the
  gating pattern is reusable.
- **bonsai data and results**: measurements about third-party LLMs; irrelevant to the
  engine and forbidden as datasets (law 3). The CSV was used once here only as a
  ready-made paired table to check the bootstrap port.
- Engine categories (a) to (k): **none** of the three repositories contains a counter
  RNG, fixed-point kernel, state-vector simulator, SO(3) code, QGT, spin network, GEMV,
  Monte Carlo sampler, PDE solver, codec or shader.

## Limits of this audit

- Targeted reading only: READMEs, tree listings, `recompute_ci.py` in full, the header
  and symbol list of the OBLITERATUS analysis modules, METHODS.md section on the SAE
  readout. The other ~100 OBLITERATUS files were not opened.
- No prior-art search was done and no novelty is claimed; both instruments are standard
  statistics (bootstrap: Efron 1979; discriminant: Fisher 1936).
