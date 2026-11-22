If your progress file `Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native\build\work\w2\stats\PROGRESS.md` exists, read it first and continue from its next step; never redo a step it records as done and tested.

# Brief: az_stats.c (measurement and classification)

You own `core/az_stats.c`, `tests/test_stats.c` and
`core/params/az_stats.def`. Read `README.md` in this folder first.

Hooks: `az_stats_collect(w, out)` (a measurement for tools and the viewer:
it reads the world and writes only `out`), `az_classify(w, slot)` (called by
Tier 1: `stat[]` -> `archetype`), `az_look_class(w, slot)` (a measurement).
`az_stat_bump` is how other modules count what a creature did.

- Nothing here feeds back into the world except `archetype`, which is per
  creature (notice, decision 14): no census in `gstate`.
- No fixed bins: distributions of genome length, neurons, tiles, body parts,
  body matter, memory words, colony size and lineage depth are reported with
  quantiles (min, 5, 25, 50, 75, 95 percent, max) from what is there, in
  `AzStats` (ask the lead for fields you need; AzStats is not state).
- `az_classify` is a graded, heritable-free summary of what the creature did
  (what it ate, fought, traded, built), never a table keyed on species.
- Test: `az_stats_collect` changes no byte of the world (hash before and
  after); counts add up to `n_live`; classification is the same for every
  worker count.
