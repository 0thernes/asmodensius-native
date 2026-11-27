module

public import AsmodensiusProofs

/-!
# Asmodensius-Native v1.2.0 - Standalone Axiom Audit Script (`VerifyAxioms.lean`)

Runs `#print axioms` on all 38 machine-checked theorems (`A01`..`A38`).
Every declaration MUST depend strictly on `[propext, Classical.choice, Quot.sound]`
(or zero axioms) with ZERO `sorryAx`, `Lean.ofReduceBool`, `Lean.trustCompiler`,
`_native`, or custom axioms.
-/

#print axioms AsmodensiusProofs.A01_bio_to_cell_mass_conserved
#print axioms AsmodensiusProofs.A02_energy_dissipation_conserved
#print axioms AsmodensiusProofs.A03_heat_sink_monotone
#print axioms AsmodensiusProofs.A04_underflow_preserved_zero
#print axioms AsmodensiusProofs.A05_dropped_events_preserved_zero
#print axioms AsmodensiusProofs.A06_species_cap_exact
#print axioms AsmodensiusProofs.A07_species_refusal_boundary
#print axioms AsmodensiusProofs.A08_arena_rollback_exact
#print axioms AsmodensiusProofs.A09_arena_retry_succeeds
#print axioms AsmodensiusProofs.A10_store_release_reduces_live
#print axioms AsmodensiusProofs.A11_store_release_exact_balance
#print axioms AsmodensiusProofs.A12_sanity_readonly_hash_refl
#print axioms AsmodensiusProofs.A13_trait_abi_width_112
#print axioms AsmodensiusProofs.A14_eps_bio_positive
#print axioms AsmodensiusProofs.A15_sanity_legacy_arith_two
#print axioms AsmodensiusProofs.A16_sanity_legacy_sum_240
#print axioms AsmodensiusProofs.A17_bulk_philox_key_equivalence
#print axioms AsmodensiusProofs.A18_sanity_congruence_twin_hash
#print axioms AsmodensiusProofs.A19_sanity_identity_prefetch_hash
#print axioms AsmodensiusProofs.A20_sanity_legacy_sum_244
#print axioms AsmodensiusProofs.A21_receive_cap_mass_conserved
#print axioms AsmodensiusProofs.A22_documented_rule_c7_six_face_bound
#print axioms AsmodensiusProofs.A23_nut_x_to_bio_energy_conserved
#print axioms AsmodensiusProofs.A24_lesion_mask_all_exact
#print axioms AsmodensiusProofs.A25_brain_dim_kernel_alignment
#print axioms AsmodensiusProofs.A26_gpu_offset8_injective_and_32gib_range
#print axioms AsmodensiusProofs.A27_nonvacuity_ledger_and_arena_witness
#print axioms AsmodensiusProofs.A28_nonvacuity_philox_and_gpu_witness
#print axioms AsmodensiusProofs.A29_refute_unbounded_species_accepted
#print axioms AsmodensiusProofs.A30_refute_unbounded_philox_bulk_key_eq
#print axioms AsmodensiusProofs.A31_chem_spread_quarter_budget_safe
#print axioms AsmodensiusProofs.A32_chem_spread_small_pool_zero
#print axioms AsmodensiusProofs.A33_refute_unguarded_budget_overdraws_mass
#print axioms AsmodensiusProofs.A34_refute_out_of_range_species_accepted
#print axioms AsmodensiusProofs.A35_refute_nonzero_delta_without_rollback
#print axioms AsmodensiusProofs.A36_refute_invalid_cap_exceeds_max
#print axioms AsmodensiusProofs.A37_refute_unaligned_brain_dim
#print axioms AsmodensiusProofs.A38_refute_spread_exceeds_quarter
