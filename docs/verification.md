# Verification Overview (`docs/verification.md`)

`Asmodensius-Native` v1.1.0 enforces four independent verification layers so no single compiler, kernel, or test harness acts as a single point of failure.

## Layer 1: Native Binary, Sanitizer, and Determinism Gates (Windows)

- **77/77 CTest Targets (`CMakeLists.txt`):** Unit, property, integration, lesion, protocol, viewer transport, and CLI tests.
- **Static & Binary Purity:** `lint_core` (+ planted-defect self-test), `core_obj_gate` (+ self-test proving 0 FP ops, 0 writable globals, 0 `___chkstk_ms`, only `memcpy`/`memset`/`memmove`/`memcmp` undefined), `avx_stack_gate` (+ self-test).
- **Runtime Undefined Behavior:** `ubsan_trap_build` builds a separate tree with `-fsanitize=undefined -fsanitize-trap=undefined` (no `-fwrapv`), confirms 6 planted UB faults in `tests/ubsan_probe.c` execute illegal instructions, and passes every test binary.
- **Multi-Substrate Determinism:** `cli_matrix_engine`, `cli_matrix_protocol`, `cli_matrix_protocol_cap`, and `cli_gpu_tick_parity` verify identical `az_state_hash`, ledger chains, and exact matter/energy conservation across worker counts (`1..24`), pinning policies, reorder periods (`7/31/255`), split seeds, scalar C vs AVX-VNNI/AVX2 twins, GPU offload, and mid-run snapshot resume.
- **GPU Differential Suite (`test_gpu.exe`):** `164/164` checks on RTX 5070 Ti across 1,000,000 sparse `AzBrain` steps (`hid=0`, `motor=0`), 1,000,000 kinematics lanes (`0` diffs), out-of-bounds clade guards, and bias-mutation negative control (`806/4096` diffs).

## Layer 2: 6-Kernel Lean 4.35.0 Formal Verification (`lean/`)

- **30 Pure Lean 4.35.0 Theorems (`A01`..`A30`):** Zero Mathlib, zero `sorry` in `Solution`, zero `native_decide`, `autoImplicit = false`, `relaxedAutoImplicit = false`, `linter.unusedVariables = true`.
- **Palomar Isolation:** Pure definitions in `AsmodensiusProofs/Spec.lean`, isolated statement-only `Challenge.lean` importing ONLY `Spec.lean`, sorry-free `Solution.lean`, and `comparator.json`.
- **Multi-Kernel Replay:** Checked locally on Windows by 6 checkers (`lean4` kernel, `leanchecker --fresh`, `leanchecker-paranoid --fresh`, `con-leche --verified`, `con-ron --verified`, `nanoda_bin` with `unpermitted_axiom_hard_error: true`, recorded in `results/lean4-multi-kernel.txt`). Ubuntu CI (`.github/workflows/ci.yml`) also runs `lake check --paranoid`, which adds `lean4lean`, and `lake comparator` under `bubblewrap`.
- **Scope:** the theorems concern the `Nat` definitions in `Spec.lean`, not the C23, NASM or PTX code. 13 of the 30 are statements about those definitions; 17 are definitional, constant or literal-arithmetic checks (see `statement-fidelity.md`).
- **Verbatim README Guard:** `python -I scripts/check_readme_axioms.py` checks 30/30 `#print axioms` lines in `README.md` against `results/lean4-asmodensius-audit.txt`.

## Layer 3: Publication & Citation Integrity (`paper/`, `dist/`)

- `python -I scripts/build_paper.py` compiles `paper/main.tex` with Tectonic, fails on ANY `Overfull \hbox`, undefined reference/citation, or missing character, freezes `dist/Donahue_2026_asmodensius_native.pdf`, packages deterministic `dist/arxiv_submission.tar.gz` (`mtime=0`), and clean-room compiles the extracted tarball in a fresh temporary directory.
- `python -I scripts/check_citations.py --live` enforces 1-to-1 BibTeX <-> LaTeX coverage and resolves all 17 entries (13 Crossref/arXiv DOIs + 2 vendor URLs + 2 book/ISO standards) live with zero retractions.

## Layer 4: Law 14 Text Hygiene & Build Standard v1.2.0 Audit

- The Law 14 text checker (`build/work/scripts/check_text.py`: 7-bit ASCII, LF line endings, no tabs, trailing newline) is not tracked in this repository and is not run in CI, so no violation count can be reproduced from the repository.
- `pwsh -NoProfile -File scripts/Audit-Repo.ps1` audits the Build Standard v1.2.0 items. In CI this step is report-only (`continue-on-error: true`).
