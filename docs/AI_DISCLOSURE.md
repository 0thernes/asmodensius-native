# AI-Assistance Disclosure and CRediT Authorship (`docs/AI_DISCLOSURE.md`)

## 1. Human Authorship and CRediT Taxonomy

- **Sole Human Author:** Alexander Stephen Donahue
- **Affiliation:** Independent Researcher, Spokane, WA, USA
- **ORCID:** `0009-0006-8168-9648` (`https://orcid.org/0009-0006-8168-9648`)
- **Contact:** `0_0@0thernes.art`
- **CRediT Roles:** Conceptualization, Methodology, Software Architecture, Supervision, Validation, Formal Verification Review, Writing -- Original Draft, Writing -- Review & Editing.

## 2. Transparent AI-Assistance Scope

AI coding assistants (including Codex CLI, Claude Code, and Gemini-based agents operating under `AGENTS.md`) were used as engineering and drafting assistants during repository construction, v1.1.0 hardening, the 2026-11-26 statement audit, and v1.2.0 closure:

- **Assisted areas:** C23/NASM/CUDA PTX implementation iterations, test-harness authoring, Lean 4 tactic drafting and refactoring, AMS-LaTeX typesetting, interactive blueprint/FAIR metadata generation, and Build Standard v1.2.0 documentation scaffolding.
- **Not inside the simulation engine (Law 3):** Zero LLMs, tokenizers, prompts, pretrained weights, external APIs, or datasets exist anywhere inside `Asmodensius-Native`'s simulation engine (`core/`, `kernels/`, `gpu/`, `platform/`, `tools/`, `view/`).

## 3. Independent Verification Guarantee (Zero Trust in LLM Prose)

Reported results are checked by the tools below. A tool check covers what the tool tests and no more; the prose around it can still overstate, which is why every finding from the 2026-11-26 statement audit is preserved in [ERRATA.md](../ERRATA.md) and scoped in [LIMITATIONS.md](../LIMITATIONS.md).

1. **Lean 4.35.0 Model Proofs (`38` declarations: `26` substantive + `12` labeled sanity checks):** Machine-checked locally on Windows by 6 checkers (`lean4` kernel, `leanchecker`, `leanchecker-paranoid`, `con-leche`, `con-ron`, `nanoda_bin`); Ubuntu CI also runs `lake check --paranoid` (adding `lean4lean`), `lake comparator --paranoid` under `bubblewrap`, and live negative elaboration + `sorry`-tamper rejection (`scripts/test_lean_negative.py --with-comparator`). The declarations concern an abstract `Nat` model in `Spec.lean`, not the C23, NASM, or PTX code.
2. **Native C23 / NASM / CUDA PTX Engine:** Verified by CTest (including `ubsan_trap_build`, `core_obj_gate`, `avx_stack_gate`, `lint_core`, determinism matrices) and `164/164` `test_gpu.exe`.
3. **White Paper, Blueprint & Bibliography:** Verified by `scripts/build_paper.py` (zero `Overfull \hbox`, clean-room arXiv tarball compile), `scripts/check_blueprint.py` (strict CSP, zero CDN, 38/38 targets), and `scripts/check_citations.py --live --write-report` (`results/citation-audit.json`, 13/13 DOIs `title=1.00`, `auth=OK`, 0 retractions).
4. **Repository Hygiene & Provenance:** Tracked `tools/check_text.py` (hard-failing in CI across active tracked files; archival dumps reported transparently), `scripts/stamp_timestamps.py --verify` (DigiCert RFC 3161 + 3-calendar Bitcoin OTS), and `scripts/Audit-Repo.ps1` (Build Standard v1.2.0, report-only in CI).

## 4. Platform Compliance Statement

This disclosure conforms to transparent AI-assistance policies for independent preprints, arXiv endorsements, and Zenodo archival deposits. No peer review, journal acceptance, or external endorsement is claimed.
