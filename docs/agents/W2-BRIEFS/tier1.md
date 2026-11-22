If your progress file `Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native\build\work\w2\tier1\PROGRESS.md` exists, read it first and continue from its next step; never redo a step it records as done and tested.

# Brief: az_tier1.c (slow cognition: learning, memory, mood)

You own `core/az_tier1.c`, `tests/test_tier1.c` and
`core/params/az_tier1.def`. Read `README.md` in this folder and ABI-V4-NOTICE
section 5 first. The toolkits are `core/tk_sh.c`, `tk_hop.c`, `tk_qgt.c`,
`tk_qsv.c` with their tests; call them, do not copy them.

Hook: `az_tier1_range` (stage 6; act on a slot when `az_tier1_due(w, uid)`).
You own `arousal`, `mod_gain`, `mod_bias`, `elig`, `rbase`, `rprev`, `ctx`,
`qstate`, `memory`, `engram`, `hop_n`, the words of `az_memory()` and the
call of `az_classify`.

Traits you own: CONFORMITY, MOODINESS, LEARN_RATE, EXPLORE, MEMORY,
PLASTICITY, CTX_GAIN, Q_DEPTH, Q_COUPLE, HOPFIELD, ATTENTION, AROUSAL_GAIN,
AROUSAL_DECAY, IMPROV.

- Plasticity adapts the creature's own brain in life: `gain[8]` and
  `bias[8]` in its `AzBrain` header (reward-modulated, with the eligibility
  trace), never its weights' layout. The brain has any number of hidden
  blocks, also none: handle `n_hb == 0`.
- Memory has no fixed size: the four `engram` words are the working set;
  more is asked for with `az_memory_request` (rule 18) at a rate set by
  MEMORY and HOPFIELD and paid for (tell metab's brief reader what a word
  costs through a parameter both read: declare it in your file).
- Improvisation: IMPROV with `AZ_M_IMPROV` sets how much of the creature's
  own noise (`az_rand` on your stream) enters its modulation. No script.
- Lesions you honour: PLASTICITY, METRIC (tk_qgt preconditioner), TIER1_Q
  (tk_qsv), TIER1_SH (tk_sh), MEMORY (tk_hop): each a mask or a skipped step
  on the same path, each with a measured contrast in your test.
- Test: exact books (learning moves no matter; if it costs energy, the heat
  goes to the cell); hashes equal across workers and reorder periods; a
  creature in a two-armed world learns the better arm faster with a higher
  LEARN_RATE; each trait changes an outcome.
