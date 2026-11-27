module

public import AsmodensiusProofs

/-!
# Asmodensius-Native v1.2.0 Formal Solution Entrypoint (`Solution.lean`)

Exports all 38 machine-checked Pure Lean 4.35.0 theorems (`A01`..`A38`, zero `sorry`,
zero `admit`, zero `native_decide`, `autoImplicit = false`, `relaxedAutoImplicit = false`)
for isolated judging against `Challenge.lean` via:
    lake comparator --config comparator.json --paranoid
-/
