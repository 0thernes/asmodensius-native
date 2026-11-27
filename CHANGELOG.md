# Changelog

All notable changes to `Asmodensius-Native` are documented in this file.
Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and Semantic Versioning.

## [1.2.0] - 2026-11-26

### Fixed (Closing All 11 Findings of the 2026-11-26 Statement Audit in `ERRATA.md`)
- **Implemented `core/az_chem.c` `spread()` Model (`A31`, `A32`, `A38`):** Added `chemSpreadBudget`, `chemSpreadStep`, and `chemSpreadSixFaces` in `lean/AsmodensiusProofs/Spec.lean` modeling the actual `left = a / 4` decreasing-budget six-face loop in `core/az_chem.c:160,180-199`, proving budget conservation, `total <= a / 4 <= a`, non-negative remaining mass `(a - total) + total = a` (`A31`), `a < 4` zero-outflow fast skip (`A32`), and refutation of any request tuple exceeding `a / 4` (`A38`). Retained `A22_documented_rule_c7_six_face_bound` clearly labeled as documented `docs/ARCHITECTURE.md` Rule C7 (`amt / 8`) only.
- **Restated Detached Theorems (`A07`..`A11`):** Restated `A07` to call `Spec.acceptSpecies`, `A08`/`A09` to call `Spec.arenaRollback` (eliminating tautological hypothesis-conjunction), and `A10`/`A11` to call `Spec.storeRelease`.
- **Renamed Inflated Sanity-Check Labels (`A12`, `A15`, `A16`, `A18`, `A19`, `A20`):** Renamed to `A12_sanity_readonly_hash_refl`, `A15_sanity_legacy_arith_two`, `A16_sanity_legacy_sum_240`, `A18_sanity_congruence_twin_hash`, `A19_sanity_identity_prefetch_hash`, and `A20_sanity_legacy_sum_244`, and honestly separated all `38` declarations into `26` substantive model/boundary/witness/refutation theorems plus `12` explicitly labeled sanity checks.
- **Tracked Law-14 Text Hygiene Checker (`tools/check_text.py`):** Added tracked `tools/check_text.py` (with `6/6` planted-fault `--self-test`) wired as a hard-failing CI gate across Ubuntu and Windows; enforces `0` violations on all active tracked files while transparently reporting verbatim owner prompts (`docs/handoff/PROMPTS.*`, 2 exempt) and historical archival dumps (`docs/requirements/`, `docs/research/`, `tests/upstream/`).
- **Rebuilt White Paper (`paper/main.tex` -> `dist/Donahue_2026_asmodensius_native.pdf`, 10 pp):** Added `\usepackage[obeyspaces]{url}`, corrected all 4 paper errata (`#1`..`#4`), corrected `carneiro2024lean4lean` arXiv title in `paper/refs.bib`, added SWHID (`swh:1:snp:08a957772115194b51a4eadb77331b8bab261773`), RFC 3161 + Bitcoin OTS timestamps, SLSA/Rekor provenance, and verified `0` `Overfull \hbox` + clean-room `dist/arxiv_submission.tar.gz`.

### Added
- **Adversarial False-Variant Refutations (`lean/AsmodensiusProofs/AdversarialTests.lean`, `A33`..`A38`):** Six concrete refutations (unguarded `budget > a` mass overdraw, out-of-range species `0` or `> 100`, non-zero arena delta without rollback, `cap > AZ_CAP_MAX`, non-8-aligned brain dims, and six-face spread exceeding `a / 4`), wired into `Challenge.lean`, `Solution.lean`, `VerifyAxioms.lean`, and `comparator.json`.
- **Live Negative Elaboration & `sorry`-Tamper Gate (`scripts/test_lean_negative.py`):** Verifies false claims fail Lean elaboration, `sorry`-tampering is caught by `VerifyAxioms.lean` (`sorryAx`), and `lake comparator --paranoid` rejects tampered solutions in Ubuntu CI.
- **Upgraded Live Citation Auditor (`scripts/check_citations.py` + `results/citation-audit.json`):** Checks HTTP reachability, Crossref/arXiv title+author alignment (`13/13` DOIs at `title=1.00`, `auth=OK`, `0` retractions), `2/2` vendor URLs, and `2` book/ISO standards.
- **Interactive Lean 4 Proof Blueprint & GitHub Pages (`docs/index.html`, `docs/blueprint.html`, `docs/site.css`, `docs/blueprint.js`, `scripts/check_blueprint.py`, `.github/workflows/pages.yml`):** WCAG 2.2 AA, strict-CSP, zero-CDN portal linking all `38/38` comparator targets.
- **Automated Tag Release & Provenance (`.github/workflows/release.yml`, `scripts/stamp_timestamps.py`):** `SHA256SUMS`, `actions/attest-build-provenance@v2`, DigiCert RFC 3161 `CHECKSUMS.sha256.tsr`, and 3-calendar Bitcoin `CHECKSUMS.sha256.ots`.
- **FAIR Metadata, Datasheet, Submission Readiness & REUSE/SPDX:** `croissant.json`, `ro-crate-metadata.json`, `codemeta.json`, `docs/DATASHEET.md`, `SUBMISSION_READINESS.md`, `REUSE.toml`, `LICENSES/Apache-2.0.txt`, `LICENSES/CC-BY-4.0.txt`.

### Changed (2026-11-26 statement audit commits `d658e1a` and `9fc56ec`, preserved)
- Scoped the Lean suite explicitly to the abstract `Nat` model in `Spec.lean` (not the C23, NASM, or CUDA PTX code).
- Distinguished 6 local Windows checkers (`lean`, `leanchecker`, `leanchecker-paranoid`, `con-leche`, `con-ron`, `nanoda_bin`) from Ubuntu-CI-only `lean4lean` (`lake check --paranoid`) and `lake comparator`.
- Preserved withdrawal of the untracked v1.1.0 "0 `check_text.py` violations" claim in `ERRATA.md`.

## [1.1.0] - 2026-11-25

### Added
- **Palomar-Ready Lean 4.35.0 Suite (`lean/`):** Pinned `lean-toolchain` (`leanprover/lean4:v4.35.0`), strict `lakefile.toml` (`autoImplicit = false`, `relaxedAutoImplicit = false`, `linter.unusedVariables = true`), `lake-manifest.json`, pure-definition `AsmodensiusProofs/Spec.lean`, `AsmodensiusProofs/Core.lean` (`A01`..`A26`), `AsmodensiusProofs/NonVacuity.lean` (`A27`..`A30`), isolated `Challenge.lean` vs `Solution.lean`, `comparator.json`, `VerifyAxioms.lean`, `formalization.yaml`, and `results/lean4-multi-kernel.txt`.
- **10 New Formal Theorems (`A21`..`A30`):** Rule C5 receive-cap mass conservation, documented Rule C7 6-face flux non-overdraw, Rule C2 `NUT_X` -> biomass energy conservation, ABI v4 23-bit lesion mask boundary, SIMD brain dimension alignment, TR-18 32 GiB VRAM byte-offset injectivity, 2 concrete non-vacuity witnesses (`A27`, `A28`), and 2 adversarial false-variant refutations (`A29`, `A30`).
- **Publication-Grade AMS-LaTeX White Paper (`paper/`, `dist/`):** `paper/main.tex` (with vector TikZ architecture and PGFPlots benchmark figures), 17 primary references in `paper/refs.bib`, `scripts/build_paper.py`, `scripts/check_citations.py`, `scripts/check_readme_axioms.py`, frozen `dist/Donahue_2026_asmodensius_native.pdf`, and clean-room verified `dist/arxiv_submission.tar.gz`.
- **Build Standard v1.2.0 Scaffolding:** Root `README.md`, `VERSION`, `LICENSE` (Apache-2.0), `LICENSE-DOCS` (CC-BY-4.0), `NOTICE`, `CITATION.cff`, `CITATION.bib`, `.zenodo.json`, `CLAUDE.md`, `GEMINI.md`, `.cursor/rules/build-standard.mdc`, `llms.txt`, `llms-full.txt`, `CLAIMS.md`, `LIMITATIONS.md`, `REPLICATION.md`, `DEVIATIONS.md`, `ERRATA.md`, `EVIDENCE.md`, `HARDENING_LOG.md`, `ROADMAP.md`, `UPGRADING.md`, `SECURITY.md`, `SUPPORT.md`, `CONTRIBUTING.md`, `CODE_OF_CONDUCT.md`, `.editorconfig`, `.gitattributes`, `.env.example`, `justfile`, `repo.yaml`, `CHECKSUMS.sha256`, `data/MANIFEST.sha256`, `examples/`, `.github/` templates/labels/CI, and full `docs/` suite including `docs/statement-fidelity.md`, `docs/MEGA_ATLAS_CROSSCHECK.md`, `docs/AI_DISCLOSURE.md`, `docs/media/social-preview.svg`, and `docs/reviews/2026-11-25-10-pass-red-team-audit.md`.

### Changed
- Standardized author attribution across active metadata to `Alexander Stephen Donahue` (`Independent Researcher, Spokane, WA, USA`, ORCID `0009-0006-8168-9648`) with dual Apache-2.0 / CC-BY-4.0 open licensing.
- Corrected `AZ_CAP_MAX` in `lean/AsmodensiusProofs/Spec.lean` to match `core/az_abi.h:410` (`0x7FFFFFC0`).

### Preserved
- Untouched freestanding C23, NASM AVX2/AVX-VNNI, and Clang 21 CUDA PTX simulation sources (`core/`, `kernels/`, `gpu/`, `platform/`, `tools/`, `view/`).
- `77/77` CTest PASS, `164/164` `test_gpu.exe` PASS, and all existing owner evaluation records (`docs/eval/TRACE.csv` intact). (Note: the earlier v1.1.0 claim of "`0` `check_text.py` violations" was withdrawn in the 2026-11-26 statement audit and superseded by tracked `tools/check_text.py` in `v1.2.0`; see `ERRATA.md` #5.)

## [1.0.0] - 2026-11-22

### Added
- 22-module freestanding C23 ABI v4 artificial-life core + 4 `tk_*` pure-integer kernels.
- Bare-metal multi-substrate compute stack closing `TR-01`, `TR-04`, `TR-07`, `TR-13`, and `TR-18`: bulk Philox AVX2, AVX-VNNI brain twin, DDR5 streaming software prefetch, 24-core `8P+16E` placement, and 41,817 B pure-integer Clang 21 CUDA PTX sparse brain/kinematics kernels on RTX 5070 Ti.
- Dual visual simulation worlds (`demo/viewer_server.py` WebGL2 + `demo/live_server.py`) with 1..1,000,000 founder population controls.
- Initial 20 Pure Lean 4 theorems (`A01`..`A20`) and `77/77` CTest passing suite.
