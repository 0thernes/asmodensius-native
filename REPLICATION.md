# Replication Protocol (`REPLICATION.md`)

This document provides clean-clone instructions to independently reproduce every computational, formal, and bibliographic result reported in `Asmodensius-Native` v1.1.0.

## 1. Prerequisites

- **Formal Verification (Windows or Linux):** `elan` with pinned `leanprover/lean4:v4.35.0` (read automatically from `lean/lean-toolchain`); on Linux, `bubblewrap` (`bwrap`) enables full 6-kernel `lake check --paranoid` and `lake comparator`.
- **White Paper & Audits (Windows or Linux):** Python 3.11+ (`python -I`), Tectonic (`tectonic` CLI).
- **Native Simulation Engine (Windows x86-64 only, Law 10):** CMake >= 3.20, Ninja, MinGW-w64 GCC (C23) or Clang 21, NASM, and (optional for `--gpu 1` / `test_gpu`) an NVIDIA GPU with CUDA driver API >= 12.0 and Clang 21 (`--target=nvptx64-nvidia-cuda`).

## 2. Clean-Clone Step-by-Step Verification

### Step A: Clone and Check Repository Hygiene

```bash
git clone https://github.com/0thernes/asmodensius-native.git
cd asmodensius-native
python -I build/work/scripts/check_text.py .
```

### Step B: Replay Lean 4.35.0 Multi-Kernel Proofs (30 Theorems)

On **Linux** (all 6 kernels + bubblewrap comparator):

```bash
cd lean
lake build
lake build Challenge
lake env lean VerifyAxioms.lean > /tmp/lean4-asmodensius-audit.txt
diff -u ../results/lean4-asmodensius-audit.txt /tmp/lean4-asmodensius-audit.txt
lake check --paranoid
lake comparator --config comparator.json --paranoid
cd ..
python -I scripts/check_readme_axioms.py
```

On **Windows PowerShell** (5 Windows-capable kernels: `lean4`, `leanchecker`, `leanchecker-paranoid`, `con-leche`, `con-ron`, `nanoda_bin`):

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
```

### Step C: Recompile White Paper, Clean-Room arXiv Tarball, and Live Citations

```bash
python -I scripts/build_paper.py
python -I scripts/check_citations.py --live
```

Expected: 0 `Overfull \hbox`, 0 undefined refs/citations, clean-room compilation of `dist/arxiv_submission.tar.gz`, and 17/17 bibliography entries (13 DOIs + 2 vendor URLs + 2 book/ISO standards) passing with 0 retractions.

### Step D: Build Native Engine and Run 77/77 CTest + Multi-Substrate Parity (Windows)

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

### Step E: Build Standard v1.2.0 Audit & SHA-256 Digest Verification

```powershell
pwsh -NoProfile -File scripts/Audit-Repo.ps1
```

## 3. Replication Log

| Date | Replicator | Environment | Scope Verified | Result | Notes |
|---|---|---|---|---|---|
| 2026-11-25 | Alexander Stephen Donahue (author self-replay) | Windows 11 x86-64, Core Ultra 9 275HX (8P+16E), RTX 5070 Ti (12,199 MiB), Lean 4.35.0, Tectonic | 30/30 Lean 4.35.0 theorems across 5 Windows kernels (`lean4`, `leanchecker`, `leanchecker-paranoid`, `con-leche`, `con-ron`, `nanoda_bin`), `164/164` `test_gpu.exe`, 1k 3-substrate parity (`7bd9823448dd38b1`), `--species 101` exit 2, Tectonic zero-overflow PDF + clean-room tarball, 17/17 verified citations (13 DOIs, 0 retractions) | PASS | Author self-replication; independent external third-party replication is invited and not yet recorded. |
| 2026-11-22 | Alexander Stephen Donahue (author baseline) | Windows 11 x86-64, Core Ultra 9 275HX, RTX 5070 Ti, MinGW GCC C23 + Clang 21 nvptx64 + NASM | Full `77/77` CTest suite (including `ubsan_trap_build`, `core_obj_gate`, `avx_stack_gate`, `cli_matrix_engine`, `cli_matrix_protocol`, `cli_gpu_tick_parity`, `test_gpu`), 10k/100k/1M multi-substrate benchmarks | PASS | Recorded in `docs/TRIGGER-REGISTER.md` and `build/work/PROGRESS.md`. |
