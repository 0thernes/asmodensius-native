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
- **Multi-Kernel Replay:** Verified locally on Windows across 5 independent kernels (`lean4`, `leanchecker --fresh`, `leanchecker-paranoid --fresh`, `con-leche --verified`, `con-ron --verified`, `nanoda_bin` with `unpermitted_axiom_hard_error: true`, recorded in `results/lean4-multi-kernel.txt`); all 6/6 kernels including `lean4lean` + `bubblewrap` comparator wired in `.github/workflows/ci.yml`.
- **Verbatim README Guard:** `python -I scripts/check_readme_axioms.py` checks 30/30 `#print axioms` lines in `README.md` against `results/lean4-asmodensius-audit.txt`.

## Layer 3: Publication & Citation Integrity (`paper/`, `dist/`)

- `python -I scripts/build_paper.py` compiles `paper/main.tex` with Tectonic, fails on ANY `Overfull \hbox`, undefined reference/citation, or missing character, freezes `dist/Donahue_2026_asmodensius_native.pdf`, packages deterministic `dist/arxiv_submission.tar.gz` (`mtime=0`), and clean-room compiles the extracted tarball in a fresh temporary directory.
- `python -I scripts/check_citations.py --live` enforces 1-to-1 BibTeX <-> LaTeX coverage and resolves all 17 entries (13 Crossref/arXiv DOIs + 2 vendor URLs + 2 book/ISO standards) live with zero retractions.

## Layer 4: Law 14 Text Hygiene & Build Standard v1.2.0 Audit

- `python -I build/work/scripts/check_text.py .` enforces pure 7-bit ASCII, LF line endings, zero tabs, and trailing newline (`0` violations).
- `pwsh -NoProfile -File scripts/Audit-Repo.ps1` audits all 90 Build Standard v1.2.0 items (`A01`..`A45`, `B01`..`B45`).
