# wishlist-pdfs: what was extracted from the six companion PDFs

Agent key `wishlist-pdfs`, 2026-11-17. Scratch: `S:\req\wishlist-pdfs\` (text dumps, parsers, builders).
"p" in every `source_ref` is the PDF page index (the printed page number is one lower in most of these files);
"dump line" is the line in `S:\req\wishlist-pdfs\<pdf name>.txt` (PyMuPDF `get_text("text")`, one `=====PAGE n=====` marker per page).

## Files written

| File | Items | What |
|---|---|---|
| `wishlist.json` | 512 | FW 143 + HX 128 + FS 17 + P6 102 + OH 122 (id prefixes below) |
| `nested_precision.json` | 134 | NESTOR levels, 21-rung ladder, 6 tier columns, per-cell precisions and clocks, formats, recipes |
| `frontier_upgrade_map.json` | 272 | Frontier Sweep section 6 table, one item per row |

`wishlist.json` id prefixes and counts:
- `FW-<path>` 143: every wish in `Future-Wishlist.pdf`, id = the owner's path id (e.g. `FW-6.1.3.1`). Counts match the cover: Near 9, Far 75, Moonshot 53, Pure Sci-Fi 6; rare 34; second-order 13; domains 11/17/8/20/10/15/19/11/9/8/15. `criterion` = the wish's "First experiment" text.
- `HX-*` 128: `Apex-Composite-Creature-Report.pdf` (Heptarch): 7 seats, 8 states, 5 laws + 2 doctrine laws, 14 capabilities, 4 omni coverage fractions, 6 emergent features, 6 feature combinations, the hard line, 10 telemetry tabs, ordering rule + phases 0-9, upgrades U1-U25, 10 lineage pulls, 6 Cold Strike phases + gate, 4 QWM-bridge rules, 8 structure items.
- `FS-F1..F10`, `FS-TP1..6`, `FS-SCF` 17: `Frontier-Sweep-2026.pdf` fusion candidates, top proposals, self-contained filter.
- `P6-*` 102: `Post-Quantum-ASI-Simulation.pdf`: all 82 idea cards (S 27, PQ 15, ASI 23, SIM 17) with their `[PROPOSAL]` text, 15 grails, 4 hard lines, 1 energy proposal.
- `OH-*` 122: `Omni-Harness-Catalog.pdf`: 43 capabilities, 52 sensor adapters, 16 platforms, 10 template sections, 1 scope statement.

Categories in `wishlist.json`: world 56, ecology 37, society 60, cognition 186, scale 13, outside-engine 160.
Rules used: a Future-Wishlist wish is `outside-engine` when its "Maps to" names neither LINEAGE nor HEPTARCH (22 wishes); every Omni-Harness item is `outside-engine` because the catalog says Creature #1 is out of scope; host/measurement items (read-only substrate, telemetry, exploit log) are filed under `world`. The other category choices are my reading of each item, done by hand (table in `build_wishlist.py`). In `frontier_upgrade_map.json` the category is assigned by keyword from the seat column (not hand-reviewed) and rows with self_contained_feasible "no" are `outside-engine`; its ids `FS-UM-001..272` are row ordinals because the source has none.

## Nested precision, as the source states it
- Sizes: micro 1–100 units; meso 10^2–10^5 units; macro "few parameters but long-lived, error-intolerant state". ChronoBlock-4: blocks of 16 elements, 4-bit codes, FP8 (E4M3) block scale, 2-bit mode tag, FP32 tensor scale, about 4.63 bits per element. Spindle synapse: about 24–28 bits.
- Update rates (Figure 1): sensory reservoir tau ~ 1 ms; sLTC motor CPG tau adaptive; LTC/CfC controller tau ~ 20–200 ms; Mamba summariser event-driven; world model on an imagination clock; homeostat tau ~ minutes–hours.
- Precisions: analog/stochastic FP8; ternary weights + INT8 state; BF16 + FP32 tau-params; FP8 E4M3 / BF16; BF16 + posit16 latents; FP64 integrators. Six static defaults and three run-time triggers are itemised.

## Not found
- No population target and no 3D world dimensions in any of the six PDFs (searched for counts of creatures/organisms/agents, "3D world", world/arena size). Only "a population of ten and a population of ten thousand" (NESTOR p3) and "hundreds of identical micro-brains" (recipe R5).
- NESTOR gives no byte sizes, no Hz rates and no unit counts per meso cell; no tier is tied to a cache level. The ladder's colour zones could not be mapped to individual rungs from the text.
- NESTOR and the Apex report say they use no pass/fail thresholds, so most `criterion` fields are empty there (304 of 512 in `wishlist.json`, 113 of 134 in `nested_precision.json`).
- Not itemised: Apex Table 7 trait rows, Table 10 scorecard rows and Table 3; Frontier Sweep sections 2-5 and 8 (findings, atlas); Post-Quantum visionaries (349) and predictions (20); NESTOR DSD/RSR/BSS, S2S internals and experiments E3-E14.
- These PDFs describe a tokenless creature but several items mention a JEPA-style predictor, "quantum" world models and the QWM/QLLM paper layer; those are recorded as written and flagged `outside-engine` where the source places them outside the creature.

## Commands run (2026-11-17) and output tails
```
python -I extract.py            -> pages: 68 / 75 / 41 / 38 / 86 / 40 (six PDFs)
python -I parse_fw.py           -> detail boxes: 143  concordance rows: 143
                                   tier counts (concordance): {'FAR': 75, 'NEAR': 9, 'MOONSHOT': 53, 'PURE SCI-FI': 6}
                                   rare: 34 2nd: 13
python -I parse_pq.py           -> cards: 82 Counter({'S': 27, 'ASI': 23, 'SIM': 17, 'PQ': 15})   without proposal: []
python -I build_wishlist.py     -> wrote N:/docs/requirements/wishlist.json items: 512
                                   by prefix: {'FW': 143, 'HX': 128, 'FS': 17, 'P6': 102, 'OH': 122}
python -I build_nested.py       -> wrote N:/docs/requirements/nested_precision.json items: 134
python -I parse_um.py; python -I build_um.py
                                -> section 6 rows: 272 {'yes': 231, 'partial': 21, 'no': 20}   rows with an empty cell: 0
python -I validate.py           -> no page mismatches; 9 low-overlap flags, all on label words I added (e.g. "column", "grid")
```
`validate.py` checks the schema, unique ids, that each cited dump line sits on the cited page, and that an item's words occur near the cited lines. The HX and NP items were keyed by hand from the dumps; the FW, P6, FS-F, OH and FS-UM items were parsed by script.
