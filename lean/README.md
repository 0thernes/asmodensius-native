# Asmodensius-Native Lean 4.35.0 Formal Verification Suite

This directory contains the 6-kernel Palomar-ready Pure Lean 4.35.0 formal
proof suite for `Asmodensius-Native` v1.2.0 (38 machine-checked declarations
`A01`..`A38`: 26 substantive model/boundary/witness/refutation theorems plus
12 explicitly labeled definitional, constant, congruence, or legacy arithmetic
sanity checks; zero Mathlib dependencies, zero `sorry` in `Solution`, zero
`native_decide`, `autoImplicit = false`, `relaxedAutoImplicit = false`,
pure 7-bit ASCII + LF).

**Scope boundary:** Every theorem concerns the hand-written `Nat` model in
`AsmodensiusProofs/Spec.lean`. Lean does NOT formally verify C23 compilation,
NASM assembly, or CUDA PTX codegen (those are verified empirically via `77/77`
CTest and `164/164` `test_gpu.exe`).

## Module Architecture

- `lean-toolchain`: pins `leanprover/lean4:v4.35.0`.
- `lakefile.toml` & `lake-manifest.json`: strict Lake configuration with zero external packages.
- `AsmodensiusProofs/Spec.lean`: pure shared ABI v4 definitions only (zero proofs, zero `sorry`), including both documented Rule C7 (`cellFaceFluxBound = amt / 8`) and the implemented `core/az_chem.c` `spread()` decreasing-budget pass (`chemSpreadBudget = a / 4`, `chemSpreadStep`, `chemSpreadSixFaces`).
- `AsmodensiusProofs/Core.lean`: 28 declarations (`A01`..`A26`, `A31`..`A32`):
  - 16 substantive model theorems (`A01`..`A03`, `A07`..`A11`, `A17`, `A21`..`A24`, `A26`, `A31`..`A32`) genuinely invoking `Spec.lean` functions (`acceptSpecies`, `arenaRollback`, `storeRelease`, `philoxBulkKey`, `receiveCap`, `cellFaceFluxBound`, `chemSpreadSixFaces`, `metabolizeNutXToBio`, `validLesionMask`, `tileBrainByteOffset`).
  - 12 explicitly labeled definitional, constant, congruence, or legacy arithmetic sanity checks (`A04`..`A06`, `A12_sanity_*`, `A13`..`A14`, `A15_sanity_*`, `A16_sanity_*`, `A18_sanity_*`, `A19_sanity_*`, `A20_sanity_*`, `A25`).
- `AsmodensiusProofs/NonVacuity.lean`: 4 theorems (`A27`..`A30`) providing explicit concrete non-vacuity inhabitants and guard-necessity refutations.
- `AsmodensiusProofs/AdversarialTests.lean`: 6 concrete false-variant refutations (`A33`..`A38`) proving unguarded mass overdraw, out-of-range species refusal, unrolled-back arena mutation, `AZ_CAP_MAX` overflow rejection, unaligned brain dimension rejection, and quarter-budget spread saturation bounds.
- `Challenge.lean`: isolated statement-only specification importing ONLY `AsmodensiusProofs.Spec` (`38` intentional challenge `sorry`s matched against `Solution.lean`).
- `Solution.lean`: sorry-free entrypoint exporting all 38 declarations.
- `VerifyAxioms.lean`: standalone `#print axioms` audit script captured verbatim in `results/lean4-asmodensius-audit.txt`.
- `comparator.json` & `formalization.yaml`: Palomar comparator contract and metadata.

## Local Replay (Windows PowerShell - 6 Local Checkers)

```powershell
$env:PATH = "$env:USERPROFILE\.elan\bin;$env:PATH"
Set-Location -LiteralPath 'lean'
lake clean
lake build
lake build Challenge
lake env lean VerifyAxioms.lean
lake env leanchecker --fresh Solution
lake env leanchecker-paranoid --fresh Solution
Set-Location -LiteralPath '..'
python -I scripts/test_lean_negative.py
```

## Full Ubuntu CI Replay (Adding `lean4lean` + `bubblewrap` Comparator)

```bash
cd lean
lake build && lake build Challenge
lake env lean VerifyAxioms.lean
lake check --paranoid
lake comparator --config comparator.json --paranoid
cd ..
python3 -I scripts/test_lean_negative.py --with-comparator
```
