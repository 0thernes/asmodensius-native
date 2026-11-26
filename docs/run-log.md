# Verification Run Log (`docs/run-log.md`)

Chronological audit trail of verified builds, formal checks, benchmarks, and releases (Build Standard `B40`).

| UTC Date | Version / Commit | Scope | Command | Measured Outcome |
|---|---|---|---|---|
| 2026-11-22 | v1.0.0 (`f6f5bc8`) | Core/worldgen repairs (`P4-CORE-02`, `P4-CORE-03`, `DEMO-16`), dual visual worlds, 16 Lean 4 proofs | `ctest --test-dir build/z --output-on-failure` | PASS: `76/76` CTest, `16/16` Lean 4 theorems, `0` `check_text.py` violations |
| 2026-11-22 | v1.0.0 (`4820dab`, `dd13a6e`) | 1..1M population controls & smart `auto_cap` across both visual worlds | `.\build\z\az_run.exe --n 1000000 --cap 1004096 --ticks 1` | PASS: `9,260.6 MB` locked (`locked=1`), `1,004,032` live, exact conservation, `7/7` viewer/ws CTest |
| 2026-11-22 | v1.0.0 (`782cd0e`) | Bare-metal compute `TR-01`, `TR-04`, `TR-07`, `TR-13`, `TR-18` (24C P+E, bulk Philox AVX2, DDR5 PF, 41,817 B CUDA PTX on RTX 5070 Ti) | `ctest --test-dir build/z --output-on-failure` + `test_gpu.exe` + 10k/100k/1M benchmarks | PASS: `77/77` CTest, `164/164` `test_gpu`, 100k `72.199 ms/tick` (+56.7%, `5b8110b324d2603f`), 1M `757.988 ms/tick` (+35.3%, `31e5d00c106ce679`), GPU 1M `1,122.495 ms/tick` (0 fallbacks) |
| 2026-11-25 | v1.1.0 (pre-commit) | Live native binary parity smoke & refusal verification | `.\build\z\test_gpu.exe` + `python -I examples/parity_smoke.py` + `az_run --n 500 --ticks 5` | PASS: `164/164` `test_gpu`, 3-substrate 1k parity hash `7bd9823448dd38b1` (`742819->742819` matter, `9724070->9724070` energy), wild mode fresh entropy `5/5`, `--species 101` exit `2` |
| 2026-11-26 | v1.1.0 (pre-commit) | 6-Kernel Palomar Lean 4.35.0 upgrade (`A01`..`A30`) | `lake build`, `lake build Challenge`, `VerifyAxioms.lean`, `leanchecker`, `leanchecker-paranoid`, `con-leche`, `con-ron`, `nanoda_bin` | PASS: `30/30` theorems within `[propext, Classical.choice, Quot.sound]`, `0` warnings, `con-leche` 1778 decls, `con-ron` 1778 decls, `nanoda_bin` 1987 decls |
| 2026-11-26 | v1.1.0 (release) | AMS-LaTeX white paper, live citations, Law 14 text gate & Build Standard v1.2.0 audit | `build_paper.py`, `check_citations.py --live`, `check_readme_axioms.py`, `check_text.py`, `Audit-Repo.ps1` | Verified clean (see `EVIDENCE.md` and `CHECKSUMS.sha256`) |
