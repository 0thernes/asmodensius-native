# Asmodensius-Native

Asmodensius-Native is a Windows-native, freestanding C23, NASM AVX2/AVX-VNNI, and pure-integer CUDA PTX artificial-life simulation engine with exact integer conservation, bit-exact multi-substrate replay, and a 38-declaration Lean 4.35.0 suite about an abstract `Nat` model of its rules (26 substantive model, boundary, witness, and adversarial refutation theorems + 12 explicitly labeled sanity checks). The C23, NASM, and CUDA PTX code itself is not verified in Lean.

![Status](https://img.shields.io/badge/status-active-2ea44f)
![Version](https://img.shields.io/badge/version-1.2.0-184e77)
![CTest](https://img.shields.io/badge/ctest-77%2F77%20baseline-168aad)
![GPU](https://img.shields.io/badge/test__gpu-164%2F164%20pass-168aad)
![Lean 4](https://img.shields.io/badge/Lean%204.35.0-38%20decls%20%7C%206%20local%20%2B%20CI-184e77)
![Code License](https://img.shields.io/badge/code-Apache--2.0-blue)
![Docs License](https://img.shields.io/badge/docs%20%26%20paper-CC--BY--4.0-blue)

status: active

Portal & Interactive Proof Blueprint: <https://0thernes.github.io/asmodensius-native/> ([docs/blueprint.html](docs/blueprint.html))

![Asmodensius-Native v1.2.0 Architecture and Verification Overview](docs/media/social-preview.svg)

## Key Properties

- **Freestanding C23 Core (`core/`):** 22 selectable biology/physics modules plus 4 pure-integer mathematical kernels (`tk_sh`, `tk_hop`, `tk_qgt`, `tk_qsv`), compiled with `-ffreestanding`, zero libc calls, zero floating-point operations, zero writable globals, and stack frames `<= 2048` bytes (`core_obj_gate`, `avx_stack_gate`, `ubsan_trap_build`).
- **Single Page-Locked Arena:** All state resides in one pre-allocated Windows `VirtualLock` arena using 128-entity Structure-of-Arrays (SoA) blocks (`AZ_CHUNK = 128`) and a 64-byte (`AZ_UNIT`) two-half compacting creature store with zero pagefile or NVMe swap backing.
- **Exact Integer Conservation (Law 5):** Global matter (`az_total_matter`) and energy (`az_total_energy`) invariants hold with exact `==` after every tick, with `field_underflow == 0` and `dropped_events == 0`. Implemented `core/az_chem.c` `spread()` caps per-species outflow at `left = a / 4` under a decreasing budget across all six faces.
- **Wild by Default, Bit-Exact in Lab Mode (Law 6):** Unflagged runs draw fresh OS entropy (`BCryptGenRandom`) every tick. `--lab --seed S` uses counter-based Philox4x32-10 streams and canonical UID order for bit-exact `az_state_hash` parity across scalar C23 (`--kernels 0`), AVX2/AVX-VNNI NASM twins (`--kernels 1`), 24-core P+E (`--ecores`), and RTX 5070 Ti CUDA PTX offload (`--gpu 1`).
- **Pure-Integer CUDA PTX Backend (`gpu/`):** Clang 21 compiles literal freestanding `core/az_brain.c`, `core/az_rng.c`, and `core/az_kin_axis.c` to `41,817` bytes of PTX with 0 externs and 0 floats (`164/164` `test_gpu.exe` pass across 1,000,000 variable-size sparse brains and 1,000,000 kinematics lanes with 0 differing bits).
- **Lean 4.35.0 Model Suite (`lean/`):** 38 machine-checked declarations (`A01`..`A38`, zero Mathlib, zero `sorry` in `Solution`, zero `native_decide`) about hand-written `Nat` definitions in `lean/AsmodensiusProofs/Spec.lean`: **26 substantive model, boundary, witness, and adversarial refutation theorems** (including `A31`/`A32`/`A38` modeling the implemented `core/az_chem.c` `a / 4` decreasing-budget pass, restated `A07`..`A11` genuinely invoking `acceptSpecies`, `arenaRollback`, `storeRelease`, `A22_documented_rule_c7_six_face_bound` clearly scoped to documented Rule C7 `amt / 8`, and 6 false-variant refutations in `AdversarialTests.lean`) plus **12 explicitly labeled definitional, constant, congruence, or legacy arithmetic sanity checks** (`A04`..`A06`, `A12_sanity_*`..`A16_sanity_*`, `A18_sanity_*`..`A20_sanity_*`, `A25`). Checked locally on Windows by 6 checkers (`lean4` kernel, `leanchecker`, `leanchecker-paranoid`, `con-leche`, `con-ron`, `nanoda_bin`; `results/lean4-multi-kernel.txt`). Ubuntu CI also runs `lake check --paranoid` (adding `lean4lean`), `lake comparator --paranoid` under `bubblewrap`, and `scripts/test_lean_negative.py --with-comparator`.
- **Tracked Law-14 Text Gate (`tools/check_text.py`):** Wired as a hard-failing gate in CI (`6/6` planted-fault `--self-test`), enforcing 0 violations (7-bit ASCII, LF, no tabs, trailing newline) across all active tracked files while transparently counting and reporting 2 exempt verbatim owner prompts (`docs/handoff/PROMPTS.*`) and historical archival dumps (`docs/requirements/`, `docs/research/`, `tests/upstream/`).
- **White Paper (`paper/` & `dist/`) & Cryptographic Provenance:** 10-page AMS-LaTeX preprint (`dist/Donahue_2026_asmodensius_native.pdf`) and clean-room verified `dist/arxiv_submission.tar.gz` with 17 primary references (`13/13` DOIs live-verified for reachability + Crossref/arXiv title & author match with 0 retractions in `results/citation-audit.json`), Software Heritage snapshot `swh:1:snp:08a957772115194b51a4eadb77331b8bab261773`, dual timestamps (`CHECKSUMS.sha256.tsr` DigiCert RFC 3161 + `CHECKSUMS.sha256.ots` 3-calendar Bitcoin OTS), SLSA/Rekor provenance, and FAIR metadata (`croissant.json`, `ro-crate-metadata.json`, `codemeta.json`, `REUSE.toml`, `docs/DATASHEET.md`).

## Honest Scope, Indicator-Only Boundary, and Mega-Atlas Non-Retirement

- **Indicator-Only Discipline (Law 8):** `Asmodensius-Native` makes **zero claims** of consciousness, sentience, biological life, AGI, or quantum speedup. Biological names label deterministic integer structs; `butlin14` is an engineering indicator checklist only; `tk_qsv` and `tk_qgt` are classical fixed-point matrix routines.
- **Formal Scope:** The Lean declarations are about an abstract `Nat` model in `Spec.lean`, not the compiled C23/NASM/PTX engine. The 12 sanity checks (`A04`..`A06`, `A12_sanity_*`, `A13`..`A14`, `A15_sanity_*`, `A16_sanity_*`, `A18_sanity_*`, `A19_sanity_*`, `A20_sanity_*`, `A25`) are explicitly labeled as definitional, constant, congruence, or legacy arithmetic checks (see [docs/statement-fidelity.md](docs/statement-fidelity.md) and [ERRATA.md](ERRATA.md)).
- **Not Peer-Reviewed:** The white paper is an independent technical preprint by Alexander Stephen Donahue (Independent Researcher, Spokane, WA, USA; ORCID `0009-0006-8168-9648`).
- **Mega-Atlas Cross-Check:** **`0 / 1,900` overarching problems** in `0thernes/unsolved-problems-mega-atlas` are solved or retired. See [docs/MEGA_ATLAS_CROSSCHECK.md](docs/MEGA_ATLAS_CROSSCHECK.md) for scoped probe mappings (`AI-045`, `BIO-001`, `BIO-008`, `SUB-047`, `SYS-050`).
- **Honest Hardware Tradeoffs:** On Intel Core Ultra 9 275HX + RTX 5070 Ti (`--lab --seed 42`, `locked=1`), 24C AVX-VNNI+PF reaches `72.199 ms/tick` at 100k (`+56.7%`) and `757.988 ms/tick` at 1M (`+35.3%`, `9,260.6 MB` locked), whereas 1M GPU offload (`1,122.495 ms/tick`, bit-exact hash `31e5d00c106ce679`) is slower than 24C CPU due to PCIe staging and two-half store compaction. Windows `--large-pages` falls back cleanly to 4 KiB `VirtualLock` unless `SeLockMemoryPrivilege` is granted (`TR-03`).

## Quickstart

### Build and Test on Windows (Native Engine)

```powershell
$root = (Get-Location).Path
$env:TMP = Join-Path $root 'build\tmp'
$env:TEMP = $env:TMP
[void][IO.Directory]::CreateDirectory($env:TMP)

cmake -S $root -B (Join-Path $root 'build\z') -G Ninja
cmake --build (Join-Path $root 'build\z')
ctest --test-dir (Join-Path $root 'build\z') --output-on-failure
```

### Verify Lean 4.35.0 Model Proofs & Negative Gate (Windows or Linux)

```powershell
Set-Location -LiteralPath 'lean'
lake build
lake build Challenge
lake env lean VerifyAxioms.lean
lake env leanchecker --fresh Solution
lake env leanchecker-paranoid --fresh Solution
Set-Location -LiteralPath '..'
python -I scripts/test_lean_negative.py
```

### Build White Paper, Check Text Hygiene, Timestamps, Blueprint & Audit Repository

```powershell
python -I tools/check_text.py --self-test
python -I tools/check_text.py
python -I scripts/build_paper.py
python -I scripts/check_citations.py --live
python -I scripts/check_readme_axioms.py
python -I scripts/check_blueprint.py
python -I scripts/stamp_timestamps.py --verify
pwsh -NoProfile -File scripts/Audit-Repo.ps1
```

## Usage

### Run a Deterministic Lab World with Multi-Substrate Parity

```powershell
# Scalar freestanding C23 reference
.\build\z\az_run.exe --lab --seed 42 --n 1000 --ticks 10 --kernels 0

# AVX-VNNI + bulk Philox AVX2 + horizontal kinematics AVX2 twins (identical hash 7bd9823448dd38b1)
.\build\z\az_run.exe --lab --seed 42 --n 1000 --ticks 10 --kernels 1

# Live RTX 5070 Ti CUDA PTX sparse brain offload (identical hash 7bd9823448dd38b1)
.\build\z\az_run.exe --lab --seed 42 --n 1000 --ticks 10 --kernels 1 --gpu 1

# All 24 physical cores (8P + 16E) at 100k founders
.\build\z\az_run.exe --lab --seed 42 --n 100000 --ticks 32 --kernels 1 --ecores --prefetch 1
```

### Run a Default Wild World (Fresh OS Entropy Every Tick)

```powershell
.\build\z\az_run.exe --n 500 --ticks 5
```

### Verify Explicit Refusal Contract (`WG_MAX_SPECIES = 100`)

```powershell
# Refuses species > 100 before arena allocation with exit code 2
.\build\z\az_run.exe --lab --seed 42 --n 100 --species 101 --ticks 1
```

### Run Standalone GPU Differential Suite (`164/164`)

```powershell
.\build\z\test_gpu.exe
```

## 5-Second Independent Proof Verification (`#print axioms`)

Every line below is captured verbatim from `lake env lean VerifyAxioms.lean` (`results/lean4-asmodensius-audit.txt`) and guarded by `python3 -I scripts/check_readme_axioms.py`:

<!-- LEAN_AXIOMS_START -->
```text
'AsmodensiusProofs.A01_bio_to_cell_mass_conserved' depends on axioms: [propext, Quot.sound]
'AsmodensiusProofs.A02_energy_dissipation_conserved' depends on axioms: [propext, Quot.sound]
'AsmodensiusProofs.A03_heat_sink_monotone' depends on axioms: [propext, Quot.sound]
'AsmodensiusProofs.A04_underflow_preserved_zero' does not depend on any axioms
'AsmodensiusProofs.A05_dropped_events_preserved_zero' does not depend on any axioms
'AsmodensiusProofs.A06_species_cap_exact' does not depend on any axioms
'AsmodensiusProofs.A07_species_refusal_boundary' depends on axioms: [propext, Quot.sound]
'AsmodensiusProofs.A08_arena_rollback_exact' depends on axioms: [propext, Quot.sound]
'AsmodensiusProofs.A09_arena_retry_succeeds' depends on axioms: [propext, Quot.sound]
'AsmodensiusProofs.A10_store_release_reduces_live' depends on axioms: [propext, Quot.sound]
'AsmodensiusProofs.A11_store_release_exact_balance' depends on axioms: [propext, Classical.choice, Quot.sound]
'AsmodensiusProofs.A12_sanity_readonly_hash_refl' does not depend on any axioms
'AsmodensiusProofs.A13_trait_abi_width_112' does not depend on any axioms
'AsmodensiusProofs.A14_eps_bio_positive' depends on axioms: [propext, Quot.sound]
'AsmodensiusProofs.A15_sanity_legacy_arith_two' does not depend on any axioms
'AsmodensiusProofs.A16_sanity_legacy_sum_240' does not depend on any axioms
'AsmodensiusProofs.A17_bulk_philox_key_equivalence' depends on axioms: [propext, Quot.sound]
'AsmodensiusProofs.A18_sanity_congruence_twin_hash' does not depend on any axioms
'AsmodensiusProofs.A19_sanity_identity_prefetch_hash' does not depend on any axioms
'AsmodensiusProofs.A20_sanity_legacy_sum_244' does not depend on any axioms
'AsmodensiusProofs.A21_receive_cap_mass_conserved' depends on axioms: [propext, Classical.choice, Quot.sound]
'AsmodensiusProofs.A22_documented_rule_c7_six_face_bound' depends on axioms: [propext, Quot.sound]
'AsmodensiusProofs.A23_nut_x_to_bio_energy_conserved' depends on axioms: [propext, Quot.sound]
'AsmodensiusProofs.A24_lesion_mask_all_exact' does not depend on any axioms
'AsmodensiusProofs.A25_brain_dim_kernel_alignment' does not depend on any axioms
'AsmodensiusProofs.A26_gpu_offset8_injective_and_32gib_range' depends on axioms: [propext, Quot.sound]
'AsmodensiusProofs.A27_nonvacuity_ledger_and_arena_witness' does not depend on any axioms
'AsmodensiusProofs.A28_nonvacuity_philox_and_gpu_witness' depends on axioms: [propext]
'AsmodensiusProofs.A29_refute_unbounded_species_accepted' does not depend on any axioms
'AsmodensiusProofs.A30_refute_unbounded_philox_bulk_key_eq' depends on axioms: [propext]
'AsmodensiusProofs.A31_chem_spread_quarter_budget_safe' depends on axioms: [propext, Classical.choice, Quot.sound]
'AsmodensiusProofs.A32_chem_spread_small_pool_zero' depends on axioms: [propext, Classical.choice, Quot.sound]
'AsmodensiusProofs.A33_refute_unguarded_budget_overdraws_mass' depends on axioms: [propext, Quot.sound]
'AsmodensiusProofs.A34_refute_out_of_range_species_accepted' depends on axioms: [propext, Quot.sound]
'AsmodensiusProofs.A35_refute_nonzero_delta_without_rollback' depends on axioms: [propext, Quot.sound]
'AsmodensiusProofs.A36_refute_invalid_cap_exceeds_max' does not depend on any axioms
'AsmodensiusProofs.A37_refute_unaligned_brain_dim' does not depend on any axioms
'AsmodensiusProofs.A38_refute_spread_exceeds_quarter' depends on axioms: [propext, Classical.choice, Quot.sound]
```
<!-- LEAN_AXIOMS_END -->

## Documentation Index

- Architecture & ABI v4 Contract: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md), [docs/adr/0001-architecture-and-verification-boundaries.md](docs/adr/0001-architecture-and-verification-boundaries.md)
- White Paper (Frozen PDF & arXiv Tarball): [paper/main.tex](paper/main.tex), [dist/Donahue_2026_asmodensius_native.pdf](dist/Donahue_2026_asmodensius_native.pdf), [dist/arxiv_submission.tar.gz](dist/arxiv_submission.tar.gz), [results/citation-audit.json](results/citation-audit.json)
- Formal Verification, Blueprint & Statement Fidelity: [lean/README.md](lean/README.md), [docs/blueprint.html](docs/blueprint.html), [docs/statement-fidelity.md](docs/statement-fidelity.md), [results/lean4-multi-kernel.txt](results/lean4-multi-kernel.txt)
- Claims, Errata, Limitations & Replication: [CLAIMS.md](CLAIMS.md), [ERRATA.md](ERRATA.md), [LIMITATIONS.md](LIMITATIONS.md), [REPLICATION.md](REPLICATION.md), [EVIDENCE.md](EVIDENCE.md), [SUBMISSION_READINESS.md](SUBMISSION_READINESS.md)
- Mega-Atlas Cross-Check, Datasheet & AI Disclosure: [docs/MEGA_ATLAS_CROSSCHECK.md](docs/MEGA_ATLAS_CROSSCHECK.md), [docs/DATASHEET.md](docs/DATASHEET.md), [docs/AI_DISCLOSURE.md](docs/AI_DISCLOSURE.md)
- Benchmarks, Preregistration & Trigger Register: [docs/benchmarks.md](docs/benchmarks.md), [docs/preregistration.md](docs/preregistration.md), [docs/TRIGGER-REGISTER.md](docs/TRIGGER-REGISTER.md)
- Red-Team Audit & Hardening Log: [docs/reviews/2026-11-25-10-pass-red-team-audit.md](docs/reviews/2026-11-25-10-pass-red-team-audit.md), [HARDENING_LOG.md](HARDENING_LOG.md)

## Licensing

- **Code, Lean 4 proofs, scripts, and CI workflows:** Apache License 2.0 ([LICENSE](LICENSE), [LICENSES/Apache-2.0.txt](LICENSES/Apache-2.0.txt)).
- **White paper, PDFs, documentation, figures, portal, and datasets:** Creative Commons Attribution 4.0 International ([LICENSE-DOCS](LICENSE-DOCS), [LICENSES/CC-BY-4.0.txt](LICENSES/CC-BY-4.0.txt)).
- **REUSE / SPDX manifest:** [REUSE.toml](REUSE.toml); third-party MIT test oracles (`tests/upstream/`): [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) and [NOTICE](NOTICE).
