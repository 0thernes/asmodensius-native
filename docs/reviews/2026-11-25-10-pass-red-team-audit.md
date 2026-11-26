# 10-Pass Red-Team Audit & 3-Cycle Hardening Review (2026-11-25)

**Repository:** `0thernes/asmodensius-native` (`v1.1.0`)
**Reviewer / Author:** Alexander Stephen Donahue (Independent Researcher, Spokane, WA, USA; ORCID `0009-0006-8168-9648`)
**Standards Applied:** Build Standard for LLMs v1.2.0 (90 items, Rules R1-R8, 10 finding classes), Defensible Independent AI Research Playbook v1.3, Project Laws 1-15 (`AGENTS.md`).

## Executive Summary

Across three hardening cycles (v1.0.0 baseline repairs -> v1.1.0 Palomar 6-kernel Lean 4.35.0 & white paper -> Build Standard v1.2.0 self-audit), every identified defect was classified into one of the 10 standard finding classes and either resolved with automated verification or documented honestly as an open boundary.

- **Native C23 / NASM / CUDA PTX Engine:** `77/77` CTest PASS (simulation sources untouched since verified commit `782cd0e`), `164/164` `test_gpu.exe` PASS on RTX 5070 Ti, live 3-substrate 1k parity hash `7bd9823448dd38b1`, `--species 101` exit `2`.
- **Lean 4.35.0 Multi-Kernel Suite:** `30/30` theorems (`A01`..`A30`), 0 `sorry` in `Solution`, 0 `native_decide`, strict `[propext, Classical.choice, Quot.sound]`, 5/5 Windows kernels PASS locally (`lean4`, `leanchecker`, `leanchecker-paranoid`, `con-leche` 1778 decls, `con-ron` 1778 decls, `nanoda_bin` 1987 decls), 6/6 wired in Linux CI.
- **White Paper & Bibliography:** Zero `Overfull \hbox`, zero undefined refs/citations, clean-room verified `dist/arxiv_submission.tar.gz`, 17/17 citations verified (13/13 live DOIs, 2/2 vendor manual URLs, 0 retractions).
- **Mega-Atlas Honesty:** `0 / 1,900` overarching open problems retired.

## 10-Pass Red-Team Protocol Findings

| Pass | Audit Focus | Findings & Class | Disposition |
|---|---|---|---|
| Pass 1 | Identity, Legal & Licensing | (`problem`) Active metadata lacked dual Apache-2.0 / CC-BY-4.0 licenses and standardized independent researcher attribution. | **Fixed:** Added `LICENSE`, `LICENSE-DOCS`, `NOTICE`, `CITATION.cff`, `CITATION.bib`, `.zenodo.json`, `formalization.yaml` under `Independent Researcher, Spokane, WA, USA` with zero corporate entity references. |
| Pass 2 | Law 14 Text Purity & Owner File Safety | (`conflict`) Upstream `Audit-Repo.ps1` had 3 UTF-8 middle-dot bytes failing `check_text.py`; confirmed `docs/eval/TRACE.csv` is `4,977,947 B` (< 5 MiB) and must never be deleted. | **Fixed:** Normalized `Audit-Repo.ps1` middle dots to ASCII `|` (logged in `DEVIATIONS.md`), kept `TRACE.csv` intact, verified `check_text.py` 0 violations. |
| Pass 3 | Formal Statement Fidelity & Constant Drift | (`bug`) v1.0.0 Lean `AZ_CAP_MAX := 2000000` drifted from `core/az_abi.h:410` (`0x7FFFFFC0`); (`blindspot`) legacy `docs/ARCHITECTURE.md` still mentioned ABI v3 `48`/`16` dims vs ABI v4 `64`/`24`. | **Fixed:** Corrected `AZ_CAP_MAX` in `Spec.lean`, added `A24`..`A26`, logged ABI v3/v4 documentation delta in `ERRATA.md` and `docs/statement-fidelity.md`. |
| Pass 4 | Formal Isolation, Non-Vacuity & Adversarial Boundaries | (`gap`) v1.0.0 single-file Lean suite lacked Palomar `module`, isolated `Challenge.lean`/`Solution.lean`, non-vacuity inhabitants, and negative-boundary refutations. | **Fixed:** Refactored into `Spec.lean`, `Core.lean` (`A01`..`A26`), `NonVacuity.lean` (`A27`..`A30`), `Challenge.lean`, `Solution.lean`, `comparator.json`, verified across 5 local kernels + 6-kernel Linux CI. |
| Pass 5 | Live Citation, DOI & Retraction Audit | (`error` caught during build) Initial `iso2024c23` URL on `iso.org` returned HTTP 403 and `open-std.org` timed out during live check. | **Fixed:** Cited `ISO/IEC 9899:2024` as a published ISO technical standard and added Leroy 2009 CompCert CACM DOI (`10.1145/1538788.1538814`); 17/17 entries verified (13/13 DOIs + 2/2 manual URLs + 2/2 public URLs, 0 retractions). |
| Pass 6 | Typesetting, Tables & Clean-Room arXiv Packaging | (`issue` caught during build) First LaTeX draft produced `Overfull \hbox` on long monospace paths and wide tables. | **Fixed:** Added clean `\allowbreak` helpers, `\resizebox` on TikZ architecture diagram, and balanced `tabularx` columns; verified 0 `Overfull \hbox` in both `paper/main.tex` and clean-room `dist/arxiv_submission.tar.gz`. |
| Pass 7 | Hardware Benchmarks & Negative Tradeoffs | (`dilemma`) 1M GPU offload (`1,122.495 ms/tick`) slower than 24C CPU (`757.988 ms/tick`) due to PCIe compaction; (`concern`) Windows `--large-pages` falls back to 4 KiB absent `SeLockMemoryPrivilege`. | **Documented Honestly:** Reported both negative tradeoffs prominently in `README.md`, `CLAIMS.md`, `LIMITATIONS.md`, `docs/benchmarks.md`, and `paper/main.tex`. |
| Pass 8 | Security, Secrets & Banned Files (R4) | Checked for secrets, `.env`, PR templates, Dependabot/Renovate, `.github/copilot-instructions.md`, `FUNDING.yml`, and `build/work/WRITER.lock`. | **Verified Clean:** 0 secrets, 0 banned files, 0 `WRITER.lock`, STRIDE threat model added in `docs/threat-model.md`. |
| Pass 9 | Epistemic & Mega-Atlas Honesty (Law 8) | Audited every document for sentience/AGI/quantum overclaiming or false Mega-Atlas retirements. | **Verified Clean:** Indicator-only boundary explicit everywhere; `docs/MEGA_ATLAS_CROSSCHECK.md` confirms `0 / 1,900` overarching problems retired; reserved confirmation seeds (`5001-5024`, `6001-6124`) unburned. |
| Pass 10 | Build Standard v1.2.0 End-to-End Replay | Verified `Audit-Repo.ps1`, `check_readme_axioms.py`, `examples/parity_smoke.py`, `CHECKSUMS.sha256`, and `data/MANIFEST.sha256`. | **Verified Clean:** All automated gates pass with zero unfilled template markers or placeholder tokens. |
