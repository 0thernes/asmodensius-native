# Tsotchke audit: peripheral repos (key: peripheral)

Date 2026-11-17. Five small repos triaged. Nothing here is a numeric kernel
library; the only reusable material is two ideas from `logo-lab`, which I
re-implemented as original integer C in `S:\tsotchke\peripheral\` and ran.

## Repo facts

HEAD from `git -C <path> log -1 --format=%h_%cs`; size is `du -sh --exclude=.git`.

| Repo | HEAD | Licence (from the repo) | Language | Size / tracked files | What it is |
|---|---|---|---|---|---|
| `T:\mirrors\homebrew-eshkol` | `7cc5d13_2026-10-19` | No LICENSE file. The formula declares the packaged Eshkol as `license "MIT"`. | Ruby (one formula, 12.9 KB) | 64K / 2 | Homebrew tap: builds Eshkol v1.3.6-evolve with LLVM 21, pcre2, readline, staged sqlite/curl resources. |
| `T:\mirrors\homebrew-moonlab` | `9650642_2026-09-11` | No LICENSE file. The formula declares Moonlab as `license "MIT"`. | Ruby (one formula, 4.2 KB) | 65K / 2 | Homebrew tap: builds Moonlab v1.2.1 with cmake/ninja, libomp, openssl, openblas on Linux. |
| `T:\mirrors\.github` | `ea3554d_2026-07-14` | No LICENSE file. | Markdown (one README, 13.8 KB) | 64K / 1 | Organisation profile page; a portfolio index pointing at the other repos. |
| `T:\mirrors\logo-lab` | `19ff17f_2026-06-03` | No LICENSE file at the top level (only third-party ones inside the committed `node_modules`). Treat as all rights reserved. | JavaScript / JSX (React 19, three 0.184, Vite 8) | 164M / 1816 (almost all is committed `node_modules`, `dist`, `test-results`) | Browser lab for a Mobius-strip logo: three.js `MeshPhysicalMaterial` controls, a generated starfield, and an ASCII overlay. |
| `T:\mirrors\SolanaQuantumFlux` | `7c951ef_2025-06-09` | "TSOTCHKE CORPORATION PROPRIETARY LICENSE" (LICENSE file): no redistribution, no modification, no derivative works. | TypeScript SDK (291 lines) plus docs | 30M / 49 | Client SDK and docs for a paid on-chain random-number service on Solana, with sample dumps and statistical plots. |

## logo-lab: what is actually in it

- There is **no custom GLSL** in the repo. `grep` for `gl_FragColor|ShaderMaterial|onBeforeCompile|fragmentShader` over `src/` and
  `mobius_material_lab_baseline.jsx` returns nothing. All shading is stock three.js: `MeshPhysicalMaterial`
  (transmission, thickness, clearcoat, sheen, iridescence) and stock post passes (`RGBShiftShader`, `SMAAPass`).
- Own code that carries an idea:
  - `mobius_material_lab_baseline.jsx:90-129` `getIridescenceThicknessTexture()`: a 64x64 grey map,
    `value = 0.65*sweep(x) + 0.35*(0.5+0.5*sin(4*pi*x/64 + 2*pi*y/64))`, used as the thin-film thickness map;
    `:131-135` `getIridescenceThicknessRange(profile)` clamps the min/max nanometre range (defaults 100 to 400 nm, IOR 1.3).
  - `mobius_material_lab_baseline.jsx:162-216` `getTwistStep`, `getLogicalFaceCount`, `getLogicalFaceIndex`:
    the number of distinct faces of an n-rail strip with a twist is `gcd(railCount, twistStep)`.
  - `mobius_material_lab_baseline.jsx:630-780` `createStarfieldTexture(seed)`: seeded by a 32-bit LCG
    (`s = 1664525*s + 1013904223`), float output. Sequential and stateful.
  - `src/asciiRenderer.js:176-246, 286-426`: per-cell peak luminance, central-difference gradient,
    `classifyEdgeDirection(theta, hasEdge)` (4 edge glyphs from `atan2`), `bucketAsciiLuminance(l, exposure, attenuation, invert)`
    (10 fill glyphs), 98th-percentile luminance normalisation, row-depth fog.

## Candidates

All candidates are ideas from logo-lab re-written as original C (no source copied; the repo has no licence and is JavaScript).

| # | Idea | Source file and lines | Source signature | Deps | Pure / stateful | Float / integer | Thread safety | Engine map |
|---|---|---|---|---|---|---|---|---|
| 1 | Thin-film skin signal: gene-set thickness map, then per-wavelength interference colour | `mobius_material_lab_baseline.jsx:90-135` | `getIridescenceThicknessTexture()`, `getIridescenceThicknessRange(profile)` | source: three.js + DOM canvas; my C: none | source caches a global texture; my C is pure | source float; my C integer only | my C: re-entrant, no globals | (k) skin shader idea; causal only if the colour is fed to perception as a visible signal |
| 2 | ASCII field viewer: gradient direction glyphs plus luminance buckets | `src/asciiRenderer.js:176-246, 360-398` | `classifyEdgeDirection(theta, hasEdge)`, `bucketAsciiLuminance(luminance, exposure, attenuation, invert)`, `glyphForTile(direction, bucket, drawEdges, drawFill)` | source: DOM canvas; my C: libc `abs`, stdio | pure | source float + `atan2`/`pow`; my C integer | re-entrant | (l) tooling only: headless dumps of world fields in logs and CI |
| 3 | Face count of a twisted n-rail strip = gcd(rails, twistStep) | `mobius_material_lab_baseline.jsx:137-216` | `gcd(a,b)`, `getLogicalFaceCount(railCount, twistDegrees)` | none | pure | integer after rounding | trivially safe | (l) marginal: body-plan topology count for ribbon-like morphologies; not tested |
| 4 | Seeded starfield | `mobius_material_lab_baseline.jsx:630-780` | `createStarfieldTexture(seed = 1)` | DOM canvas | stateful LCG closure | float | not safe (sequential stream) | none: an LCG breaks law 6; Philox already covers this |

## Build and run (ran now, in `S:\tsotchke\peripheral\`)

### Candidate 1: `skin_film.c`

```
cd /s/tsotchke/peripheral && gcc -std=c2x -O2 -Wall -Wextra skin_film.c -o skin_film.exe && ./skin_film.exe && ./skin_film.exe | head -2
```

Output (no warnings; second run repeats the same hashes):

```
gene0 hash=1a29176c8a1c278e meanRGB=146,127,129 d(0,0)=151 d(63,63)=329
gene1 hash=cb6e2166d07922dd meanRGB=127,127,123 d(0,0)=255 d(63,63)=500
sin_exact_points=ok 2000 patches (8.19M texels) in 60.0 ms acc=297550
gene0 hash=1a29176c8a1c278e meanRGB=146,127,129 d(0,0)=151 d(63,63)=329
gene1 hash=cb6e2166d07922dd meanRGB=127,127,123 d(0,0)=255 d(63,63)=500
```

Timing is noisy, parallel load: about 7 ns per texel (three channels), scalar, one thread.
What this shows: the map is a pure integer function of five gene fields, two genes give different maps, repeat runs give the same hash.
What it does not show: physical accuracy. The sine is a parabola (`4x(1-|x|)`, exact at 0 and the quarter turns, up to about 5.6 % off
elsewhere) and the colour is a two-beam model at three fixed wavelengths, not the Belcour-Barla model three.js uses.

### Candidate 2: `ascii_field.c`

```
cd /s/tsotchke/peripheral && gcc -std=c2x -O2 -Wall -Wextra ascii_field.c -o ascii_field.exe && ./ascii_field.exe; echo rc=$?
```

Output tail (no warnings):

```
            |\?????????????????????/|::ccccccccc
             \\-?????????????????-//:::ccccccccc
             .-\\-?????????????-//-::::ccccccccc
             ...-\\-----------//-::::::ccccccccc
             .....-------------::::::::ccccccccc
             .............:::::::::::::ccccccccc
edges | 16  - 64  / 14  \ 14 ; dir(10,0)=0 dir(0,10)=1 dir(7,7)=2 dir(7,-7)=3 dir(1,1)=-1
rc=0
```

The `atan2` bands of the source (0.05 pi and 0.45 pi) became integer slope tests (`ay*512 < ax*81`, `ay*512 > ax*3233`).

### Reference check of the source idea (repo's own language, copied to scratch first)

```
cd /s/tsotchke/peripheral/jsref && node --test tests/ascii-renderer.test.mjs 2>&1 | tail -9
```

```
ℹ tests 6
ℹ pass 6
ℹ fail 0
```

Only `src/asciiRenderer.js` and `tests/ascii-renderer.test.mjs` were copied. The Playwright smoke test and the Vite build were not run
(they need a browser and a server, which the common brief forbids).

## Proposed integration

1. Skin signal (candidate 1). Where: **render path** (per-instance GPU shader, or CPU for a debug view), and optionally
   **Tier 1** (every 32 to 64 ticks) as a visible-signal channel in perception.
   ```c
   typedef struct { uint16_t d_min_nm, d_max_nm, ior_q8; uint8_t kx, ky; } skin_gene_t;   /* 8 bytes, from the genome */
   uint32_t skin_thickness_nm(const skin_gene_t *g, uint32_t u6, uint32_t v6);             /* 0..63 texel coords */
   void     skin_film_rgb8(const skin_gene_t *g, uint32_t d_nm, uint8_t rgb[3]);           /* integer, no libm */
   uint32_t skin_signal_rgb565(const skin_gene_t *g, uint32_t view_q8);                    /* one colour per creature for perception */
   ```
   Law 4 condition: this is decoration unless another creature's sensor reads `skin_signal_rgb565` (mate choice, kin
   recognition, warning colour) and a lesion (set every signal to grey) changes mating or predation statistics. If the
   perception channel is not built, keep it out of the simulation and use it in the renderer only.
2. ASCII field viewer (candidate 2). Where: **tooling only** (headless logs, CI artefacts, crash dumps of world fields).
   ```c
   void dbg_ascii_field_u8(const uint8_t *field, int w, int h, int32_t edge_thresh, char *out /* (w+1)*h+1 */);
   ```
3. gcd face count (candidate 3): no integration proposed; noted only.

## NOT usable, and why

- `homebrew-eshkol`, `homebrew-moonlab`: Ruby packaging recipes for macOS/Linux Homebrew. No numeric code. Their only value is as a
  record of upstream build flags (Moonlab: `QSIM_NATIVE_ARCH=OFF`, `QSIM_FAST_MATH=OFF`; Eshkol: LLVM 21 pin), which matter to
  whoever audits those two projects, not to the engine.
- `.github`: a portfolio README. Useful as a map of which Tsotchke repo claims what (spin networks, libirrep, QGT, PIMC, PINN);
  its claims are marketing text and were not verified here.
- `logo-lab` as code: JavaScript/JSX only (law 1), browser and DOM bound, no licence, and no custom shader source to port. The
  starfield LCG is sequential and float. Stock three.js materials are three.js's work (MIT), not this repo's.
- `SolanaQuantumFlux`: a TypeScript client for a paid network service (law 1 and the network-API exclusion), under a proprietary
  licence that forbids modification and derivative works. Remote "quantum" random numbers would also break law 6 (determinism)
  and law 7 (never call it quantum randomness). The SDK has no numeric kernel: `grep` for hash, xor or mixing code in
  `sdk/src/client.ts` finds only Base64/hex decoding of a returned u64. The statistical plots (bit balance, autocorrelation,
  lag plots) are an ordinary test list that could be applied to our Philox output as tooling; nothing was copied.

## Verdicts

| Repo | Verdict |
|---|---|
| homebrew-eshkol | not_applicable |
| homebrew-moonlab | not_applicable |
| .github | not_applicable |
| logo-lab | reference_only (interpreted language; two ideas re-implemented as integer C and run) |
| SolanaQuantumFlux | excluded_by_law (network API, TypeScript, proprietary licence) |

## Limits of this audit

- Triage depth only: READMEs, formulae, file lists and targeted greps; `mobius_material_lab_baseline.jsx` (2345 lines) was read in parts.
- No search for prior art was made; nothing here is claimed as new. Thin-film colour and ASCII edge rendering are well-known techniques.
- The skin signal has not been wired into any perception code, so its causal effect is unmeasured.
