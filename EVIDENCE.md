# Verification Evidence Index (`EVIDENCE.md`)

This ledger links every automated and machine-checked evidence artifact in `Asmodensius-Native` v1.1.0 to the exact command that generated it.

| Evidence Area | Result | Primary Artifact | Reproduction Command |
|---|---|---|---|
| Lean 4.35.0 Axiom Audit (30 Theorems) | 30/30 PASS (`[propext, Classical.choice, Quot.sound]`, 0 forbidden axioms) | `results/lean4-asmodensius-audit.txt` | `cd lean && lake env lean VerifyAxioms.lean` |
| Independent Multi-Kernel Replay | 5/5 Windows kernels PASS (`lean4`, `leanchecker`, `leanchecker-paranoid`, `con-leche`, `con-ron`, `nanoda_bin`); 6/6 wired in Linux CI (`lean4lean` + `bwrap` comparator) | `results/lean4-multi-kernel.txt`, `lean/comparator.json` | `cd lean && lake env leanchecker-paranoid --fresh Solution` |
| README Axiom Parity Guard | 30/30 verbatim match | `scripts/check_readme_axioms.py` | `python -I scripts/check_readme_axioms.py` |
| Full Native CTest Suite | 77/77 PASS (baseline commit `782cd0e`, untouched simulation sources) | `CMakeLists.txt`, `build/work/PROGRESS.md` | `ctest --test-dir build/z --output-on-failure` |
| GPU Differential & Control Suite | 164/164 PASS (1M sparse brains `hid=0 motor=0`, 1M kinematics `0` diffs, 1-word bias control `806/4096` diffs) | `gpu/test_gpu.c`, `gpu/az_gpu_dev.c` | `.\build\z\test_gpu.exe` |
| Multi-Substrate 1k Live Smoke Parity | 3/3 identical hash `7bd9823448dd38b1` (`--kernels 0`, `--kernels 1`, `--gpu 1`), exact matter `742819 -> 742819`, energy `9724070 -> 9724070`, `field_underflow 0`, `dropped_events 0` | `docs/run-log.md`, `REPLICATION.md` | `.\build\z\az_run.exe --lab --seed 42 --n 1000 --ticks 10 --quiet --kernels 1 --gpu 1` |
| Multi-Substrate 10k/100k/1M Benchmarks | 100k (`5b8110b324d2603f`): 24C `72.199 ms/tick` (+56.7%); 1M (`31e5d00c106ce679`, `9,260.6 MB` locked): 24C `757.988 ms/tick` (+35.3%) vs GPU `1,122.495 ms/tick` | `docs/benchmarks.md`, `docs/TRIGGER-REGISTER.md` | `.\build\z\az_run.exe --lab --seed 42 --n 100000 --ticks 32 --kernels 1 --ecores --prefetch 1` |
| Species Refusal Boundary (`DEMO-16`) | PASS (`--species 101` refused with exit code 2 before arena allocation) | `tools/az_run.c`, `core/az_worldgen.c` | `.\build\z\az_run.exe --lab --seed 42 --n 100 --species 101 --ticks 1 --quiet` |
| White Paper PDF & Clean-Room arXiv Tarball | PASS (0 `Overfull \hbox`, 0 undefined refs/citations) | `dist/Donahue_2026_asmodensius_native.pdf`, `dist/arxiv_submission.tar.gz` | `python -I scripts/build_paper.py` |
| Live Bibliography & Retraction Audit | 17/17 entries 1-to-1 cited, 13/13 DOIs + 2/2 manual URLs + 2/2 public URLs live-verified, 0 retractions | `paper/refs.bib`, `scripts/check_citations.py` | `python -I scripts/check_citations.py --live` |
| Law 14 Text Hygiene Gate | 0 violations (7-bit ASCII, LF, 0 tabs, trailing newline) | `build/work/scripts/check_text.py` | `python -I build/work/scripts/check_text.py . --since 240` |
| Build Standard v1.2.0 Audit | Audited via `Audit-Repo.ps1` | `repo.yaml`, `scripts/Audit-Repo.ps1` | `pwsh -NoProfile -File scripts/Audit-Repo.ps1` |
| Cryptographic Checksums | Verified SHA-256 digests | `CHECKSUMS.sha256`, `data/MANIFEST.sha256` | `Get-FileHash` / `sha256sum -c CHECKSUMS.sha256` |
