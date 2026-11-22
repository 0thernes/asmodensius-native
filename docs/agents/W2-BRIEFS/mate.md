If your progress file `Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native\build\work\w2\mate\PROGRESS.md` exists, read it first and continue from its next step; never redo a step it records as done and tested.

# Brief: az_mate.c (who mates with whom)

You own `core/az_mate.c`, `tests/test_mate.c` and `core/params/az_mate.def`.
Read `README.md` in this folder and ABI-V4-NOTICE section 5 first. The worked
example of intent, score and resolve is `tests/protocol/az_mate.c`.

Hooks: `az_mate_intent` (phase 1: a ripe creature, RECEPT, whose brain drives
`AZ_M_REPRO` proposes `AZ_ACT_MATE` to a neighbour from its percept),
`az_mate_score` (phase 2, pure: how much the responder wants this claimant),
`az_mate_resolve` (phase 3: the bearing side calls `az_life_brood(w, mother,
father, worker)`; nobody else creates births).

Traits you own (az_owners.def): MATING_MODE, SEX_BIAS, PREF_SIGNAL, PREF_SIZE,
PREF_SIMILAR, PREF_RANK, CHOOSY, ORNAMENT, BOND, FIDELITY, GUARD, and the look
traits PATTERN, SHAPE_M, SHEEN as what a chooser sees.

- Choice is a game, not a rule table: a chooser scores what it can see of the
  other in the public record (signal, size, look, rank, kinship) through its
  own preference traits, each a graded response; choosiness trades waiting
  against quality. Ornaments cost what metab makes them cost.
- Modes (bud, self, hermaphrodite, two sexes) come from MATING_MODE and the
  sex locus as the notice says (decision 15); which hermaphrodite bears is
  SEX_BIAS. Bonds and guarding use `pair`/HOT state only between ticks.
- Lesion: `AZ_LESION_MATING` drops mate pairs only (the tick does it); your
  test shows budding and selfing go on and outcrossers stop.
- Test: exact books every tick; equal hashes for 1, 3 and 8 workers and
  reorder 1 and 9; each owned trait changes a measured outcome (who pairs,
  how often, with whom); a world where preference for a signal spreads the
  signal over generations (tuning seeds 1 to 99).
