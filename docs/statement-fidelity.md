# Statement-Fidelity Audit Matrix (`docs/statement-fidelity.md`)

This audit maps every Lean 4.35.0 declaration in `lean/AsmodensiusProofs/` (`A01`..`A38`)
to its corresponding C23, NASM, CUDA PTX, or ABI v4 specification in the codebase,
explicitly separating substantive model statements from definitional, constant,
congruence, or legacy arithmetic sanity checks, and stating what remains outside formal proof.

## Classification Summary (v1.2.0)

- **Total audited declarations (`VerifyAxioms.lean`):** `38` (`A01`..`A38`)
- **Substantive statements about `Spec.lean` model definitions (`26` of `38`):**
  - Conservation, metabolism, receive-cap, documented Rule C7, and implemented `core/az_chem.c` `spread()` `a / 4` decreasing-budget pass (`8`): `A01`, `A02`, `A03`, `A21`, `A22_documented_rule_c7_six_face_bound`, `A23`, `A31_chem_spread_quarter_budget_safe`, `A32_chem_spread_small_pool_zero`
  - Protocol and boundary theorems genuinely invoking `Spec.lean` functions (`acceptSpecies`, `arenaRollback`, `storeRelease`, `philoxBulkKey`, `validLesionMask`, `tileBrainByteOffset`) (`8`): `A07`, `A08`, `A09`, `A10`, `A11`, `A17`, `A24`, `A26`
  - Concrete non-vacuity witnesses (`2`): `A27`, `A28`
  - Adversarial false-variant refutations (`8`): `A29`, `A30` (`NonVacuity.lean`), `A33`..`A38` (`AdversarialTests.lean`)
- **Explicitly labeled definitional, constant, congruence, or legacy arithmetic sanity checks (`12` of `38`):**
  - Field-untouched record-update checks (`2`): `A04`, `A05`
  - Constant / ABI identities (`4`): `A06`, `A13`, `A14`, `A25`
  - Explicitly prefixed `sanity_*` checks (`6`, renamed in v1.2.0 to remove inflated v1.1.0 labels):
    - `A12_sanity_readonly_hash_refl` (`hAfter = hBefore`; NOT a proof of C D7 observers)
    - `A15_sanity_legacy_arith_two` (`1 + 1 = 2`; NOT a proof about dual visual worlds)
    - `A16_sanity_legacy_sum_240` (literal sum `= 240`; NOT a domain invariant)
    - `A18_sanity_congruence_twin_hash` (pure congruence given `stepGpu = stepCpu`; NOT hardware GPU/CPU parity)
    - `A19_sanity_identity_prefetch_hash` (identity function `fun x => x`; NOT x86-64 `PREFETCHT0` semantics)
    - `A20_sanity_legacy_sum_244` (literal sum `= 244`; NOT a domain invariant)
- **Scope boundary:** All 38 declarations concern hand-written `Nat` definitions in `lean/AsmodensiusProofs/Spec.lean`. **None of them verifies the C23, NASM, or CUDA PTX code.**
- **Permitted axiom envelope:** `[propext, Classical.choice, Quot.sound]` (`0` `sorryAx`, `0` `Lean.ofReduceBool`, `0` `Lean.trustCompiler`).

## Line-by-Line Mapping Matrix

| Declaration(s) | Lean Module | Category | Source Location | Formalized Property | Scope Boundary / Non-Formalized Gap |
|---|---|---|---|---|---|
| `A01_bio_to_cell_mass_conserved` | `Core.lean` | Substantive | `core/az_world.c:745` (`az_total_matter`) | `transferBioToCell s dm` preserves `totalMass` when `dm <= s.bioMass`. | Single-step `Nat` model; full multi-module C loop checked empirically by CTest + UBSan. |
| `A02_energy_dissipation_conserved`, `A03_heat_sink_monotone` | `Core.lean` | Substantive | `core/az_world.c:766` (`az_total_energy`), Rule C3 | `dissipateToHeat s de` preserves `totalEnergy` and monotonically increases `cellHeat`. | Single-step `Nat` model; external tallies `energy_in`/`energy_out` checked via CTest audits. |
| `A04_underflow_preserved_zero`, `A05_dropped_events_preserved_zero` | `Core.lean` | Sanity Check | `Spec.lean` record updates | `fieldUnderflow = 0` and `droppedEvents = 0` remain `0` under record updates. | Definitional: record updates never touch these fields. Runtime event-pool and cell arithmetic tested in `test_ledger` and `test_field`. |
| `A06_species_cap_exact` | `Core.lean` | Sanity Check | `core/az_worldgen.c:84` | `WG_MAX_SPECIES = 100`. | Constant identity matching `core/az_worldgen.c:84`. |
| `A07_species_refusal_boundary`, `A29_refute_unbounded_species_accepted`, `A34_refute_out_of_range_species_accepted` | `Core.lean`, `NonVacuity.lean`, `AdversarialTests.lean` | Substantive + Adversarial | `core/az_worldgen.c:84`, `tools/az_run.c:174` | `Spec.acceptSpecies req = false` for all `req > 100` (`A07`), counterexample `101` (`A29`), and refutation of any `req = 0` or `req > 100` acceptance (`A34`). | Restated in v1.2.0 so `A07` genuinely calls `Spec.acceptSpecies`. CLI exit code 2 checked live via `az_run --species 101`. |
| `A08_arena_rollback_exact`, `A09_arena_retry_succeeds`, `A35_refute_nonzero_delta_without_rollback` | `Core.lean`, `AdversarialTests.lean` | Substantive + Adversarial | `core/az_world.c:279-372` (`P4-CORE-02`) | `Spec.arenaRollback saved spent = saved` (`A08`), retry `arenaRollback saved spent + small <= avail` (`A09`), and refutation of cursor preservation without rollback (`A35`). | Restated in v1.2.0 to invoke `Spec.arenaRollback` without tautological hypothesis conjunction. Pointer/alignment behavior tested in `tests/test_core.c`. |
| `A10_store_release_reduces_live`, `A11_store_release_exact_balance` | `Core.lean` | Substantive | `core/az_world.c:433`, `core/az_store.c:192` (`P4-CORE-03`) | `Spec.storeRelease st units` strictly reduces `liveUnits` for `0 < units <= st.liveUnits` (`A10`) and conserves `liveUnits` while keeping `totalUnits` invariant (`A11`). | Restated in v1.2.0 to invoke `Spec.storeRelease`. Two-half compaction parity tested in `tests/test_store.c`. |
| `A12_sanity_readonly_hash_refl` | `Core.lean` | Sanity Check | Renamed in v1.2.0 (`ERRATA.md` #10) | Definitional reflexivity (`hAfter = hBefore`). | Explicitly NOT a proof of C observer non-interference (Rule D7 is tested empirically in `cli_live_smoke`, etc.). |
| `A13_trait_abi_width_112`, `A14_eps_bio_positive`, `A25_brain_dim_kernel_alignment` | `Core.lean` | Sanity Check | `core/az_abi.h:28-35,51,61,421` | `AZ_TRAITS = 112`, `0 < AZ_EPS_BIO`, `brainDimAligned AZ_SENS`, `brainDimAligned AZ_MOT`, `AZ_UNIT = 64`, `AZ_CHUNK = 128`. | ABI constant and alignment checks matching `_Static_assert` in `core/az_abi.h`. |
| `A15_sanity_legacy_arith_two`, `A16_sanity_legacy_sum_240`, `A20_sanity_legacy_sum_244` | `Core.lean` | Sanity Check | Historical v1.0.0 baseline accounting | `1 + 1 = 2`, `42+62+72+48+16 = 240`, `42+62+72+48+20 = 244`. | Renamed in v1.2.0; explicitly NOT claimed as domain invariants or dual-world proofs. |
| `A17_bulk_philox_key_equivalence`, `A30_refute_unbounded_philox_bulk_key_eq` | `Core.lean`, `NonVacuity.lean` | Substantive + Adversarial | `core/az_tick.c`, `kernels/az_philox_avx2.asm` (`TR-13`) | `uid < 2^32 -> philoxBulkKey tkey0 uid = tkey0` (`A17`) plus counterexample at `uid = 2^32` (`A30`). | Models key-folding lemma; full 10-round Philox4x32 AVX2 bit parity tested vs C oracle in `test_rng`. |
| `A18_sanity_congruence_twin_hash`, `A19_sanity_identity_prefetch_hash` | `Core.lean` | Sanity Check | Renamed in v1.2.0 (`ERRATA.md` #10) | Congruence given `forall s, stepGpu s = stepCpu s` (`A18`); identity function `fun x => x` (`A19`). | Explicitly NOT formal proofs of GPU/CPU hardware parity or x86-64 `PREFETCHT0`; hardware parity is tested by `test_gpu` (`164/164`) and `cli_gpu_tick_parity`. |
| `A21_receive_cap_mass_conserved` | `Core.lean` | Substantive | `docs/ARCHITECTURE.md` Rule C5 | For `cur <= cap`, `(kept, overflow) = receiveCap cur cap q` satisfies `kept + overflow = q` and `cur + kept <= cap`. | Exact arithmetic closure of Rule C5 overflow redirection. |
| `A22_documented_rule_c7_six_face_bound` | `Core.lean` | Substantive | `docs/ARCHITECTURE.md` Rule C7 (documented rule only) | Six face outflows each `<= amt / 8` sum to `<= amt`. | Formalizes documented Rule C7 only (NOT `core/az_chem.c` and NOT `core/az_field.c`, which only applies posted deltas). |
| `A31_chem_spread_quarter_budget_safe`, `A32_chem_spread_small_pool_zero`, `A38_refute_spread_exceeds_quarter` | `Core.lean`, `AdversarialTests.lean` | Substantive + Adversarial | `core/az_chem.c:160,180-199` (`spread()`) | Six-face decreasing budget `left = a / 4` (`chemSpreadSixFaces`) satisfies `total + rem = a / 4`, `total <= a / 4 <= a`, `(a - total) + total = a` (`A31`); `a < 4 -> total = 0` (`A32`); no request tuple can exceed `a / 4` (`A38`). | Directly models the implemented `core/az_chem.c` `spread()` budget loop; stochastic rounding and multi-worker posting tested empirically in `test_chem`, `test_chem_stock`, and determinism matrices. |
| `A23_nut_x_to_bio_energy_conserved` | `Core.lean` | Substantive | `core/az_abi.h:420-421`, Rule C2 | Converting `k <= nutXMass` from `NUT_X` (24 eu/mu) to biomass (16 eu/mu) + `8*k` thermal energy preserves total chemical+thermal energy. | Models C2 stoichiometry; full multi-species reaction network tested in `test_metab` and `test_chem`. |
| `A24_lesion_mask_all_exact`, `A26_gpu_offset8_injective_and_32gib_range` | `Core.lean` | Substantive | `core/az_abi.h:409`, `gpu/az_gpu_dev.c:103,118` | `validLesionMask` accepts `2^23 - 1` and rejects `2^23`; `tileBrainByteOffset` (`off8 * 8`) is injective and `< 32 GiB` for all `u32`. | Matches ABI constants; PTX pointer safety verified on RTX 5070 Ti in `test_gpu.exe`. |
| `A27_nonvacuity_ledger_and_arena_witness`, `A28_nonvacuity_philox_and_gpu_witness` | `NonVacuity.lean` | Witness | `AsmodensiusProofs.Spec` | Concrete inhabited witnesses for ledger conservation, rollback, store release, `chemSpreadSixFaces 100 10..10 = (25, 0)`, Philox (`uid=1004019`), VRAM offset, C5 receive-cap, C2 metabolism, `validCap`, `acceptSpecies`. | Proves hypothesis bundles are non-vacuously satisfiable. |
| `A33_refute_unguarded_budget_overdraws_mass`, `A36_refute_invalid_cap_exceeds_max`, `A37_refute_unaligned_brain_dim` | `AdversarialTests.lean` | Adversarial | Law 5, `core/az_abi.h:61,410` | Refutes unguarded `(a - budget) + budget = a` (`a=3, budget=4`), refutes `validCap (AZ_CAP_MAX + 1)`, refutes non-8-aligned brain dims (`65`, `25`). | Proves boundary guards are mathematically necessary. |

## Explicitly Non-Formalized Layers

1. **C23 Compiler Translation:** GCC/Clang lowering of `core/*.c` to x86-64 is verified empirically via CTest, `ubsan_trap_build`, `core_obj_gate`, and `avx_stack_gate`, not via CompCert/VST.
2. **NASM Assembly & CUDA PTX Codegen:** Verified empirically via differential testing (`test_rng`, `test_brain`, `test_physics`, `test_gpu` 164/164, `cli_gpu_tick_parity`) rather than formal ISA semantics.
3. **Long-Horizon Evolutionary Dynamics:** Not formalized and not claimed; see [MEGA_ATLAS_CROSSCHECK.md](MEGA_ATLAS_CROSSCHECK.md) (`0 / 1,900` retired).
