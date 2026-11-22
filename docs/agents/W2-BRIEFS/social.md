If your progress file `Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native\build\work\w2\social\PROGRESS.md` exists, read it first and continue from its next step; never redo a step it records as done and tested.

# Brief: az_social.c (share, groom, teach, recruit, attack, steal; signals, marks, rank, reputation)

**Amended 2026-11-18 (ABI-V4-NOTICE section 5, README rules 13 to 20; they win over the text below).** You own `AZ_T_STAKE` and `AZ_T_SIG_AIM`. A contest is a game with a continuous stake (trait plus `AZ_M_STAKE`); what is won and lost is conserved and reaches the creature as `AZ_S_PAYOFF`. Signals have a direction (`AZ_M_SIG_AIM`). The colony's pool and channels are section 5.5 (`az_colony_say`, `AZ_M_COLONY_SIG`).

You own `core/az_social.c` and `tests/test_social.c`. Read `README.md` in
this folder first, then `tests/protocol/az_social.c`: it is the worked
example of exact stakes, shared draws and receive caps; keep its book-keeping
pattern. You write `rank`, `sig`, `rep_tag`, `rep_val`, the `AZ_F_COOP` flag,
your offer bytes, and (as the documented exceptions) a groomed slot's own
`toxin`, a recruit's own `colony`, a learner's own `mod_gain` and `mod_bias`,
a robbed or robbing slot's own `inv`.

## az_social_intent (phase 1, everybody; HOT state and the percept)

Zero the offer, then write: STRENGTH (from `mass` and `health`), SIZE
(`az_radius_code`), WEALTH (log of energy plus inventory), RANK (`rank`
mapped to 0..255, 128 neutral), ENERGY and MATTER (the one commitment: a
share of `energy` and of `pool_a`, never more than held), the RETALIATE bit
(from `AZ_P_AGGR` and arousal) and the SUBMIT bit (low rank, low health).

An initiator then picks at most one proposal from its drives and targets
(test the target's uid with `az_is_initiator`): `AZ_M_AGON` with
`AZ_P_AGGR` toward `near_prey` or `near_threat` gives ATTACK or STEAL;
`AZ_M_AFFIL` with `AZ_P_SOCIAL` toward `near_kin`, `child` or `bond` gives
SHARE, GROOM, TEACH or RECRUIT. Only on the branch that proposes may you
read the trait row to choose among them and set the urge: ALTRUISM, STEAL,
BOLDNESS (daring a larger target), TERRITORIAL (attacking an intruder on
your own mark), CARE (share and groom toward `child`; none with
`AZ_LESION_CARE`). No RECRUIT with `AZ_LESION_COLONY`.

## az_social_score (phase 2; pure)

Consent for SHARE, GROOM, TEACH, RECRUIT: refuse a partner remembered as
harmful (`rep_tag`, `rep_val`, weighted by `AZ_T_RECIPROCITY`), prefer kin.
Priority for ATTACK and STEAL: the strongest claimant wins the target.

## az_social_resolve (phase 3; own slot only; both sides compute the same outcome)

- ATTACK: strengths from the two offers plus the trait rows of both
  (WEAPON and SHAPE_N of each side, ARMOR of the one hit) and one shared
  draw (`AZ_RS_CONTEST`, lower uid as id, higher in idx). The defender first
  gets a chance to notice and evade (its SENSE_ACUITY against the
  attacker's SAT: a gaudy attacker is seen coming). A SUBMIT defender with
  SUBMISSION yields without a fight and loses a smaller stake. The loser
  loses its committed ENERGY and MATTER and some health; the winner takes
  the matter it can use (`AZ_T_PREDATION` share into pool_a, the rest to the
  cell) and the energy up to its cap (overflow to cell heat); SPIKES of the
  defender wound the attacker either way; PUNISH lets a defender that
  retaliates do extra harm at an energy cost. Rank: the winner gains by
  DOMINANCE, the loser drops. The loser lays an ALARM mark. Death by wounds:
  `az_request_death(..., AZ_DIE_KILLED)`.
- STEAL: the thief takes up to the victim's `GIVE_QTY` of `GIVE_RES`
  (inventory, exact, receive cap `AZ_INV_MAX`); a victim that notices
  (SENSE_ACUITY) keeps it and may retaliate.
- SHARE: the giver's committed ENERGY moves (receive cap, overflow to heat
  at the receiver's cell). GROOM: the target moves part of its toxin to its
  cell. TEACH: the learner moves its `mod_gain` and `mod_bias` toward the
  teacher's by its own `AZ_T_IMITATE`. RECRUIT: the recruit takes the
  recruiter's colony (from the recruiter's public record).
- Memory: both sides store the partner's tag with what it did (`rep_tag`,
  `rep_val`), older harm fading by `AZ_T_FORGIVE`. COOP flag set or cleared.
  Stats (`az_stat_bump`), one event by the initiator.

## az_social_range (phase 3, every slot)

Behind `az_phase_due(w, uid, AZ_RS_SOCIAL, 2)`: refresh `sig[0]` and
`sig[1]` from the brain's `AZ_M_SIG_A`, `AZ_M_SIG_B` with `SIG_A_GAIN`,
`SIG_B_GAIN`, tied to true condition by `SIG_HONESTY`, plus the passive
`GLOW` on channel A, at an energy cost paid as heat (0 and no cost with
`AZ_LESION_SIGNALS`); lay a TERRITORY mark with the colony id by
`TERRITORIAL`.

## Contracts others rely on

- Offer bytes 0, 1, 2, 4, 12..15 and the RETALIATE and SUBMIT bits are
  yours and always valid; econ and mate write theirs after you.
- A pair moves exactly what the two offers committed.
- `sig` holds the last refreshed value between phases.

## Traits and lesions you own

Through phen: AGGR, SOCIAL. From the row: ARMOR, WEAPON, SENSE_ACUITY,
PREDATION, CARE, ALTRUISM, RECIPROCITY, FORGIVE, PUNISH, STEAL, DOMINANCE,
SUBMISSION, TERRITORIAL, BOLDNESS, SIG_A_GAIN, SIG_B_GAIN, SIG_HONESTY,
IMITATE, SAT, GLOW, SHAPE_N, SPIKES. Lesions: SIGNALS, CARE, COLONY (your
part).

## Tests (tests/test_social.c)

Each act on a hand-made pair with exact numbers for both sides. Contests:
the stronger side wins more often by the stated odds (seeds 1 to 24); the
books balance whoever wins. Reputation: a robbed creature refuses the
thief's next gift offer. Signals rise with the drive and cost energy; 0
under the lesion. Care: a parent feeds its child; not under the lesion
(measured: energy the young receive). A full world shows every act with
exact invariants and equal hashes for 1, 3 and 8 workers, reorder 1 and 9.

References: the hawk-dove contest and assessment (Maynard Smith and Price
1973); reciprocal altruism and image scoring (Trivers 1971; Nowak and
Sigmund 1998); kin selection (Hamilton 1964); honest signalling with costs
(Zahavi 1975; Grafen 1990).
