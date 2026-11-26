module

public import AsmodensiusProofs.Spec
public import AsmodensiusProofs.Core
public import AsmodensiusProofs.NonVacuity

/-!
# Asmodensius-Native Pure Kernel Lean 4.35.0 Formal Verification Library (`AsmodensiusProofs.lean`)

Aggregates all 30 machine-checked theorems across `AsmodensiusProofs.Spec`,
`AsmodensiusProofs.Core` (A01..A26), and `AsmodensiusProofs.NonVacuity` (A27..A30).

Zero Mathlib, zero `sorry`, zero `admit`, zero `native_decide`, `autoImplicit = false`,
`relaxedAutoImplicit = false`, 100% pure 7-bit ASCII + LF.

Standalone `#print axioms` auditing runs via `VerifyAxioms.lean` (`lake env lean VerifyAxioms.lean`),
and 6-kernel Palomar judging runs against isolated `Challenge.lean` via `comparator.json`.
-/
