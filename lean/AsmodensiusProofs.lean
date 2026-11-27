module

public import AsmodensiusProofs.Spec
public import AsmodensiusProofs.Core
public import AsmodensiusProofs.NonVacuity
public import AsmodensiusProofs.AdversarialTests

/-!
# Asmodensius-Native Pure Kernel Lean 4.35.0 Formal Verification Library (`AsmodensiusProofs.lean`)

Aggregates all 38 machine-checked theorems (`A01`..`A38`) across:
  - `AsmodensiusProofs.Spec` (pure shared ABI v4 and model definitions; zero proofs)
  - `AsmodensiusProofs.Core` (28 theorems: `A01`..`A26`, `A31`..`A32`)
  - `AsmodensiusProofs.NonVacuity` (4 theorems: `A27`..`A30`)
  - `AsmodensiusProofs.AdversarialTests` (6 false-variant refutations: `A33`..`A38`)

Of the 38 audited declarations, 26 are substantive model/boundary/witness/refutation
statements over `Spec.lean` and 12 are explicitly labeled definitional, constant,
congruence, or legacy arithmetic sanity checks.

Zero Mathlib, zero `sorry`, zero `admit`, zero `native_decide`, `autoImplicit = false`,
`relaxedAutoImplicit = false`, 100% pure 7-bit ASCII + LF.
-/
