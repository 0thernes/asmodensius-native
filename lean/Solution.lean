module

public import AsmodensiusProofs

/-!
# Asmodensius-Native v1.1.0 Formal Solution Entrypoint (`Solution.lean`)

Exports all 30 machine-checked Pure Lean 4.35.0 theorems (`A01`..`A30`, zero `sorry`,
zero `admit`, zero `native_decide`, `autoImplicit = false`, `relaxedAutoImplicit = false`)
for isolated judging against `Challenge.lean` via:
    lake comparator --config comparator.json --paranoid
-/
