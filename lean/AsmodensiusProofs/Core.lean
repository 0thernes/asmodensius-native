module

public import AsmodensiusProofs.Spec

/-!
# AsmodensiusProofs.Core - Pure Kernel Model Invariants & Labeled Sanity Checks (A01..A26, A31..A32)

Machine-checked formal proofs over the abstract `Nat` definitions in `AsmodensiusProofs.Spec`:
  - Substantive model statements (16 in this module):
      * A01..A03: Integer matter & energy conservation, monotone heat sink (Law 5)
      * A07:      `acceptSpecies` refusal for `req > WG_MAX_SPECIES` (Law 7 / P406-WG-04 / DEMO-16)
      * A08..A09: `arenaRollback` exact cursor restoration & retry headroom (P4-CORE-02)
      * A10..A11: `storeRelease` live-unit reduction and exact balance (P4-CORE-03)
      * A17:      TR-13 bulk Philox AVX2 key equivalence (`philoxBulkKey` for `uid < 2^32`)
      * A21:      Rule C5 `receiveCap` mass conservation and cap bound
      * A22:      Documented Rule C7 (`cellFaceFluxBound = amt / 8`, `docs/ARCHITECTURE.md` only)
      * A23:      Rule C2 `NUT_X` -> biomass exact chemical-thermal energy balance
      * A24:      ABI v4 23-bit `validLesionMask` boundary
      * A26:      TR-18 32 GiB VRAM `tileBrainByteOffset` injectivity and range
      * A31..A32: Implemented `core/az_chem.c` `spread()` decreasing-budget `a / 4` rule & `a < 4` zero skip
  - Explicitly labeled definitional, constant, congruence, or legacy arithmetic sanity checks (12):
      * A04..A05: Record-update field preservation (`fieldUnderflow`, `droppedEvents` untouched)
      * A06, A13, A14, A25: ABI constant and alignment identities
      * A12_sanity_readonly_hash_refl: Definitional reflexivity check (NOT a proof of C D7 observers)
      * A15_sanity_legacy_arith_two, A16_sanity_legacy_sum_240, A20_sanity_legacy_sum_244:
        Historical v1.0.0 literal arithmetic checks (NOT domain invariants)
      * A18_sanity_congruence_twin_hash: Function congruence given `stepGpu = stepCpu` (NOT hardware parity)
      * A19_sanity_identity_prefetch_hash: Identity-function check (NOT x86-64 PREFETCHT0 semantics)

None of these theorems verifies GCC/Clang C23 compilation, NASM assembly, or CUDA PTX codegen.
Strict Axiom Allowlist: {propext, Classical.choice, Quot.sound}.
Zero sorry, zero admit, zero native_decide, pure 7-bit ASCII + LF.
-/

namespace AsmodensiusProofs

private theorem chemSpreadStep_conserved (left req : Nat) :
    (chemSpreadStep left req).1 + (chemSpreadStep left req).2 = left := by
  unfold chemSpreadStep
  dsimp only
  omega

public theorem A01_bio_to_cell_mass_conserved (s : LedgerState) (dm : Nat) (h : dm <= s.bioMass) :
    totalMass (transferBioToCell s dm) = totalMass s := by
  unfold totalMass transferBioToCell
  dsimp only
  omega

public theorem A02_energy_dissipation_conserved (s : LedgerState) (de : Nat) (h : de <= s.bioEnergy) :
    totalEnergy (dissipateToHeat s de) = totalEnergy s := by
  unfold totalEnergy dissipateToHeat
  dsimp only
  omega

public theorem A03_heat_sink_monotone (s : LedgerState) (de : Nat) :
    s.cellHeat <= (dissipateToHeat s de).cellHeat := by
  unfold dissipateToHeat
  dsimp only
  omega

/-- Definitional sanity check: `transferBioToCell` does not modify `fieldUnderflow`. -/
public theorem A04_underflow_preserved_zero (s : LedgerState) (dm : Nat)
    (hz : s.fieldUnderflow = 0) :
    (transferBioToCell s dm).fieldUnderflow = 0 := by
  unfold transferBioToCell
  exact hz

/-- Definitional sanity check: `dissipateToHeat` does not modify `droppedEvents`. -/
public theorem A05_dropped_events_preserved_zero (s : LedgerState) (de : Nat)
    (hz : s.droppedEvents = 0) :
    (dissipateToHeat s de).droppedEvents = 0 := by
  unfold dissipateToHeat
  exact hz

/-- Constant identity check matching `core/az_worldgen.c:84` (`WG_MAX_SPECIES = 100`). -/
public theorem A06_species_cap_exact : WG_MAX_SPECIES = 100 := rfl

/-- Substantive model check: `Spec.acceptSpecies` refuses any request above `WG_MAX_SPECIES`. -/
public theorem A07_species_refusal_boundary (req : Nat) (h : req > WG_MAX_SPECIES) :
    acceptSpecies req = false := by
  unfold acceptSpecies WG_MAX_SPECIES at *
  simp only [decide_eq_false_iff_not, Bool.and_eq_true, decide_eq_true_eq]
  omega

/-- Substantive model check: `Spec.arenaRollback` restores `saved` exactly after any `spent`. -/
public theorem A08_arena_rollback_exact (saved spent : Nat) :
    arenaRollback saved spent = saved := by
  unfold arenaRollback
  omega

/-- Substantive model check: after `Spec.arenaRollback` of a failed allocation `spent`,
    a subsequent retry `small` fits within `avail` whenever `saved + small <= avail`. -/
public theorem A09_arena_retry_succeeds (saved spent small avail : Nat)
    (hSmall : saved + small <= avail) :
    arenaRollback saved spent + small <= avail := by
  unfold arenaRollback
  omega

/-- Substantive model check: `Spec.storeRelease` strictly reduces `liveUnits` when `0 < units <= liveUnits`. -/
public theorem A10_store_release_reduces_live (st : StoreState) (units : Nat)
    (hPos : 0 < units) (hLe : units <= st.liveUnits) :
    (storeRelease st units).liveUnits < st.liveUnits := by
  unfold storeRelease
  dsimp only
  omega

/-- Substantive model check: `Spec.storeRelease` conserves `liveUnits` and leaves `totalUnits` unchanged. -/
public theorem A11_store_release_exact_balance (st : StoreState) (units : Nat)
    (hLe : units <= st.liveUnits) :
    (storeRelease st units).liveUnits + units = st.liveUnits /\
    (storeRelease st units).totalUnits = st.totalUnits := by
  unfold storeRelease
  dsimp only
  omega

/-- Definitional reflexivity sanity check (`hAfter = hBefore`).
    NOT a formal proof of C observer non-interference (Rule D7 is tested empirically in CTest). -/
public theorem A12_sanity_readonly_hash_refl (hBefore : Nat) :
    let hAfter := hBefore
    hAfter = hBefore := by
  rfl

/-- Constant identity check matching `core/az_abi.h:35` (`AZ_TRAITS = 112`). -/
public theorem A13_trait_abi_width_112 : AZ_TRAITS = 112 := rfl

/-- Constant positivity check matching `core/az_abi.h:421` (`AZ_EPS_BIO = 16`). -/
public theorem A14_eps_bio_positive : 0 < AZ_EPS_BIO := by
  unfold AZ_EPS_BIO
  omega

/-- Legacy v1.0.0 literal arithmetic sanity check (`1 + 1 = 2`).
    Retained for backward traceability only; NOT a formal proof about dual visual worlds. -/
public theorem A15_sanity_legacy_arith_two : 1 + 1 = 2 := rfl

/-- Legacy v1.0.0 literal arithmetic sanity check (`42 + 62 + 72 + 48 + 16 = 240`).
    Retained for backward traceability only; NOT a domain invariant. -/
public theorem A16_sanity_legacy_sum_240 :
    42 + 62 + 72 + 48 + 16 = 240 := rfl

/-- Substantive model check (TR-13): `Spec.philoxBulkKey` equals `tkey0` whenever `uid < 2^32`. -/
public theorem A17_bulk_philox_key_equivalence (tkey0 uid : Nat) (h : uid < 2 ^ 32) :
    philoxBulkKey tkey0 uid = tkey0 := by
  unfold philoxBulkKey
  have hdiv : uid / 2 ^ 32 = 0 := Nat.div_eq_of_lt h
  rw [hdiv]
  exact Nat.xor_zero tkey0

/-- Congruence sanity check: assuming `stepGpu = stepCpu`, applying `hash` gives equal outputs.
    NOT a proof of RTX 5070 Ti PTX vs CPU hardware parity (tested empirically by 164/164 `test_gpu`). -/
public theorem A18_sanity_congruence_twin_hash
    {S : Type} (hash : S -> Nat) (stepCpu stepGpu : S -> S)
    (hTwin : forall s, stepGpu s = stepCpu s) (s : S) :
    hash (stepGpu s) = hash (stepCpu s) := by
  rw [hTwin s]

/-- Identity-function sanity check: `fun x => x` preserves `hash`.
    NOT a formal proof of x86-64 `PREFETCHT0` observational transparency. -/
public theorem A19_sanity_identity_prefetch_hash
    {S : Type} (hash : S -> Nat) (s : S) :
    let prefetchHint : S -> S := fun x => x
    hash (prefetchHint s) = hash s := by
  rfl

/-- Legacy v1.0.0 literal arithmetic sanity check (`42 + 62 + 72 + 48 + 20 = 244`).
    Retained for backward traceability only; NOT a domain invariant. -/
public theorem A20_sanity_legacy_sum_244 :
    42 + 62 + 72 + 48 + 20 = 244 := rfl

public theorem A21_receive_cap_mass_conserved (cur cap q : Nat) (hCur : cur <= cap) :
    (receiveCap cur cap q).1 + (receiveCap cur cap q).2 = q /\
    cur + (receiveCap cur cap q).1 <= cap := by
  unfold receiveCap
  dsimp only
  omega

/-- Documented Rule C7 (`docs/ARCHITECTURE.md` only): six face outflows each `<= amt / 8`
    sum to `<= amt`. For the actual `a / 4` decreasing-budget pass implemented in
    `core/az_chem.c` (`spread()`), see `A31_chem_spread_quarter_budget_safe`. -/
public theorem A22_documented_rule_c7_six_face_bound (amt f0 f1 f2 f3 f4 f5 : Nat)
    (h0 : f0 <= cellFaceFluxBound amt) (h1 : f1 <= cellFaceFluxBound amt)
    (h2 : f2 <= cellFaceFluxBound amt) (h3 : f3 <= cellFaceFluxBound amt)
    (h4 : f4 <= cellFaceFluxBound amt) (h5 : f5 <= cellFaceFluxBound amt) :
    f0 + f1 + f2 + f3 + f4 + f5 <= amt := by
  unfold cellFaceFluxBound at h0 h1 h2 h3 h4 h5
  omega

public theorem A23_nut_x_to_bio_energy_conserved (c : ChemEnergyState) (k : Nat)
    (hk : k <= c.nutXMass) :
    totalChemThermalEnergy (metabolizeNutXToBio c k) = totalChemThermalEnergy c := by
  unfold totalChemThermalEnergy metabolizeNutXToBio AZ_EPS_NUT_X AZ_EPS_BIO
  dsimp only
  omega

public theorem A24_lesion_mask_all_exact :
    AZ_LESION_ALL = 2 ^ AZ_LESION_BITS - 1 /\
    validLesionMask AZ_LESION_ALL = true /\
    validLesionMask (2 ^ AZ_LESION_BITS) = false := by
  decide

/-- Constant alignment check matching `core/az_abi.h:28-34,51,61`. -/
public theorem A25_brain_dim_kernel_alignment :
    brainDimAligned AZ_SENS = true /\ brainDimAligned AZ_MOT = true /\
    AZ_SENS = 64 /\ AZ_MOT = 24 /\ AZ_UNIT = 64 /\ AZ_CHUNK = 128 := by
  decide

public theorem A26_gpu_offset8_injective_and_32gib_range (a b : Nat) :
    (tileBrainByteOffset a = tileBrainByteOffset b -> a = b) /\
    (a < 2 ^ 32 -> tileBrainByteOffset a < 32 * 1024 * 1024 * 1024) := by
  unfold tileBrainByteOffset
  exact And.intro (fun h => by omega) (fun ha => by omega)

/-- Substantive model check for the ACTUAL transport code in `core/az_chem.c` (`spread()`,
    lines 180-199): clamping each of the six face outflows against a decreasing budget
    initialized to `left = a / 4` satisfies exact budget accounting (`total + rem = a / 4`),
    caps total outflow at `a / 4 <= a`, and preserves non-negative remaining mass
    (`(a - total) + total = a`). -/
public theorem A31_chem_spread_quarter_budget_safe (a r0 r1 r2 r3 r4 r5 : Nat) :
    let res := chemSpreadSixFaces a r0 r1 r2 r3 r4 r5
    res.1 + res.2 = chemSpreadBudget a /\
    res.1 <= chemSpreadBudget a /\
    res.1 <= a /\
    (a - res.1) + res.1 = a := by
  dsimp only [chemSpreadSixFaces]
  have h0 := chemSpreadStep_conserved (chemSpreadBudget a) r0
  have h1 := chemSpreadStep_conserved (chemSpreadStep (chemSpreadBudget a) r0).2 r1
  have h2 := chemSpreadStep_conserved (chemSpreadStep (chemSpreadStep (chemSpreadBudget a) r0).2 r1).2 r2
  have h3 := chemSpreadStep_conserved (chemSpreadStep (chemSpreadStep (chemSpreadStep (chemSpreadBudget a) r0).2 r1).2 r2).2 r3
  have h4 := chemSpreadStep_conserved (chemSpreadStep (chemSpreadStep (chemSpreadStep (chemSpreadStep (chemSpreadBudget a) r0).2 r1).2 r2).2 r3).2 r4
  have h5 := chemSpreadStep_conserved (chemSpreadStep (chemSpreadStep (chemSpreadStep (chemSpreadStep (chemSpreadStep (chemSpreadBudget a) r0).2 r1).2 r2).2 r3).2 r4).2 r5
  have hBud : chemSpreadBudget a <= a := by
    unfold chemSpreadBudget
    omega
  omega

/-- Substantive model check for `core/az_chem.c` (`spread()`, line 160): when cell species
    mass `a < 4`, the quarter-budget is `0` and zero units can leave across all six faces. -/
public theorem A32_chem_spread_small_pool_zero (a r0 r1 r2 r3 r4 r5 : Nat) (h : a < 4) :
    (chemSpreadSixFaces a r0 r1 r2 r3 r4 r5).1 = 0 := by
  have hSafe := A31_chem_spread_quarter_budget_safe a r0 r1 r2 r3 r4 r5
  have hZero : chemSpreadBudget a = 0 := by
    unfold chemSpreadBudget
    omega
  omega

end AsmodensiusProofs
