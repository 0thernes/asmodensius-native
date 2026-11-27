# Verification Overview (`docs/verification.md`)

`Asmodensius-Native` v1.2.0 enforces four independent verification layers so no single compiler, kernel, or test harness acts as a single point of failure.

## Layer 1: Native Binary, Sanitizer, and Determinism Gates (Windows)

- **77-Target CTest Suite (`CMakeLists.txt`):** Unit, property, integration, lesion, protocol, viewer transport, and CLI tests (`77/77` PASS on baseline commit `782cd0e`; C23/NASM/PTX sources untouched since baseline; targeted v1.2.0 spot checks recorded in `docs/run-log.md`).
- **Static & Binary Purity:** `lint_core` (+ planted-defect self-test), `core_obj_gate` (+ self-test proving 0 FP ops, 0 writable globals, 0 `___chkstk_ms`, only `memcpy`/`memset`/`memmove`/`memcmp` undefined), `avx_stack_gate` (+ self-test).
- **Runtime Undefined Behavior:** `ubsan_trap_build` builds a separate tree with `-fsanitize=undefined -fsanitize-trap=undefined` (no `-fwrapv`), confirms 6 planted UB faults in `tests/ubsan_probe.c` execute illegal instructions, and passes every test binary.
- **Multi-Substrate Determinism:** `cli_matrix_engine`, `cli_matrix_protocol`, `cli_matrix_protocol_cap`, and `cli_gpu_tick_parity` verify identical `az_state_hash`, ledger chains, and exact matter/energy conservation across worker counts (`1..24`), pinning policies, reorder periods (`7/31/255`), split seeds, scalar C vs AVX-VNNI/AVX2 twins, GPU offload, and mid-run snapshot resume.
- **GPU Differential Suite (`test_gpu.exe`):** `164/164` checks on RTX 5070 Ti across 1,000,000 sparse `AzBrain` steps (`hid=0`, `motor=0`), 1,000,000 kinematics lanes (`0` diffs), out-of-bounds clade guards, and bias-mutation negative control (`806/4096` diffs).

## Layer 2: Multi-Checker Lean 4.35.0 Formal Model Suite (`lean/`)

- **38 Pure Lean 4.35.0 Declarations (`A01`..`A38`):** Zero Mathlib, zero `sorry` in `Solution`, zero `native_decide`, `autoImplicit = false`, `relaxedAutoImplicit = false`, `linter.unusedVariables = true`.
- **Honest 26 vs 12 Separation:** `26` substantive model, boundary, non-vacuity witness, and adversarial false-variant refutation theorems (including `A31`/`A32`/`A38` modeling implemented `core/az_chem.c` `spread()` `a / 4` decreasing budget and restated `A07`..`A11`) plus `12` explicitly labeled definitional, constant, congruence, or legacy arithmetic sanity checks (`docs/statement-fidelity.md`). **C23, NASM, and CUDA PTX code is not verified in Lean.**
- **Palomar Isolation & Live Negative Tamper Gate:** Pure definitions in `AsmodensiusProofs/Spec.lean`, isolated statement-only `Challenge.lean` importing ONLY `Spec.lean`, sorry-free `Solution.lean`, `comparator.json`, and `scripts/test_lean_negative.py` proving false claims fail elaboration and injected `sorry`s are caught by `VerifyAxioms.lean` and rejected by `lake comparator --paranoid` in Ubuntu CI.
- **Multi-Checker Replay:** Checked locally on Windows by 6 checkers (`lean4` kernel, `leanchecker --fresh`, `leanchecker-paranoid --fresh`, `con-leche --verified`, `con-ron --verified`, `nanoda_bin` with `unpermitted_axiom_hard_error: true`, recorded in `results/lean4-multi-kernel.txt`). Ubuntu CI (`.github/workflows/ci.yml`) also runs `lake check --paranoid` (adding `lean4lean`) and `lake comparator --paranoid` under `bubblewrap`.
- **Verbatim README & Blueprint Guards:** `scripts/check_readme_axioms.py` and `scripts/check_blueprint.py` enforce 38/38 parity with `results/lean4-asmodensius-audit.txt` and `lean/comparator.json`.

## Layer 3: Publication, Citation & Provenance Integrity (`paper/`, `dist/`)

- `python3 -I scripts/build_paper.py` compiles `paper/main.tex` with Tectonic, fails on ANY `Overfull \hbox`, undefined reference/citation, or missing character, freezes `dist/Donahue_2026_asmodensius_native.pdf`, packages deterministic `dist/arxiv_submission.tar.gz` (`mtime=0`), and clean-room compiles the extracted tarball in a fresh temporary directory.
- `python3 -I scripts/check_citations.py --live --write-report` enforces 1-to-1 BibTeX <-> LaTeX coverage and resolves all 17 entries (13 DOIs with Crossref/arXiv `title=1.00`, `auth=OK`, 0 retractions; 2 vendor URLs HTTP 200; 2 book/ISO standards) into `results/citation-audit.json`.
- `sha256sum -c CHECKSUMS.sha256` and `python3 -I scripts/stamp_timestamps.py --verify` verify LF-canonical SHA-256 digests bound to DigiCert RFC 3161 (`CHECKSUMS.sha256.tsr`) and 3-calendar Bitcoin OpenTimestamps (`CHECKSUMS.sha256.ots`).

## Layer 4: Tracked Law-14 Text Hygiene & Build Standard v1.2.0 Audit

- Tracked `tools/check_text.py` (`6/6` planted-fault self-test) runs as a hard-failing gate in CI across Ubuntu and Windows, enforcing 0 violations (7-bit ASCII, LF, no tabs except `.tsv` and `scripts/audit-repo.sh`, trailing newline) across all active tracked files while transparently counting and reporting `2` exempt verbatim owner prompts (`docs/handoff/PROMPTS.*`) and historical archival dumps (`docs/requirements/`, `docs/research/`, `tests/upstream/`).
- `pwsh -NoProfile -File scripts/Audit-Repo.ps1` and `bash scripts/audit-repo.sh .` audit Build Standard v1.2.0 (`100.0%`, report-only in CI per R7).
