module

/-!
# AsmodensiusProofs.Spec - Pure Shared Definitions Only (Zero Proofs, Zero sorry)

Canonical mathematical and ABI v4 definitions shared identically by BOTH:
  - `Challenge.lean` (isolated statement-only specification; imports ONLY this file)
  - `AsmodensiusProofs/Core.lean`, `AsmodensiusProofs/NonVacuity.lean`,
    `AsmodensiusProofs/AdversarialTests.lean`, and `Solution.lean`

All constants match `core/az_abi.h` (ABI version 4) and `core/az_worldgen.c` verbatim.
Note on chemical transport: `cellFaceFluxBound` models documented Rule C7 (`amt / 8`
in `docs/ARCHITECTURE.md`), whereas `chemSpreadBudget`, `chemSpreadStep`, and
`chemSpreadSixFaces` model the actual decreasing-budget `a / 4` per-species pass
implemented in `core/az_chem.c` (`spread()`, lines 180-199).

Pure 7-bit ASCII, LF line endings, zero tabs, zero Mathlib, zero sorry.
-/

namespace AsmodensiusProofs

@[expose] public def AZ_ABI_VERSION : Nat := 4
@[expose] public def AZ_EPS_NUT_X : Nat := 24
@[expose] public def AZ_EPS_BIO : Nat := 16
@[expose] public def AZ_EPS_TOXIN : Nat := 4
@[expose] public def AZ_EPS_WASTE : Nat := 2
@[expose] public def WG_MAX_SPECIES : Nat := 100
@[expose] public def AZ_SENS : Nat := 64
@[expose] public def AZ_MOT : Nat := 24
@[expose] public def AZ_MODG : Nat := 8
@[expose] public def AZ_UNIT : Nat := 64
@[expose] public def AZ_TRAITS : Nat := 112
@[expose] public def AZ_PHEN : Nat := 16
@[expose] public def AZ_NSPECIES : Nat := 8
@[expose] public def AZ_CHUNK : Nat := 128
@[expose] public def AZ_LESION_BITS : Nat := 23
@[expose] public def AZ_LESION_ALL : Nat := 0x7FFFFF
@[expose] public def AZ_CAP_MAX : Nat := 0x7FFFFFC0

public structure LedgerState where
  bioMass : Nat
  cellMass : Nat
  bioEnergy : Nat
  cellHeat : Nat
  fieldUnderflow : Nat
  droppedEvents : Nat
deriving Repr, DecidableEq

@[expose] public def totalMass (s : LedgerState) : Nat :=
  s.bioMass + s.cellMass

@[expose] public def totalEnergy (s : LedgerState) : Nat :=
  s.bioEnergy + s.cellHeat

@[expose] public def transferBioToCell (s : LedgerState) (dm : Nat) : LedgerState :=
  { s with bioMass := s.bioMass - dm, cellMass := s.cellMass + dm }

@[expose] public def dissipateToHeat (s : LedgerState) (de : Nat) : LedgerState :=
  { s with bioEnergy := s.bioEnergy - de, cellHeat := s.cellHeat + de }

public structure ChemEnergyState where
  nutXMass : Nat
  bioMass : Nat
  thermalEnergy : Nat
deriving Repr, DecidableEq

@[expose] public def totalChemThermalEnergy (c : ChemEnergyState) : Nat :=
  AZ_EPS_NUT_X * c.nutXMass + AZ_EPS_BIO * c.bioMass + c.thermalEnergy

@[expose] public def metabolizeNutXToBio (c : ChemEnergyState) (k : Nat) : ChemEnergyState :=
  { nutXMass := c.nutXMass - k,
    bioMass := c.bioMass + k,
    thermalEnergy := c.thermalEnergy + (AZ_EPS_NUT_X - AZ_EPS_BIO) * k }

public structure ArenaState where
  used : Nat
  cap : Nat
deriving Repr, DecidableEq

@[expose] public def arenaRollback (saved spent : Nat) : Nat :=
  (saved + spent) - spent

public structure StoreState where
  liveUnits : Nat
  totalUnits : Nat
deriving Repr, DecidableEq

@[expose] public def storeRelease (st : StoreState) (units : Nat) : StoreState :=
  { st with liveUnits := st.liveUnits - units }

@[expose] public def acceptSpecies (req : Nat) : Bool :=
  decide (1 <= req && req <= WG_MAX_SPECIES)

@[expose] public def validLesionMask (mask : Nat) : Bool :=
  decide (mask <= AZ_LESION_ALL)

@[expose] public def validCap (cap : Nat) : Bool :=
  decide (cap <= AZ_CAP_MAX)

@[expose] public def brainDimAligned (dim : Nat) : Bool :=
  decide (dim % 8 = 0)

@[expose] public def philoxBulkKey (tkey0 uid : Nat) : Nat :=
  Nat.xor tkey0 (uid / 2 ^ 32)

@[expose] public def receiveCap (cur cap q : Nat) : Prod Nat Nat :=
  let headroom := cap - cur
  let kept := min q headroom
  (kept, q - kept)

/-- Documented Rule C7 per-face upper bound (`docs/ARCHITECTURE.md`: floor(A / 8)).
    Not the transport budget implemented in `core/az_chem.c` (see `chemSpreadBudget`). -/
@[expose] public def cellFaceFluxBound (amt : Nat) : Nat :=
  amt / 8

/-- Implemented `core/az_chem.c` (`spread()`, line 182): per-species outflow budget `left = a / 4`. -/
@[expose] public def chemSpreadBudget (a : Nat) : Nat :=
  a / 4

/-- Implemented `core/az_chem.c` (`spread()`, lines 193-195): clamp face outflow `n` to remaining
    budget `left` and decrement `left` by the clamped outflow. -/
@[expose] public def chemSpreadStep (left req : Nat) : Prod Nat Nat :=
  let sent := min req left
  (sent, left - sent)

/-- Implemented `core/az_chem.c` (`spread()`, lines 182-199): six-face decreasing-budget loop
    returning `(totalOutflow, remainingBudget)` for one species with cell amount `a`. -/
@[expose] public def chemSpreadSixFaces (a r0 r1 r2 r3 r4 r5 : Nat) : Prod Nat Nat :=
  let b0 := chemSpreadBudget a
  let (n0, b1) := chemSpreadStep b0 r0
  let (n1, b2) := chemSpreadStep b1 r1
  let (n2, b3) := chemSpreadStep b2 r2
  let (n3, b4) := chemSpreadStep b3 r3
  let (n4, b5) := chemSpreadStep b4 r4
  let (n5, b6) := chemSpreadStep b5 r5
  (n0 + n1 + n2 + n3 + n4 + n5, b6)

@[expose] public def tileBrainByteOffset (off8 : Nat) : Nat :=
  off8 * 8

end AsmodensiusProofs
