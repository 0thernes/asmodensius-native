If your progress file `Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native\build\work\w2\weather\PROGRESS.md` exists, read it first and continue from its next step; never redo a step it records as done and tested.

# Brief: az_weather.c (suns, light, heat flow, wind, storms, vents)

**Amended 2026-11-18 (ABI-V4-NOTICE section 5, README rules 13 to 20; they win over the text below).** You write the new vector field `lvec` (where the light comes from, i8 x 4) with `light`; seasons and storms are sensed (`AZ_S_SEASON`, `AZ_S_STORM`). Weather should be chaotic in the plain sense: sensitive to its state, never a fixed cycle table.

You own `core/az_weather.c` and `tests/test_weather.c`. Read `README.md` in
this folder first. You write the derived cell columns `temp`, `wind`,
`charge`, `light` and the words `gstate[AZ_G_WEATHER .. +31]`.

## az_weather_global(w): serial, every tick, and once from worldgen

- Suns: `AZ_G_SUN0_PHASE`, `AZ_G_SUN1_PHASE` from `tick` and
  `sun_period[]`; `AZ_G_SUN0_LIGHT`, `AZ_G_SUN1_LIGHT` = the positive half of
  a sine (0 at night); `AZ_G_SEASON` = their slow beat.
- Storm: a state machine in gstate. It starts with a small probability per
  tick (stream `AZ_RS_WEATHER`, id 0), has an intensity (`AZ_G_STORM`) that
  rises and decays, and a centre (`AZ_G_STORM_CELL`) that drifts. Emit
  `AZ_EV_STORM` when one starts.
- Vents: each `vent_cell[]` emits NUT_X from the reservoir at a steady small
  rate with a pulse now and then: `az_tally(w, 0)->reservoir -= q`,
  `az_cell_post(w, 0, c, AZ_SP_NUT_X, q)`, and the chemical energy of the new
  mineral is external input: `az_tally(w, 0)->energy_in += 24 * q`. Add
  geothermal heat the same way (`az_cell_post_heat` plus `energy_in`).
  Never emit more than the reservoir holds.

## az_weather_cell(w, c, worker): stepped cells, after chemistry

- Light: the top layer gets the suns' light; a cell below gets the light of
  the cell above times a transmittance that falls with the matter in that
  cell (SOLV a little, ORG_Y and WST_W a lot, bedrock all).
- Solar heating: absorbed light becomes heat (`energy_in`); radiative
  cooling grows with temperature (`energy_out`). Choose the two so that a
  lifeless world settles near the worldgen temperatures instead of drifting.
- Heat conduction: push heat to each colder face neighbour, a fraction of
  the temperature difference, at most 1/8 of the cell's heat in total per
  step (`az_cell_post_heat` here negative, there positive). Nothing into or
  out of bedrock.
- Wind: from the temperature differences across the cell, plus a swirl
  around the storm centre scaled by the storm intensity; 0 in bedrock.
- Charge: decays every step; raised by radiotoxin in the cell, by the suns
  in the top layer and by the storm near its centre. At charge >= 192 under
  a storm, lightning fixes mineral: `q` GAS becomes `q` NUT_X and the cell
  pays `24 * q` heat for it (skip when the heat is not there).
- End with `az_weather_derive(w, c)`.

## az_weather_derive(w, c)

`c_temp[c] = az_temp_of(c_heat[c], sum of c_sp[c])` and the light of the cell
from the cell above (or the suns). No posts.

## Contracts others rely on

- A flux is computed from committed values and capped so that no amount and
  no heat can go below 0 (`totals.field_underflow` stays 0).
- `c_light` is 0 in bedrock and never above 255; `c_temp` is 0..32767.
- Under `AZ_LESION_WEATHER` the tick calls neither hook: every weather
  column and word keeps its last value.

## Tests (tests/test_weather.c)

Day and night at the top layer follow the sun periods. Light falls with
depth and is 0 under bedrock. A hot cell warms its neighbours and the heat
total changes only by the tallied input and output. A lifeless world's mean
temperature stays inside a band over 2,000 ticks. Vent emission lowers the
reservoir by exactly what the cells gain. A storm starts, moves and ends;
lightning produces NUT_X with exact books. The lesion: with
`AZ_LESION_WEATHER` the light of a top cell stays constant over a day while
it changes in the intact world.

References: energy-balance climate models (Budyko 1969; Sellers 1969);
Beer-Lambert attenuation of light in a medium.
