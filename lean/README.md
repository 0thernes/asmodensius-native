# Asmodensius-Native Lean 4.35.0 Formal Verification Suite

This directory contains the 6-kernel Palomar-ready Pure Lean 4.35.0 formal
proof suite for `Asmodensius-Native` (30 machine-checked theorems `A01`..`A30`,
zero Mathlib dependencies, zero `sorry` in `Solution`, zero `native_decide`,
`autoImplicit = false`, `relaxedAutoImplicit = false`, pure 7-bit ASCII + LF).

## Module Architecture

- `lean-toolchain`: pins `leanprover/lean4:v4.35.0`.
- `lakefile.toml` & `lake-manifest.json`: strict Lake configuration with zero external packages.
- `AsmodensiusProofs/Spec.lean`: pure shared ABI v4 definitions only (zero proofs, zero `sorry`).
- `AsmodensiusProofs/Core.lean`: theorems `A01`..`A26` covering Law 5 conservation, Rule C2/C5/C7 bounds, `WG_MAX_SPECIES = 100` refusal, arena rollback, store block release, D7 non-interference, TR-13 bulk Philox key equivalence, TR-18/TR-04 GPU/CPU twin parity, TR-01 prefetch transparency, ABI v4 lesion mask & brain SIMD alignment, and 32 GiB VRAM offset injectivity.
- `AsmodensiusProofs/NonVacuity.lean`: theorems `A27`..`A30` providing explicit concrete non-vacuity inhabitants and adversarial false-variant refutations proving guard necessity.
- `Challenge.lean`: isolated statement-only specification importing ONLY `AsmodensiusProofs.Spec` (`sorry` proofs matched against `Solution.lean`).
- `Solution.lean`: sorry-free entrypoint exporting all 30 theorems.
- `VerifyAxioms.lean`: standalone `#print axioms` audit script captured verbatim in `results/lean4-asmodensius-audit.txt`.
- `comparator.json` & `formalization.yaml`: 6-kernel comparator contract and metadata.

## Local Replay (Windows PowerShell)

```powershell
$env:PATH = "$env:USERPROFILE\.elan\bin;$env:PATH"
Set-Location -LiteralPath 'lean'
lake build
lake build Challenge
lake env lean VerifyAxioms.lean
lake env leanchecker --fresh Solution
lake env leanchecker-paranoid --fresh Solution
```

## Full 6-Kernel Linux CI Replay (with bubblewrap)

```bash
cd lean
lake build && lake build Challenge
lake env lean VerifyAxioms.lean
lake check --paranoid
lake comparator --config comparator.json --paranoid
```
