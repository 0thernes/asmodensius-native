If your progress file `Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native\build\work\w2\econ\PROGRESS.md` exists, read it first and continue from its next step; never redo a step it records as done and tested.

# Brief: az_econ.c (gathering, stores, trade and prices)

You own `core/az_econ.c`, `tests/test_econ.c` and `core/params/az_econ.def`.
Read `README.md` in this folder and ABI-V4-NOTICE section 5 first. The worked
example is `tests/protocol/az_econ.c` (it also shows the colony pool calls).

Hooks: `az_econ_intent` (phase 1: write the trade bytes of the offer when in
the market; propose `AZ_ACT_TRADE`; post gathering demand in `gather`),
`az_econ_score` (pure), `az_econ_resolve` (the swap, both sides, exact),
`az_econ_range` (phase 3: gather from the cell by the granted share, carry,
build, give to or take from the colony), `az_econ_global` (serial: prices and
volumes into `AZ_G_PRICE`, `AZ_G_VOLUME` from the trade tallies).

Traits you own: BUILD, TRUST, VAL_0..VAL_3, HOARD, TRADE_DRIVE, CARRY_CAP,
STORE_SHARE, COMMONS.

- There is no inventory cap constant: what a creature can carry follows its
  body (`AZ_PART_STORE` parts, CARRY_CAP) and `PART_HOLD`; what does not fit
  goes to the cell, exactly.
- Value is heritable and private (VAL_k): a trade happens when both sides
  gain by their own values; the price that results is an outcome, written to
  `gstate` for others to sense, never an input constant.
- The commons: COMMONS and `AZ_M_GIVE` decide what a member pays into or
  draws from its colony's pool (`az_colony_ask_energy` in phase 1, `take`,
  `give` in phase 3) and stores. Free riding must be possible and must cost
  the colony: that is the game.
- Niche construction: BUILD with `AZ_M_BUILD` moves matter from inventory
  into the cell's marks or deposits (with chem's species), inside the books.
- Lesion: `AZ_LESION_TRADE` (the tick drops trade proposals and skips
  `az_econ_global`); your test shows what stops.
- Test: exact books every tick with `CHECK_EQ`; `trade_units` tallies equal
  both sides; hashes equal across workers and reorder periods; each trait
  changes an outcome.
