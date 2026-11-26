# Changelog

All notable changes to `Asmodensius-Native` are documented in this file.
Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and Semantic Versioning.

## [Unreleased]

### Changed (documentation only, 2026-11-26 statement audit)
- Scoped the Lean suite in `README.md`, `CLAIMS.md`, `LIMITATIONS.md`, `CITATION.cff` and `docs/`: the 30 theorems concern an abstract `Nat` model, not the C23, NASM or PTX code; 13 are statements about the model and 17 are definitional, constant or literal-arithmetic checks.
- Stated that `A22` formalizes documented Rule C7 (per-face `amt / 8`), while `core/az_chem.c` uses an `a / 4` per-species budget; corrected its source mapping in `docs/statement-fidelity.md`.
- Corrected "5 Windows-capable kernels" to 6 local checkers.
- Withdrew the "0 `check_text.py` violations" statement of the 1.1.0 entry: the checker is not tracked here and is not run in CI.
- Recorded paper errors and known Lean naming and fidelity issues in `ERRATA.md`. No Lean, C, paper or CI file changed.

## [1.1.0] - 2026-11-25

### Added
- **6-Kernel Palomar-Ready Lean 4.35.0 Suite (`lean/`):** Pinned `lean-toolchain` (`leanprover/lean4:v4.35.0`), strict `lakefile.toml` (`autoImplicit = false`, `relaxedAutoImplicit = false`, `linter.unusedVariables = true`), `lake-manifest.json`, pure-definition `AsmodensiusProofs/Spec.lean`, `AsmodensiusProofs/Core.lean` (`A01`..`A26`), `AsmodensiusProofs/NonVacuity.lean` (`A27`..`A30`), isolated `Challenge.lean` vs `Solution.lean`, `comparator.json`, `VerifyAxioms.lean`, `formalization.yaml`, and `results/lean4-multi-kernel.txt`.
- **10 New Formal Theorems (`A21`..`A30`):** Rule C5 receive-cap mass conservation, Rule C7 6-face flux non-overdraw, Rule C2 `NUT_X` -> biomass energy conservation, ABI v4 23-bit lesion mask boundary, SIMD brain dimension alignment, TR-18 32 GiB VRAM byte-offset injectivity, 2 concrete non-vacuity witnesses (`A27`, `A28`), and 2 adversarial false-variant refutations (`A29`, `A30`).
- **Publication-Grade AMS-LaTeX White Paper (`paper/`, `dist/`):** `paper/main.tex` (with vector TikZ architecture and PGFPlots benchmark figures), 17 primary references in `paper/refs.bib`, `scripts/build_paper.py`, `scripts/check_citations.py`, `scripts/check_readme_axioms.py`, frozen `dist/Donahue_2026_asmodensius_native.pdf`, and clean-room verified `dist/arxiv_submission.tar.gz`.
- **Build Standard v1.2.0 Scaffolding:** Root `README.md`, `VERSION`, `LICENSE` (Apache-2.0), `LICENSE-DOCS` (CC-BY-4.0), `NOTICE`, `CITATION.cff`, `CITATION.bib`, `.zenodo.json`, `CLAUDE.md`, `GEMINI.md`, `.cursor/rules/build-standard.mdc`, `llms.txt`, `llms-full.txt`, `CLAIMS.md`, `LIMITATIONS.md`, `REPLICATION.md`, `DEVIATIONS.md`, `ERRATA.md`, `EVIDENCE.md`, `HARDENING_LOG.md`, `ROADMAP.md`, `UPGRADING.md`, `SECURITY.md`, `SUPPORT.md`, `CONTRIBUTING.md`, `CODE_OF_CONDUCT.md`, `.editorconfig`, `.gitattributes`, `.env.example`, `justfile`, `repo.yaml`, `CHECKSUMS.sha256`, `data/MANIFEST.sha256`, `examples/`, `.github/` templates/labels/CI, and full `docs/` suite including `docs/statement-fidelity.md`, `docs/MEGA_ATLAS_CROSSCHECK.md`, `docs/AI_DISCLOSURE.md`, `docs/media/social-preview.svg`, and `docs/reviews/2026-11-25-10-pass-red-team-audit.md`.

### Changed
- Standardized author attribution across active metadata to `Alexander Stephen Donahue` (`Independent Researcher, Spokane, WA, USA`, ORCID `0009-0006-8168-9648`) with dual Apache-2.0 / CC-BY-4.0 open licensing.
- Corrected `AZ_CAP_MAX` in `lean/AsmodensiusProofs/Spec.lean` to match `core/az_abi.h:410` (`0x7FFFFFC0`).

### Preserved
- Untouched freestanding C23, NASM AVX2/AVX-VNNI, and Clang 21 CUDA PTX simulation sources (`core/`, `kernels/`, `gpu/`, `platform/`, `tools/`, `view/`).
- `77/77` CTest PASS, `164/164` `test_gpu.exe` PASS, `0` `check_text.py` violations (Law 14), and all existing owner evaluation records (`docs/eval/TRACE.csv` intact).

## [1.0.0] - 2026-11-22

### Added
- 22-module freestanding C23 ABI v4 artificial-life core + 4 `tk_*` pure-integer kernels.
- Bare-metal multi-substrate compute stack closing `TR-01`, `TR-04`, `TR-07`, `TR-13`, and `TR-18`: bulk Philox AVX2, AVX-VNNI brain twin, DDR5 streaming software prefetch, 24-core `8P+16E` placement, and 41,817 B pure-integer Clang 21 CUDA PTX sparse brain/kinematics kernels on RTX 5070 Ti.
- Dual visual simulation worlds (`demo/viewer_server.py` WebGL2 + `demo/live_server.py`) with 1..1,000,000 founder population controls.
- Initial 20 Pure Lean 4 theorems (`A01`..`A20`) and `77/77` CTest passing suite.
