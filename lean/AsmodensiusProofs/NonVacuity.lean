module

public import AsmodensiusProofs.Spec
public import AsmodensiusProofs.Core

/-!
# AsmodensiusProofs.NonVacuity - Concrete Inhabitant Witnesses & Guard Refutations (A27..A30)

Constructs explicit concrete witnesses proving that hypothesis bundles in `Core.lean`
are simultaneously satisfiable (non-vacuity) and proves sharp negative boundary
refutations showing why explicit guard predicates (`acceptSpecies`, `uid < 2^32`)
cannot be dropped without breaking soundness.
-/

namespace AsmodensiusProofs

public theorem A27_nonvacuity_ledger_and_arena_witness :
    let s : LedgerState := {
      bioMass := 742819, cellMass := 1000000,
      bioEnergy := 5000000, cellHeat := 4724070,
      fieldUnderflow := 0, droppedEvents := 0
    }
    let st : StoreState := { liveUnits := 27961, totalUnits := 311360 }
    totalMass (transferBioToCell s 500) = totalMass s /\
    totalEnergy (dissipateToHeat s 250) = totalEnergy s /\
    (transferBioToCell s 500).fieldUnderflow = 0 /\
    (dissipateToHeat s 250).droppedEvents = 0 /\
    arenaRollback 4096 1024 = 4096 /\
    (storeRelease st 42).liveUnits < st.liveUnits /\
    (storeRelease st 42).liveUnits + 42 = st.liveUnits /\
    chemSpreadSixFaces 100 10 10 10 10 10 10 = (25, 0) := by
  decide

public theorem A28_nonvacuity_philox_and_gpu_witness :
    1004019 < 2 ^ 32 /\
    philoxBulkKey 0xCAFEBABE 1004019 = 0xCAFEBABE /\
    tileBrainByteOffset 1048576 = 8388608 /\
    receiveCap 90 100 25 = (10, 15) /\
    totalChemThermalEnergy (metabolizeNutXToBio { nutXMass := 50, bioMass := 10, thermalEnergy := 100 } 12) =
      totalChemThermalEnergy { nutXMass := 50, bioMass := 10, thermalEnergy := 100 } /\
    validCap 1004096 = true /\
    acceptSpecies 100 = true := by
  decide

public theorem A29_refute_unbounded_species_accepted :
    Not (forall req : Nat, acceptSpecies req = true) := by
  intro h
  have h101 : acceptSpecies 101 = true := h 101
  have hFalse : acceptSpecies 101 = false := by decide
  rw [hFalse] at h101
  exact Bool.noConfusion h101

public theorem A30_refute_unbounded_philox_bulk_key_eq :
    Not (forall tkey0 uid : Nat, philoxBulkKey tkey0 uid = tkey0) := by
  intro h
  have hBad : philoxBulkKey 0 (2 ^ 32) = 0 := h 0 (2 ^ 32)
  have hOne : philoxBulkKey 0 (2 ^ 32) = 1 := by
    unfold philoxBulkKey
    have hdiv : 2 ^ 32 / 2 ^ 32 = 1 := Nat.div_self (by decide)
    rw [hdiv]
    decide
  rw [hOne] at hBad
  exact Nat.noConfusion hBad

end AsmodensiusProofs
