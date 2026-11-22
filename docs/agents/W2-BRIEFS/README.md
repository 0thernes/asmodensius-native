# Wave 2 briefs (ABI version 4)

One brief per module. Each owner writes `core/az_<module>.c` (which replaces
`core/stubs/az_<module>.c` as soon as the file exists) and
`tests/test_<module>.c`, and nothing else. Every brief begins with the
progress-file sentence; an owner that is restarted continues from its file.

Read before your brief: `docs/agents/COMMON.md`, `docs/agents/BUILD.md`,
`docs/agents/ABI-V4-NOTICE.md`, `core/az_abi.h`, the two column files,
`core/az_owners.def`, and the stub of your module. The worked example of
intent, score and resolve with exact books is `tests/protocol/`.

## Landing order (one module at a time into `Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native\build\z`)

The owner's order of 2026-11-18: life, mate, tier1, metab (evolution
itself); then social, econ, colony (game theory and levels); then weather,
chem, worldgen (gradients). sense, physics and stats land where their first
reader needs them. Each landing: all tests, the determinism matrix on
`az_run --lab`, a new reference hash recorded with its cause in
`tests/golden_hashes.txt`. (The first freeze put the physical world first;
the owner's order replaces that.)

## Rules every brief shares

1. **Books.** Matter and energy are exact integers (ARCHITECTURE section 6,
   rules C1 to C7). A conversion books the difference of chemical energy
   (`az_eps_species`, `AZ_EPS_BIO`, `AZ_EPS_WASTE`, `AZ_EPS_TOXIN`): released
   energy goes to the entity's `energy` or to cell heat, absorbed energy is
   paid from one of them. Your test checks `az_total_matter` and
   `az_total_energy` with `CHECK_EQ` after every tick, and that
   `totals.field_underflow` and `totals.dropped_events` stay 0.
2. **Own slot, own cell.** A hook writes its own slot (or its own stepped
   cell) and posts to cells. From a pair partner it reads only what rule D2
   allows.
3. **HOT on the common path.** Per-tick code reads HOT columns, scratch,
   `pub` and the cell. The trait row and the other WARM columns are read in
   pair score and resolve, at birth and death, in Tier 1, in serial passes,
   or behind `az_phase_due(w, uid, <your stream>, k)` with `k >= 2`.
4. **Randomness.** Only `az_rand`, `az_rand4`, `az_rand_below` on the streams
   your module owns (list in az_abi.h). A pair draws with the smaller uid as
   id and the larger in `idx`.
5. **No unchecked narrowing or overflow.** Clamp before every narrowing
   store; count a clamp that changes a physical value in
   `az_tally(w, worker)->sat[]`. The trap build (`-DAZ_UBSAN=ON`) must run
   your test without a trap.
6. **Frames stay under 2,048 bytes.** Bigger buffers come from
   `az_scratch(w, worker)` (16 KiB, valid until the hook returns).
7. **Events.** At most one event per entity and tick from the phase-3 hooks
   (the initiator emits a pair's event), plus the death. Kinds and fields
   are in the table at `AZ_EV_*` in az_abi.h.
8. **Lesions.** A lesion you own is a mask or a skipped step on the same
   path; with the bit set nothing else changes. Your test runs the intact
   and the lesioned world on tuning seeds and compares a measured outcome.
9. **Traits.** Each trait `az_owners.def` gives you is read the way its line
   says and changes an outcome: `tests/test_traits.c` clamps it to 16 and to
   240 in two otherwise equal worlds and must find different digests.
10. **Seeds.** Tune on seeds 1 to 99 only. Never run a reporting seed
    (5001-5024, 5101-5124). `az_lesion` is always given `--seeds 1-24`.
11. **Scales are shared, formulas are yours.** Where two modules must agree
    on a number, the number is in az_abi.h (temperature, pressure, the
    energy table, offer bytes, `AZ_VEL_MAX`). Everything else is your
    design: say what you chose and why in the header comment of your file,
    with the reference you took it from.

## Shared numbers

- Position: u32 per axis; the cell is the top bits (`shift_x/y/z`). x and y
  wrap, z is bounded (`1 << 16` to `2^32 - 1 - (1 << 16)`).
- Velocity: 1/65536 cell per tick, at most `AZ_VEL_MAX` per axis.
- Temperature: `az_temp_of`; a creature's optimum is `AZ_TEMP_OF_TOPT(phen
  TOPT)`, its comfortable band `AZ_TEMP_TOL(phen TTOL)` either side.
- Pressure: `az_cell_pressure(w, c)`.
- Medium: `c_med[2c]` density, `c_med[2c + 1]` viscosity, both 0..255; body
  density is `phen[AZ_P_DENS]` on the same scale.
- Light: `c_light[c]` 0..255; one unit offers `AZ_LIGHT_EU` eu per tick on
  uptake channel `AZ_D_LIGHT`.
- The redox cycle metab and chem share (exact in matter and energy):
  photosynthesis `2 WST_W + 12 eu light -> 1 biomass + 1 OXI`; aerobic
  respiration `1 biomass + 1 OXI -> 2 WST_W + 12 eu`; fermentation
  `2 biomass -> 1 WST_W + 1 ORG_Y + 14 eu`; chemotrophy
  `1 NUT_X -> 1 biomass + 8 eu`; heterotrophy `1 ORG_Y -> 1 biomass`;
  scavenging `1 WST_W + 14 eu -> 1 biomass`; slow oxidation in the field
  `1 ORG_Y + 1 OXI -> 2 WST_W + 12 eu heat`; toxin decay
  `1 TOX_Z -> 1 WST_W + 2 eu heat`.
- References are standard literature cited from memory in each brief; none
  was re-checked online in milestone 4. An owner who is unsure checks with a
  generic web query (no project or owner names) and cites what it found.

## Rules added on 2026-11-18: nothing fixed

The owner's instruction ("Life isn't fixed or fixed sizes or fixed
anything") is section 5 of `ABI-V4-NOTICE.md`. Read it before your brief.
Where a brief says something else, section 5 wins. In short:

13. **No size is assumed** (R1). Genes, neurons, tiles, body parts, memory
    words, members of a colony and young in a brood come in any number. Your
    test has a creature at each edge you can reach: no hidden neuron, one
    part, a missing gene family, an empty memory.
14. **No number of the biology is a constant in your file** (R2). It is a
    trait, a property of the creature's own block, or a world parameter that
    you declare in your own `core/params/az_<module>.def` with a default and
    a range and read as `w->param[AZ_WP_<NAME>]`. Where a brief below names
    a number ("a hazard that doubles at a fixed interval"), that number is
    such a parameter or a trait, never a `#define`.
15. **Graded responses.** A trait or a sense acts through a response that is
    linear or bent by the creature's own `AZ_T_CURVE`; no switch at a fixed
    threshold where a gradient can do the work, and no table of cases.
16. **Strategies are heritable and payoffs are physics.** What a creature
    risks, gives, takes or shares comes from its traits and its motors
    (`AZ_M_STAKE`, `AZ_M_GIVE`, ...); what it wins or loses is conserved
    matter or energy moved in a pair resolve. No payoff matrix.
17. **Running out is an event** (R3). Check every `bool` from the store and
    the tables, and carry on.
18. **Block changes are asked for** (R4): `az_part_request`,
    `az_part_shed_request`, `az_memory_request`.
19. **Wild is the default.** Draw only with `az_rand`, `az_rand4`,
    `az_rand_below`; never branch on `cfg.mode`. Your tests run in lab mode
    (`az_test_config`).
20. **Levels.** Put state where it belongs: body part, creature, colony,
    lineage, ecosystem. The colony's pool and channels are section 5.5.
