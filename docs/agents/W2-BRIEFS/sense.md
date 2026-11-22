If your progress file `Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native\build\work\w2\sense\PROGRESS.md` exists, read it first and continue from its next step; never redo a step it records as done and tested.

# Brief: az_sense.c (the 64 sensory channels)

**Amended 2026-11-18 (ABI-V4-NOTICE section 5, README rules 13 to 20; these rules define the current contract).** There are 64 channels (5.4). You own `AZ_T_TAXIS` (gain of the directional channels) and `AZ_T_CURVE` (the shape of the response). Fill the directional channels from `AzCellSense` (`temp_grad`, `light_grad`, `grav`, `lvec`) and `AzPercept` (`sigv_a`, `sigv_b`) projected on the creature's own axes; fill `AZ_S_COLONY`, `COLONY_NEED`, `MASS`, `PARTS`, `SEASON`, `STORM`, `PAYOFF`, `STAKE`. Body parts of kind `AZ_PART_SENSOR` should matter to acuity.

You own `core/az_sense.c` and `tests/test_sense.c`. Read `README.md` in this
folder first. You write nothing but your outputs: `AzCellSense` and
`sens[64]`. TAXIS and CURVE shape the response. The TAXIS lesion removes the
field-gradient gain. The tick masks your channels for
`AZ_LESION_SENSES` and `AZ_LESION_NOISE`, and the brain is the only reader.

## az_sense_cell(w, c, cs): once per cell and tick

From the cell and its six face neighbours (missing neighbours above and
below count as the cell itself): the amount here and the gradient of the
four food channels (LIGHT, NUT_X, ORG_Y, WST_W through `az_field_avail`),
ambient toxin, `az_cell_pressure`, temperature, light, the medium's density
and viscosity, wind, the four marks, the owner of mark 0 and the gradient of
mark 0.

## az_sense(w, slot, p, cs, sens): every active entity, every tick

HOT state, the percept and `cs` only. All channels 0..127; directions go
through `az_rotor_apply_inv` into the body frame and then `az_tetra4`.

- interoception: ENERGY (log scale), HEALTH, PAIN (health deficit and toxin
  load), AGE (a stage code from the flags: juvenile, adult, ripe adult,
  senescent), CARRY (inventory load), READY (`AZ_F_RECEPT`), WASTE, RANK,
  AROUSAL;
- the cell: LIGHT, HOT and COLD (how far the cell is above or below the
  body's own band, `AZ_TEMP_OF_TOPT`, `AZ_TEMP_TOL`), MEDIUM (density
  against the body's `AZ_P_DENS`), TOXIN, PRESS, DAY (the suns'
  `AZ_G_SUN0_LIGHT`, `AZ_G_SUN1_LIGHT`), FLOW (wind along the heading), UP
  (the body's vertical axis against local up, opposite gravity or +z in zero gravity);
- FOOD: the four gradients weighted by the body's own diet affinities
  (`AZ_P_DIET_L/X/Y/W`), so each creature smells what it can eat;
- others: THREAT, PREY, KIN (offset sums of the percept classes), CROWD,
  TOUCH, ALIGN (shared heading of kin), SIG_A, SIG_B;
- specific others, each as a nearness and an "ahead" channel: MATE
  (`best_mate`), BOND, FAMILY (parent or child), HOME (the colony centroid
  from `w->colonies[colony]`, or the gradient of mark 0 for the solitary);
- NOISE: one draw per tick (`AZ_RS_NOISE`).

## Contracts others rely on

- `az_sense` is a pure function of its inputs and the uid: same inputs, same
  channels, for any worker.
- The thirteen interoceptive channels the tick keeps under `AZ_LESION_SENSES`
  (table `az_interoceptive` in az_tick.c) depend on the body only.
- No channel exceeds 127.

## Tests (tests/test_sense.c)

On hand-made worlds (`az_test_shell`): each channel responds to its cause
and to nothing else (move the food, the threat, the mate: the right
receptors rise). Turning the body by a rotor turns the direction channels
the same way. Every channel stays in 0..127 for random inputs (1,000,000
cases). In a full world, `AZ_LESION_SENSES` changes the final hash and
leaves the interoceptive channels equal on tick 0.

References: population coding of direction on a tetrahedral receptor basis
(four non-negative axes span 3D; Salinas and Abbott 1994 for population
vectors); Weber-Fechner log scaling for magnitudes.
