# Submission & Archival Readiness Ledger - Asmodensius-Native (`v1.2.0`)

**Author:** Alexander Stephen Donahue (`Independent Researcher, Spokane, WA, USA`)
**ORCID:** [`0009-0006-8168-9648`](https://orcid.org/0009-0006-8168-9648)
**Contact:** `0_0@0thernes.art` | **Repository:** [`https://github.com/0thernes/asmodensius-native`](https://github.com/0thernes/asmodensius-native) | **Portal:** [`https://0thernes.github.io/asmodensius-native/`](https://0thernes.github.io/asmodensius-native/)
**Status:** Independent computational-science and formal-model technical preprint (**not peer-reviewed**; zero fabricated DOIs; zero claims of sentience, consciousness, biological life, AGI, or quantum speedup; **`0 / 1,900`** overarching Mega-Atlas open problems retired).

## Verification Gates Summary

| Gate | Requirement | Artifact / Command | Status |
|---|---|---|---|
| **Lean 4.35.0 Model Suite (38 Declarations)** | Pure zero-Mathlib Lean 4.35.0, `autoImplicit = false`, 0 `sorry` in `Solution`, 0 `native_decide`, standard axioms `[propext, Classical.choice, Quot.sound]` | `lean/` (26 substantive model/boundary/witness/refutation theorems incl. `core/az_chem.c` `a/4` spread, `acceptSpecies`, `arenaRollback`, `storeRelease`, 8 adversarial refutations + 12 explicitly labeled sanity checks); C23/NASM/PTX code NOT verified in Lean | **PASS** |
| **Multi-Checker & Palomar Comparator** | 6 local Windows checkers (`lean`, `leanchecker`, `leanchecker-paranoid`, `con-leche`, `con-ron`, `nanoda_bin`) + Ubuntu CI `lean4lean` (`lake check --paranoid`) & `lake comparator --paranoid` | `results/lean4-asmodensius-audit.txt`, `results/lean4-multi-kernel.txt`, `lean/comparator.json`, `.github/workflows/ci.yml` | **PASS** |
| **Live Negative Elaboration & Tamper Gate** | False claims fail elaboration; injecting `sorry` is caught by `VerifyAxioms.lean` (`sorryAx`) and rejected by `lake comparator --paranoid` on Ubuntu CI | `scripts/test_lean_negative.py` (`--with-comparator` in CI) | **PASS** |
| **Tracked Law-14 Text Hygiene Gate** | Strict 7-bit ASCII, LF, no tabs, trailing newline across all active tracked files, with transparent reporting of verbatim owner prompts (2 exempt) and archival research/requirements dumps | `tools/check_text.py` (6/6 self-test + 0 active violations; wired in CI) | **PASS** |
| **Native Engine & GPU Differential Suite** | Freestanding C23 purity gates, UBSan trap build, determinism matrices, 164/164 RTX 5070 Ti PTX checks (0 differing bits across 1M brains + 1M kinematics), `--species 101` exit 2 | `CMakeLists.txt`, `gpu/test_gpu.c`, `docs/benchmarks.md`, `EVIDENCE.md` | **PASS** |
| **Live Citation & Title/Author Integrity** | 1-to-1 BibTeX <-> LaTeX coverage, HTTP 200 reachability, Crossref/arXiv title+author alignment, 0 retractions | `scripts/check_citations.py --live --write-report` -> `results/citation-audit.json` (17/17 covered; 13/13 DOIs title=1.00, auth=OK, 0 retractions; 2/2 vendor URLs HTTP 200; 2 book/ISO standards) | **PASS** |
| **AMS-LaTeX Preprint & arXiv Bundle** | Zero `Overfull \hbox`, zero undefined refs/citations, `\usepackage[obeyspaces]{url}`, clean-room tarball compile | `paper/main.tex`, `dist/Donahue_2026_asmodensius_native.pdf` (10 pp), `dist/arxiv_submission.tar.gz`, `scripts/build_paper.py` | **PASS** |
| **Interactive Blueprint & GitHub Pages** | Strict-CSP, zero-CDN, WCAG 2.2 AA portal & Lean proof blueprint linking all 38 comparator targets | `docs/index.html`, `docs/blueprint.html`, `docs/site.css`, `docs/blueprint.js`, `scripts/check_blueprint.py`, `.github/workflows/pages.yml` | **PASS** |
| **Cryptographic Provenance & FAIR Metadata** | LF-canonical SHA-256 manifests, DigiCert RFC 3161 `.tsr`, 3-calendar Bitcoin `.ots`, SLSA/Rekor tag release workflow, Croissant 1.0, RO-Crate 1.1, CodeMeta 2.0, REUSE/SPDX, Datasheet | `CHECKSUMS.sha256`, `CHECKSUMS.sha256.tsr`, `CHECKSUMS.sha256.ots`, `data/MANIFEST.sha256`, `.github/workflows/release.yml`, `croissant.json`, `ro-crate-metadata.json`, `codemeta.json`, `REUSE.toml`, `LICENSES/`, `docs/DATASHEET.md` | **PASS** |
| **Build Standard v1.2.0 Audit** | Automated 90-item repository standard audit | `scripts/Audit-Repo.ps1` & `scripts/audit-repo.sh` (`100.0%` structure / completion / core) | **PASS** |

## Archival & Compliance Notes

- **Software Heritage:** Preserved snapshot `swh:1:snp:08a957772115194b51a4eadb77331b8bab261773`.
- **Dual Licensing (REUSE/SPDX):** `Apache-2.0` (`LICENSE`, `LICENSES/Apache-2.0.txt`) for code, Lean 4 proofs, scripts, and workflows; `CC-BY-4.0` (`LICENSE-DOCS`, `LICENSES/CC-BY-4.0.txt`) for white paper, documentation, portal, and evaluation CSV datasets; third-party `MIT` test oracles under `tests/upstream/` (`THIRD_PARTY_NOTICES.md`).
- **Errata Closure:** All 11 findings logged in the 2026-11-26 statement audit (`ERRATA.md`) are explicitly addressed in `v1.2.0` while preserving the full chronological audit trail.
- **Peer Review & Endorsement Policy:** No external peer review, journal acceptance, or arXiv endorsement is claimed. AI coding assistance is transparently disclosed in `docs/AI_DISCLOSURE.md` and `paper/main.tex`.
