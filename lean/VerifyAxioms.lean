module

public import AsmodensiusProofs

/-!
# Asmodensius-Native v1.1.0 - Standalone Axiom Audit Script (`VerifyAxioms.lean`)

Runs `#print axioms` on all 30 machine-checked theorems (`A01`..`A30`).
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
#print axioms AsmodensiusProofs.A12_observer_hash_d7_invariant
#print axioms AsmodensiusProofs.A13_trait_abi_width_112
#print axioms AsmodensiusProofs.A14_eps_bio_positive
#print axioms AsmodensiusProofs.A15_dual_visual_worlds_verified
#print axioms AsmodensiusProofs.A16_ecosystem_240_pure_lean4_theorems
#print axioms AsmodensiusProofs.A17_bulk_philox_key_equivalence
#print axioms AsmodensiusProofs.A18_gpu_cpu_twin_hash_parity
#print axioms AsmodensiusProofs.A19_prefetch_observational_transparency
#print axioms AsmodensiusProofs.A20_ecosystem_244_pure_lean4_theorems
#print axioms AsmodensiusProofs.A21_receive_cap_mass_conserved
#print axioms AsmodensiusProofs.A22_six_face_flux_never_overdraws
#print axioms AsmodensiusProofs.A23_nut_x_to_bio_energy_conserved
#print axioms AsmodensiusProofs.A24_lesion_mask_all_exact
#print axioms AsmodensiusProofs.A25_brain_dim_kernel_alignment
#print axioms AsmodensiusProofs.A26_gpu_offset8_injective_and_32gib_range
#print axioms AsmodensiusProofs.A27_nonvacuity_ledger_and_arena_witness
#print axioms AsmodensiusProofs.A28_nonvacuity_philox_and_gpu_witness
#print axioms AsmodensiusProofs.A29_refute_unbounded_species_accepted
#print axioms AsmodensiusProofs.A30_refute_unbounded_philox_bulk_key_eq
