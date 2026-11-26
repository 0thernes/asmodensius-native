# AI-Assistance Disclosure and CRediT Authorship (`docs/AI_DISCLOSURE.md`)

## 1. Human Authorship and CRediT Taxonomy

- **Sole Human Author:** Alexander Stephen Donahue
- **Affiliation:** Independent Researcher, Spokane, WA, USA
- **ORCID:** `0009-0006-8168-9648` (`https://orcid.org/0009-0006-8168-9648`)
- **Contact:** `0_0@0thernes.art`
- **CRediT Roles:** Conceptualization, Methodology, Software Architecture, Supervision, Validation, Formal Verification Review, Writing -- Original Draft, Writing -- Review & Editing.

## 2. Transparent AI-Assistance Scope

AI coding assistants (including Codex CLI, Claude Code, and Gemini-based agents operating under `AGENTS.md`) were used as engineering and drafting assistants during repository construction and v1.1.0 hardening:

- **Assisted areas:** C23/NASM/CUDA PTX implementation iterations, test-harness authoring, Lean 4 tactic drafting and refactoring to Lean 4.35.0 Palomar `module` headers, AMS-LaTeX typesetting, and Build Standard v1.2.0 documentation scaffolding.
- **Not inside the simulation engine (Law 3):** Zero LLMs, tokenizers, prompts, pretrained weights, external APIs, or datasets exist anywhere inside `Asmodensius-Native`'s simulation engine (`core/`, `kernels/`, `gpu/`, `platform/`, `tools/`, `view/`).

## 3. Independent Verification Guarantee (Zero Trust in LLM Prose)

Reported results are checked by the tools below. A tool check covers what the tool tests and no more; the prose around it, including theorem names, can still overstate. The scope of each check is stated in [LIMITATIONS.md](../LIMITATIONS.md), and corrections are logged in [ERRATA.md](../ERRATA.md).

1. **Lean 4.35.0 Formal Proofs:** Machine-checked by 6 checkers locally on Windows (`lean4` kernel, `leanchecker`, `leanchecker-paranoid`, `con-leche`, `con-ron`, `nanoda_bin`); Ubuntu CI also runs `lake check --paranoid`, which adds `lean4lean`, and `lake comparator` under `bubblewrap` (`results/lean4-asmodensius-audit.txt`, `results/lean4-multi-kernel.txt`). The theorems concern an abstract `Nat` model, not the C23, NASM or PTX code.
2. **Native C23 / NASM / CUDA PTX Engine:** Verified by `77/77` CTest targets (including `ubsan_trap_build`, `core_obj_gate`, `avx_stack_gate`, `lint_core`, determinism matrices) and `164/164` `test_gpu.exe`.
3. **White Paper & Bibliography:** Verified by `scripts/build_paper.py` (zero `Overfull \hbox`, clean-room arXiv tarball compile) and `scripts/check_citations.py --live` (live Crossref DOI, retraction, and HTTP verification).
4. **Repository Hygiene:** `scripts/Audit-Repo.ps1` (Build Standard v1.2.0, report-only in CI). The Law 14 text checker (`build/work/scripts/check_text.py`) is not tracked in this repository and is not run in CI.

## 4. Platform Compliance Statement

This disclosure conforms to transparent AI-assistance policies for independent preprints, arXiv endorsements, and Zenodo archival deposits. No peer review, journal acceptance, or external endorsement is claimed.
