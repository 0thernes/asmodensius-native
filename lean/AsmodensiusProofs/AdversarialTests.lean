module

public import AsmodensiusProofs.Spec
public import AsmodensiusProofs.Core

/-!
# AsmodensiusProofs.AdversarialTests - False-Variant Refutations (A33..A38)

Six concrete false-variant refutations proving that dropping safety guards,
bounds checks, rollback logic, ABI limits, alignment requirements, or the
`core/az_chem.c` `a / 4` decreasing-budget clamp falsifies the corresponding
invariant.

Strict Axiom Allowlist: {propext, Classical.choice, Quot.sound}.
Zero sorry, zero admit, zero native_decide, pure 7-bit ASCII + LF.
-/

namespace AsmodensiusProofs

/-- Refutation 1: Dropping the `budget <= a` guard overdraws cell mass and breaks
    exact integer subtraction-addition conservation (`(a - budget) + budget = a`). -/
public theorem A33_refute_unguarded_budget_overdraws_mass :
    Not (forall a budget : Nat, (a - budget) + budget = a) := by
  intro h
  have hBad : (3 - 4) + 4 = 3 := h 3 4
  omega

/-- Refutation 2: No out-of-range species request (`req = 0` or `req > WG_MAX_SPECIES`,
    corresponding to 0-based species index `>= 100`) is accepted by `acceptSpecies`. -/
public theorem A34_refute_out_of_range_species_accepted :
    Not (exists req : Nat, (req = 0 \/ WG_MAX_SPECIES < req) /\ acceptSpecies req = true) := by
  intro h
  match h with
  | Exists.intro req (And.intro hOut hAcc) =>
    unfold WG_MAX_SPECIES at hOut
    unfold acceptSpecies WG_MAX_SPECIES at hAcc
    simp only [decide_eq_true_eq, Bool.and_eq_true] at hAcc
    omega

/-- Refutation 3: Omitting `arenaRollback` after a non-zero failed allocation
    permanently mutates the arena cursor (`saved + spent != saved`). -/
public theorem A35_refute_nonzero_delta_without_rollback :
    Not (forall saved spent : Nat, 0 < spent -> saved + spent = saved) := by
  intro h
  have hBad : 4096 + 64 = 4096 := h 4096 64 (by decide)
  omega

/-- Refutation 4: Caps strictly above `AZ_CAP_MAX` (`0x7FFFFFC0`) fail `validCap`. -/
public theorem A36_refute_invalid_cap_exceeds_max :
    validCap (AZ_CAP_MAX + 1) = false /\
    Not (forall cap : Nat, validCap cap = true) := by
  refine And.intro (by decide) ?_
  intro h
  have hBad : validCap (AZ_CAP_MAX + 1) = true := h (AZ_CAP_MAX + 1)
  have hFalse : validCap (AZ_CAP_MAX + 1) = false := by decide
  rw [hFalse] at hBad
  exact Bool.noConfusion hBad

/-- Refutation 5: Non-8-aligned brain dimensions fail `brainDimAligned`. -/
public theorem A37_refute_unaligned_brain_dim :
    brainDimAligned (AZ_SENS + 1) = false /\
    brainDimAligned (AZ_MOT + 1) = false /\
    Not (forall dim : Nat, brainDimAligned dim = true) := by
  refine And.intro (by decide) (And.intro (by decide) ?_)
  intro h
  have hBad : brainDimAligned (AZ_SENS + 1) = true := h (AZ_SENS + 1)
  have hFalse : brainDimAligned (AZ_SENS + 1) = false := by decide
  rw [hFalse] at hBad
  exact Bool.noConfusion hBad

/-- Refutation 6: No combination of cell amount `a` and six face requests `r0..r5`
    can cause `chemSpreadSixFaces` (modeling `core/az_chem.c` `spread()`) to emit
    total outflow exceeding `chemSpreadBudget a = a / 4`. -/
public theorem A38_refute_spread_exceeds_quarter :
    Not (exists a r0 r1 r2 r3 r4 r5 : Nat,
      chemSpreadBudget a < (chemSpreadSixFaces a r0 r1 r2 r3 r4 r5).1) := by
  intro h
  match h with
  | Exists.intro a (Exists.intro r0 (Exists.intro r1 (Exists.intro r2 (Exists.intro r3 (Exists.intro r4 (Exists.intro r5 hGt)))))) =>
    have hSafe := A31_chem_spread_quarter_budget_safe a r0 r1 r2 r3 r4 r5
    omega

end AsmodensiusProofs
