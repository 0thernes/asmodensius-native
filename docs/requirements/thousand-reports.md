# thousand-reports: what the three 1000-row reports contain

Sources (read-only, parsed by script, never read whole):
`W:/TSOTCHKE_1000_BIOLOGIC_SUBSTRATES_REPORT.md` (#0001-#1000, D01-D20),
`W:/SUPER_INTELLIGENCE_1000_HIGH_WEIRDNESS_REPORT.md` (#1001-#2000, D21-D40),
`W:/ALIFE_CONSCIOUSNESS_1000_CREATURE_REPORT.md` (#2001-#3000, D41-D60).

## Found (ran now: `python -I S:/req/thousand-reports/parse.py`, then `stats.py`, `build.py`)

- Each report: exactly 1000 table rows, ids unique and contiguous, 20 domains x 50 rows. Global id space #0001-#3000, 60 domains.
- The rows are templated. Distinct owner-worded mechanisms: **900**, not 3000.
  - ALIFE: 500 bases x 2 variants ("Forward Dynamical Coupling", "Reciprocal Back-Action"). The operator and role cells are the same 2 strings for all 1000 rows, so only the names carry content.
  - SUPER: 200 bases x 5 variants; TSOT: 200 bases x 5 variants. Only the "Primal Forward Operator" row has its own formula; the other 4 wrap it in a fixed template.
- ALIFE sublists as rows: Butlin-14 + 14 brain qualities (D41), 10 frameworks (D42), CQM-01..25 (D43), LIFE-100 groups I-X (D44-D57), NOVEL N01-N25 and N51-N75, Foundations F01-F25.
- Repo mapping: TSOT D01-D10, D18, D19 and SUPER D21-D23, D25-D34 name a `tsotchke/...` repo in their provenance line. Every named repo is present under `T:\mirrors` (eshkol under `T:\Eshkol\eshkol_repo`); I listed the folder, I did not open the repos.

Tail of the parse run:
```
tsotchke TSOTCHKE_1000_BIOLOGIC_SUBSTRATES_REPORT.md
  rows 1000 unique ids 1000 range 1 1000 contiguous True
  domains 20 row counts [50] base counts [10] base total 200
  bad lines []
```
Tail of the build run:
```
wrote N:/docs/requirements/thousand_reports.json items 150
wrote N:/docs/requirements/thousand_reports_index.json items 900
by report {'ALIFE': 104, 'TSOT': 27, 'SUPER': 19}
```

## Written

- `N:/docs/requirements/thousand_reports.json`: `taxonomy` (per report: domains, codes, id ranges, row and base counts, variant counts, provenance) plus 150 items: creature diversity 65 (disposition 10, mating 15, social 16, ecological role 24), world substances and hazards 45 (food 8, waste 5, conservation 3, phase of matter 7, toxicity 4, weather 9, hazard 9), substrate mappings 40 (32 Tsotchke repo, 8 lab source).
- `N:/docs/requirements/thousand_reports_index.json`: all 900 base mechanisms, same item shape.
- Id scheme: `ALIFE-2801` = base mechanism whose first row is #2801; `TSOT-D16` = domain-level item. `source_ref` carries file, line range and row range.
- Scratch: `S:/req/thousand-reports/` (parse.py, stats.py, build.py, look.py, parsed.json, base_index.json).

## Not found / limits

- No pass rules. `criterion` is filled for 13 items only, where the name itself states a threshold or rule (for example ledger error < 1e-9, I(Signal; Referent) > 0.5 bits, 5+ generations); the other 137 are "".
- NOVEL-100 N26-N50 and N76-N100 and Foundations F26-F100 are not enumerated as rows (N36, N37, N42, N46, N50, N100 appear only in the summary). "Groups I-XI" is claimed once; no Group XI rows exist.
- Thin for a 3D world: no phases-of-matter model (solid/liquid/gas/plasma), no weather system, no food or waste catalogue. Closest are 5 metabolic pools, urea, heat, corpse detritus, one toxin row, temperature/light dials, NO/CO/H2S gas rows and Gray-Scott u/v.
- Diversity axes are named mechanisms, not enumerated value sets (no list of mating systems, diets or trophic roles). One personality vector is listed: Bold, Curious, Reactive, Vuln, Hedge, RiskScar.
- SUPER D24 and D35-D40 and TSOT D11-D17, D20 cite the TypeScript lab or generic "engines", not a Tsotchke repo.
- Conflicts with the owner's laws to resolve before tracing: the reports assume JS/WebGL2/60 FPS substrates, claim "quantum-sourced" mutation and Bell/Orch-OR effects (Law 7), use timer-jitter entropy (#0471, Law 6) and consciousness wording. Treat them as a naming taxonomy, not as specifications.
