# Statement-Fidelity Audit Matrix (`docs/statement-fidelity.md`)

This audit maps every Lean 4.35.0 theorem in `lean/AsmodensiusProofs/` (`A01`..`A30`)
to its corresponding C23, NASM, CUDA PTX, or ABI v4 specification in the codebase,
explicitly stating what IS proved by the Lean kernel and what remains outside formal proof.

## Classification Summary

- **Total audited theorems (`VerifyAxioms.lean`):** `30` (`A01`..`A30`)
- **Substantive algebraic/protocol/boundary theorems:** `21` (`A01`..`A05`, `A07`..`A12`, `A14`, `A17`..`A19`, `A21`..`A26`)
- **Explicit non-vacuity inhabitants:** `2` (`A27`, `A28`)
- **Adversarial false-variant refutations (guard necessity):** `2` (`A29`, `A30`)
- **Legacy constant / ecosystem arithmetic sanity checks (retained for v1.0.0 compatibility):** `5` (`A06`, `A13`, `A15`, `A16`, `A20`)
- **Permitted axiom envelope:** `[propext, Classical.choice, Quot.sound]` (`0` `sorryAx`, `0` `Lean.ofReduceBool`, `0` `Lean.trustCompiler`)

## Line-by-Line Mapping Matrix

| Theorem(s) | Lean Module | Source Location | Formalized Property | Scope Boundary / Non-Formalized Gap |
|---|---|---|---|---|
| `A01_bio_to_cell_mass_conserved` | `Core.lean` | `core/az_world.c:745` (`az_total_matter`) | `transferBioToCell s dm` preserves `totalMass` when `dm <= s.bioMass`. | Single-step `Nat` model; full multi-module C loop checked empirically by `77/77` CTest + UBSan. |
| `A02_energy_dissipation_conserved`, `A03_heat_sink_monotone` | `Core.lean` | `core/az_world.c:766` (`az_total_energy`), Rule C3 | `dissipateToHeat s de` preserves `totalEnergy` and monotonically increases `cellHeat`. | Single-step `Nat` model; external tallies `energy_in`/`energy_out` checked via CTest audits. |
| `A04_underflow_preserved_zero`, `A05_dropped_events_preserved_zero` | `Core.lean` | `core/az_abi.h`, Law 5 | `fieldUnderflow = 0` and `droppedEvents = 0` are preserved by valid transfers. | Runtime event-pool budget and cell-field arithmetic verified in `test_ledger` and `test_field`. |
| `A06_species_cap_exact`, `A07_species_refusal_boundary`, `A29_refute_unbounded_species_accepted` | `Core.lean`, `NonVacuity.lean` | `core/az_worldgen.c:84`, `tools/az_run.c:174` | `WG_MAX_SPECIES = 100`, `req > 100` implies `100 < req`, and `acceptSpecies` rejects `101` (proving guard is non-trivial). | `A06` is a constant identity check; CLI exit code 2 verified directly via `az_run --species 101`. |
| `A08_arena_rollback_exact`, `A09_arena_retry_succeeds` | `Core.lean` | `core/az_world.c:279-372` (`P4-CORE-02`) | Failed allocation cursor rollback `(saved + spent) - spent = saved` preserves headroom for subsequent smaller creation. | Abstract cursor arithmetic; actual pointer/alignment behavior checked in `tests/test_core.c`. |
| `A10_store_release_reduces_live`, `A11_store_release_exact_balance` | `Core.lean` | `core/az_world.c:433`, `core/az_store.c:192` (`P4-CORE-03`) | Releasing `units > 0` strictly decreases `liveUnits` while conserving `(live - units) + units = live`. | Abstract unit accounting; two-half compaction parity tested in `tests/test_store.c`. |
| `A12_observer_hash_d7_invariant` | `Core.lean` | `docs/ARCHITECTURE.md` Rule D7, `tools/*_report.c` | Pure read-only observer leaves state hash identical (`hAfter = hBefore`). | Structural model of D7; actual C observer non-interference tested in `cli_live_smoke`, `cli_colony_commons`, etc. |
| `A13_trait_abi_width_112`, `A14_eps_bio_positive` | `Core.lean` | `core/az_abi.h:35,421` | `AZ_TRAITS = 112` and `0 < AZ_EPS_BIO` (`16`). | ABI constant checks matching `_Static_assert` in `core/az_abi.h`. |
| `A15`, `A16`, `A20` | `Core.lean` | Historical v1.0.0 baseline accounting | `1 + 1 = 2`, `42+62+72+48+16 = 240`, `42+62+72+48+20 = 244`. | Retained solely for v1.0.0 backward compatibility; explicitly NOT claimed as domain invariants. |
| `A17_bulk_philox_key_equivalence`, `A30_refute_unbounded_philox_bulk_key_eq` | `Core.lean`, `NonVacuity.lean` | `core/az_tick.c`, `kernels/az_philox_avx2.asm` (`TR-13`) | `uid < 2^32 -> Nat.xor tkey0 (uid / 2^32) = tkey0`, PLUS counterexample at `uid = 2^32` proving scalar fallback guard is strictly required. | Models key-folding lemma; full 10-round Philox4x32 AVX2 bit parity tested vs C oracle in `test_rng`. |
| `A18_gpu_cpu_twin_hash_parity`, `A19_prefetch_observational_transparency` | `Core.lean` | `gpu/az_gpu_tick.c` (`TR-04`), `core/az_tick.c:233` (`TR-01`) | Functional equality `stepGpu = stepCpu` implies identical `hash`; identity prefetch hint preserves `hash`. | Conditional on hardware twin equivalence; actual hardware parity verified by `test_gpu` (`164/164`) and `cli_gpu_tick_parity`. |
| `A21_receive_cap_mass_conserved` | `Core.lean` | `docs/ARCHITECTURE.md` Rule C5 | For `cur <= cap`, `(kept, overflow) = receiveCap cur cap q` satisfies `kept + overflow = q` and `cur + kept <= cap`. | Exact arithmetic closure of Rule C5 overflow redirection. |
| `A22_six_face_flux_never_overdraws` | `Core.lean` | `docs/ARCHITECTURE.md` Rule C7, `core/az_field.c` | Six face outflows each `<= amt / 8` sum to `<= amt`, preventing underflow. | Exact `Nat` guarantee of Rule C7; Philox randomized rounding tested in `test_field`. |
| `A23_nut_x_to_bio_energy_conserved` | `Core.lean` | `core/az_abi.h:420-421`, Rule C2 | Converting `k <= nutXMass` from `NUT_X` (24 eu/mu) to biomass (16 eu/mu) + `8*k` thermal energy preserves total chemical+thermal energy. | Models C2 stoichiometry; full multi-species reaction network tested in `test_metab` and `test_chem`. |
| `A24_lesion_mask_all_exact`, `A25_brain_dim_kernel_alignment` | `Core.lean` | `core/az_abi.h:28-34,51,61,409` | `AZ_LESION_ALL = 2^23 - 1` accepts all 23 named bits and rejects bit 23 (`2^23`); `AZ_SENS=64`, `AZ_MOT=24` divisible by 8; `AZ_UNIT=64`, `AZ_CHUNK=128`. | Matches `core/az_abi.h` `_Static_assert`s and CLI refusal bounds. |
| `A26_gpu_offset8_injective_and_32gib_range` | `Core.lean` | `gpu/az_gpu_dev.c:103,118` (`TR-18`) | 8-byte store offsets `bo8 * 8` are injective and map every `u32` (`< 2^32`) strictly below 32 GiB without wraparound. | Models address arithmetic; PTX pointer safety verified on RTX 5070 Ti in `test_gpu.exe`. |
| `A27_nonvacuity_ledger_and_arena_witness`, `A28_nonvacuity_philox_and_gpu_witness` | `NonVacuity.lean` | `AsmodensiusProofs.Spec` | Concrete inhabited witnesses satisfying ledger conservation, arena rollback, store release, TR-13 Philox (`uid=1004019`), TR-18 offset (`1048576`), C5 receive-cap, and C2 metabolism. | Proves hypothesis bundles are non-vacuously satisfiable. |

## Explicitly Non-Formalized Layers

1. **C23 Compiler Translation:** GCC/Clang lowering of `core/*.c` to x86-64 is verified empirically via `77/77` CTest, `ubsan_trap_build`, `core_obj_gate`, and `avx_stack_gate`, not via CompCert/VST.
2. **NASM Assembly & CUDA PTX Codegen:** Verified empirically via differential testing (`test_rng`, `test_brain`, `test_physics`, `test_gpu` 164/164, `cli_gpu_tick_parity`) rather than formal ISA semantics.
3. **Long-Horizon Evolutionary Dynamics:** Not formalized and not claimed; see [MEGA_ATLAS_CROSSCHECK.md](MEGA_ATLAS_CROSSCHECK.md).
