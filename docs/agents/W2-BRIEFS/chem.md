If your progress file `Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native\build\work\w2\chem\PROGRESS.md` exists, read it first and continue from its next step; never redo a step it records as done and tested.

# Brief: az_chem.c (reactions, diffusion, settling, phases, the medium)

**Amended 2026-11-18 (ABI-V4-NOTICE section 5, README rules 13 to 20; they win over the text below).** Niche construction: what creatures deposit and mark (`AZ_M_BUILD`) changes the cell they and their young live in; keep it inside the exact books.

You own `core/az_chem.c` and `tests/test_chem.c`. Read `README.md` in this
folder first. You write the derived cell columns `phase` and `med`, and you
decay `mark`.

## az_chem_cell(w, c, worker): stepped cells, before weather

Everything is a post from committed amounts; a step never takes more than a
fixed fraction of what the cell holds, so nothing can go below 0. A bedrock
cell (`c_solid == 255`) is skipped.

1. **Reactions** (exact; rates rise with temperature, stream `AZ_RS_CHEM`
   only where a rate needs a draw to move single units):
   - slow oxidation: `1 ORG_Y + 1 OXI -> 2 WST_W`, 12 eu of heat per unit;
   - toxin decay: `1 TOX_Z -> 1 WST_W`, 2 eu of heat (stream `AZ_RS_DECAY`);
   - mineral precipitation near a hot cell: `1 NUT_X -> 1 STR_S`, 24 eu of
     heat (the chimneys that vents build);
   - burial on the lowest open cell of a column: a small share of WST_W,
     ORG_Y and STR_S goes into the reservoir
     (`az_tally(w, worker)->reservoir += q`), its chemical energy stays in the
     cell as heat. This closes the cycle the vents open.
2. **Diffusion.** Each species spreads to the six face neighbours down its
   own gradient: push only, a fraction of the difference, slower for a
   species that is solid in this cell, faster for gas. Nothing enters
   bedrock.
3. **Settling and rising.** Under gravity, ORG_Y, WST_W, STR_S and NUT_X
   sink (an extra downward share); GAS rises. With `AZ_LESION_GRAVITY` this
   step is skipped, nothing else.
4. **Marks.** Every mark channel of the cell decays (a fixed share plus 1),
   written in place.
5. End with `az_chem_derive(w, c)`.

## az_chem_derive(w, c)

- `c_phase[c]`: two bits per species from the cell's temperature against
  that species' melting and boiling point (your table); GAS is plasma when
  `c_charge[c] >= 192`.
- `c_med[2c]` density and `c_med[2c + 1]` viscosity, 0..255, from the
  amounts and phases: mostly liquid solvent is dense and viscous, mostly gas
  is thin, structure and detritus thicken it. Bedrock: 255, 255.

## Contracts others rely on

- Matter moves only between cells, or between a cell and the reservoir
  through the tally; the energy of every reaction goes to cell heat.
- `med` is defined for every cell from tick 0 (worldgen calls the derive
  function) and changes only when the cell is stepped.
- Under `AZ_LESION_CHEM` the tick does not call `az_chem_cell`: amounts stay
  where creatures and weather put them, marks stop decaying, `phase` and
  `med` freeze.

## Tests (tests/test_chem.c)

Each reaction alone on a hand-made cell: amounts and heat change by exactly
the stated numbers. A point load of each species spreads, total unchanged; a
bedrock wall blocks it. ORG_Y sinks with gravity and does not with
`AZ_LESION_GRAVITY` (the measured outcome is the centre of mass of ORG_Y
after 500 ticks). Burial moves matter to the reservoir and the totals hold.
Marks decay to 0. Phases follow temperature; `med` separates the ocean from
the atmosphere of the worldgen world. A lifeless world keeps both invariants
for 2,000 ticks with 0 underflow, for 1, 3 and 8 workers with equal hashes.

References: explicit finite-volume diffusion with flux limiting (standard
numerical practice; the push-only rule keeps amounts non-negative);
Arrhenius-type temperature dependence, as a table.
