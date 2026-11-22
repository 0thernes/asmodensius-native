If your progress file `Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native\build\work\w2\colony\PROGRESS.md` exists, read it first and continue from its next step; never redo a step it records as done and tested.

# Brief: az_colony.c (the colony as an organism)

You own `core/az_colony.c`, `tests/test_colony.c` and
`core/params/az_colony.def`. Read `README.md` in this folder and
ABI-V4-NOTICE sections 5.5 and 5.1 first. Keep the stub's `az_colony_grant`
and `az_colony_commit` exactly (they keep the matter stores exact); the
energy pool and the signal channels are engine code (az_tick.c) and not yours.

Hooks: `az_colony_tier2` (serial, every `tier2_period` ticks: count members
and castes, centroid and velocity, found colonies with `az_colony_found`,
dissolve empty ones, set `need[]` per role, assign roles), `az_colony_found`
(serial; 0 when the table is full: counted in `totals.colony_denied`).

Traits you own: COLONIALITY (readiness to found and to stay), CASTE (which
role a member leans to).

- A colony has no size limit and no fixed number of roles in use: roles are
  filled by a response-threshold rule (the member's CASTE lean against the
  colony's `need[role]`, both graded), so division of labour is an outcome.
- A colony that dissolves returns everything: its matter stores to its cell
  as species matter and its energy pool as heat, exactly.
- The colony's `memory` (four 64-bit patterns), `culture`, `mood` and `alarm`
  are its own state one level above the creature; members read them through
  their senses (`AZ_S_COLONY`, `AZ_S_COLONY_NEED`).
- Lesion: `AZ_LESION_COLONY` (no Tier 2 pass; social stops recruiting). Your
  test shows colonies neither form nor change with the bit set.
- Test: exact books every tick including a dissolution; hashes equal across
  workers and reorder periods; a snapshot taken with living colonies resumes
  to the same hash; both traits change an outcome.
