# Contributing to Asmodensius-Native (`CONTRIBUTING.md`)

Thank you for your interest in `Asmodensius-Native`. This repository follows Build Standard v1.2.0 and the 15 project laws defined in [AGENTS.md](AGENTS.md).

## Mandatory Engineering Laws

1. **Read `AGENTS.md` first:** Every change must preserve all 15 Laws, including freestanding C23 purity in `core/`, exact integer matter/energy conservation (`==`, zero underflow, zero dropped events), wild-by-default entropy with `--lab` deterministic parity, and Law 8 indicator-only reporting.
2. **Text format (Law 14):** Every edited text file must be pure 7-bit ASCII, LF (`\n`) line endings, zero tabs (except `.tsv` tables), and end with a trailing newline. Verify with:
   ```powershell
   python -I build/work/scripts/check_text.py . --since 240
   ```
3. **Never delete owner data:** Do not remove existing evaluation traces (`docs/eval/TRACE.csv`) or historical logs.
4. **No fabrication (Build Standard Rule R6):** Every claimed test result, benchmark, or Lean proof check must be backed by an actual run of the current code.
5. **Direct workflow (Rules R1-R2):** Maintainers commit directly to `main`; external contributors should open a GitHub Issue describing the bug, feature, or independent replication log first.

## Required Local Checks Before Reporting or Committing

- Native C23/NASM/PTX (Windows): `ctest --test-dir build/z --output-on-failure` (`77/77` pass) and `.\build\z\test_gpu.exe` (`164/164` pass).
- Lean 4.35.0 proofs: `cd lean && lake build && lake build Challenge && lake env lean VerifyAxioms.lean` + `python -I scripts/check_readme_axioms.py`.
- White paper & citations: `python -I scripts/build_paper.py` and `python -I scripts/check_citations.py --live`.
- Build Standard audit: `pwsh -NoProfile -File scripts/Audit-Repo.ps1`.

## Licensing of Contributions

By submitting contributions to `Asmodensius-Native`, you agree that code and Lean proofs are licensed under Apache-2.0 (`LICENSE`) and documentation/paper/dataset contributions are licensed under CC-BY-4.0 (`LICENSE-DOCS`).
