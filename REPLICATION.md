# Replication Protocol (`REPLICATION.md`)

This document provides clean-clone instructions to independently reproduce every computational, formal, and bibliographic result reported in `Asmodensius-Native` v1.2.0.

## 1. Prerequisites

- **Formal Verification (Windows or Linux):** `elan` with pinned `leanprover/lean4:v4.35.0` (read automatically from `lean/lean-toolchain`); on Linux, `bubblewrap` (`bwrap`) enables `lake check --paranoid` (adding `lean4lean`) and `lake comparator --config comparator.json --paranoid`.
- **White Paper, Hygiene & Audits (Windows or Linux):** Python 3.11+ (`python3 -I`), Tectonic (`tectonic` CLI), OpenSSL (for minting/inspecting RFC 3161 `.tsr` tokens).
- **Native Simulation Engine (Windows x86-64 only, Law 10):** CMake >= 3.20, Ninja, MinGW-w64 GCC (C23) or Clang 21, NASM, and (optional for `--gpu 1` / `test_gpu`) an NVIDIA GPU with CUDA driver API >= 12.0 and Clang 21 (`--target=nvptx64-nvidia-cuda`).

## 2. Clean-Clone Step-by-Step Verification

### Step A: Clone, Law-14 Text Hygiene, and Cryptographic Provenance

```bash
git clone https://github.com/0thernes/asmodensius-native.git
cd asmodensius-native
python3 -I tools/check_text.py --self-test
python3 -I tools/check_text.py
sha256sum -c CHECKSUMS.sha256
sha256sum -c data/MANIFEST.sha256
python3 -I scripts/stamp_timestamps.py --verify
```

### Step B: Replay Lean 4.35.0 Model Proofs (38 Declarations + Live Negative Tamper)

On **Linux** (adding `lean4lean` via `lake check --paranoid` + `bubblewrap` comparator):

```bash
cd lean
lake build
lake build Challenge
lake env lean VerifyAxioms.lean > /tmp/lean4-asmodensius-audit.txt
diff -u ../results/lean4-asmodensius-audit.txt /tmp/lean4-asmodensius-audit.txt
lake env leanchecker --fresh Solution
lake env leanchecker-paranoid --fresh Solution
lake check --paranoid
lake comparator --config comparator.json --paranoid
cd ..
python3 -I scripts/check_readme_axioms.py
python3 -I scripts/check_blueprint.py
python3 -I scripts/test_lean_negative.py --with-comparator
```

On **Windows PowerShell** (6 local checkers: `lean4` kernel, `leanchecker`, `leanchecker-paranoid`, `con-leche`, `con-ron`, `nanoda_bin`):

```powershell
$env:PATH = "$env:USERPROFILE\.elan\bin;$env:PATH"
Set-Location -LiteralPath 'lean'
lake build
lake build Challenge
lake env lean VerifyAxioms.lean
lake env leanchecker --fresh Solution
lake env leanchecker-paranoid --fresh Solution
Set-Location -LiteralPath '..'
python -I scripts/check_readme_axioms.py
python -I scripts/check_blueprint.py
python -I scripts/test_lean_negative.py
```

### Step C: Recompile White Paper, Clean-Room arXiv Tarball, and Live Citations

```bash
python3 -I scripts/build_paper.py
python3 -I scripts/check_citations.py --live
```

Expected: 10 pages, 0 `Overfull \hbox`, 0 undefined refs/citations, clean-room compilation of `dist/arxiv_submission.tar.gz`, and 17/17 bibliography entries (13 DOIs with Crossref/arXiv `title=1.00` + `auth=OK` + 0 retractions, 2 vendor URLs HTTP 200, 2 book/ISO standards) passing.

### Step D: Build Native Engine and Run CTest + Multi-Substrate Parity (Windows)

```powershell
$root = (Get-Location).Path
$env:TMP = Join-Path $root 'build\tmp'
$env:TEMP = $env:TMP
[void][IO.Directory]::CreateDirectory($env:TMP)

cmake -S $root -B (Join-Path $root 'build\z') -G Ninja
cmake --build (Join-Path $root 'build\z')
ctest --test-dir (Join-Path $root 'build\z') --output-on-failure

# Multi-substrate 1k bit-exact parity smoke (expected hash: 7bd9823448dd38b1)
.\build\z\az_run.exe --lab --seed 42 --n 1000 --ticks 10 --quiet --kernels 0
.\build\z\az_run.exe --lab --seed 42 --n 1000 --ticks 10 --quiet --kernels 1
.\build\z\az_run.exe --lab --seed 42 --n 1000 --ticks 10 --quiet --kernels 1 --gpu 1

# Explicit species cap refusal check (expected exit code: 2)
.\build\z\az_run.exe --lab --seed 42 --n 100 --species 101 --ticks 1 --quiet
```

### Step E: Build Standard v1.2.0 Audit

```powershell
pwsh -NoProfile -File scripts/Audit-Repo.ps1
```

## 3. Replication Log

| Date | Replicator | Environment | Scope Verified | Result | Notes |
|---|---|---|---|---|---|
| 2026-11-26 | Alexander Stephen Donahue (author v1.2.0 self-replay) | Windows 11 x86-64, Core Ultra 9 275HX (8P+16E), RTX 5070 Ti (12,199 MiB), Lean 4.35.0, Tectonic | 38/38 Lean 4.35.0 declarations across 6 local Windows checkers (`lean4`, `leanchecker`, `leanchecker-paranoid`, `con-leche` 1,816 decls, `con-ron` 1,816 decls, `nanoda_bin`), `test_lean_negative.py`, `tools/check_text.py` (6/6 self-test, 0 active violations), `check_blueprint.py` (38/38), Tectonic 10-page zero-overflow PDF + clean-room tarball, 17/17 citations (13/13 DOIs `title=1.00`, `auth=OK`, 0 retractions), RFC 3161 + Bitcoin OTS timestamps, targeted native CTest & GPU checks | PASS | Author self-replication; Ubuntu CI independently verifies `lean4lean`, `lake comparator --paranoid`, and `test_lean_negative.py --with-comparator`. External third-party replication is invited and not yet recorded. |
| 2026-11-25 | Alexander Stephen Donahue (author v1.1.0 self-replay) | Windows 11 x86-64, Core Ultra 9 275HX (8P+16E), RTX 5070 Ti (12,199 MiB), Lean 4.35.0, Tectonic | 30/30 Lean 4.35.0 declarations across 6 local Windows checkers, `164/164` `test_gpu.exe`, 1k 3-substrate parity (`7bd9823448dd38b1`), `--species 101` exit 2, Tectonic zero-overflow PDF + clean-room tarball, 17/17 citations | PASS | Author self-replication (Ubuntu CI run `45748401683` also passed `lake check --paranoid` and `lake comparator`). |
| 2026-11-22 | Alexander Stephen Donahue (author baseline) | Windows 11 x86-64, Core Ultra 9 275HX, RTX 5070 Ti, MinGW GCC C23 + Clang 21 nvptx64 + NASM | Full `77/77` CTest suite (including `ubsan_trap_build`, `core_obj_gate`, `avx_stack_gate`, `cli_matrix_engine`, `cli_matrix_protocol`, `cli_gpu_tick_parity`, `test_gpu`), 10k/100k/1M multi-substrate benchmarks | PASS | Recorded in `docs/TRIGGER-REGISTER.md` and `docs/run-log.md`. |
