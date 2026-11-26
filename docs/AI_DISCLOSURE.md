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

No claim in this repository rests on AI-generated prose. Every reported result is verified directly by deterministic machines:

1. **Lean 4.35.0 Formal Proofs:** Machine-checked by 5 independent kernels locally (`lean4`, `leanchecker`, `leanchecker-paranoid`, `con-leche`, `con-ron`, `nanoda_bin`) and 6/6 kernels including `lean4lean` + `bubblewrap` comparator in Linux CI (`results/lean4-asmodensius-audit.txt`, `results/lean4-multi-kernel.txt`).
2. **Native C23 / NASM / CUDA PTX Engine:** Verified by `77/77` CTest targets (including `ubsan_trap_build`, `core_obj_gate`, `avx_stack_gate`, `lint_core`, determinism matrices) and `164/164` `test_gpu.exe`.
3. **White Paper & Bibliography:** Verified by `scripts/build_paper.py` (zero `Overfull \hbox`, clean-room arXiv tarball compile) and `scripts/check_citations.py --live` (live Crossref DOI, retraction, and HTTP verification).
4. **Repository Hygiene:** Verified by `build/work/scripts/check_text.py` (Law 14) and `scripts/Audit-Repo.ps1` (Build Standard v1.2.0).

## 4. Platform Compliance Statement

This disclosure conforms to transparent AI-assistance policies for independent preprints, arXiv endorsements, and Zenodo archival deposits. No peer review, journal acceptance, or external endorsement is claimed.
