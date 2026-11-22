# checklist500: the 500-item checklist, the "100", and the 10x10x10 cube

Agent key `checklist500`, 2026-11-17. Scripts and logs: `S:\req\checklist500\` (`extract.py`, `extract2.py`, `verify.py`, `run1.log`, `run2.log`, `report.json`).

## What I found, and where

| Set file (`N:\docs\requirements\`) | Rows | What it is | Source |
|---|---|---|---|
| `checklist500.json` | 500 | LIFE-100 matrix, suite `L5_LIFE500`: 100 areas x 5 sub-checks, ids `LIFE-L001-A1` .. `LIFE-L100-A5` | `W:\ALife-Framework-v3.0-Master-Checklists.csv` lines 151-650; same rows in `Master-Spec.json` `/executable_checks_825/149..648`, `Clean-Slate-Creature-Blueprint-Checklist.md` 387-886, `ALife-Build-Framework-Template.md` 645-1144 |
| `life100_areas.json` | 100 | The 100 areas (each a five-item checklist), ids `LIFE-L001` .. `LIFE-L100`, 11 Roman-numeral groups | derived from the same rows |
| `cube1000.json` | 1000 | Hybrid Cube, ids `X0Y0Z0` .. `X9Y9Z9`: X = 10 model families, Y = 10 numeric precisions, Z = 10 geometries | `W:\ALife-Hybrid-Cube-10x10x10.csv` lines 2-1001; `Master-Spec.json` `/hybrid_cube_10x10x10/cells_1000` |
| `life100_v40_500.json` | 500 | The earlier LIFE-100 from the v40 lab, with its status label per row (202 BUILT, 27 PARTIAL, 106 PLANNED, 165 MISSING) | `V:\creatures\001\LIFE-100.md` |
| `passes100_v40.json` | 100 | Creature 001 100-pass ledger, `PASS-001` .. `PASS-100` | `V:\creatures\001\PASS-100-LEDGER.md` lines 26-125 |
| `checklists_index.json` | 42 lists | Every list found, with counted rows, id range, file and line range or JSON pointer | all of the above |

- **The 500.** All 500 rows carry one uniform pass rule (kept in `criterion`): exact paired Wilcoxon `p_Holm <= 0.05` (FWER m=500), `delta_pair >= 0.33`, non-inferiority bound, 64 paired seeds, contrast "Intact vs Organ/Pathway Lesion + First-Law Guard". Every row is `☐ Not Yet`, cost tag `[O(1)]`. No per-row numeric threshold exists beyond what is inside the row text.
- **The cube really has 1000 cells.** Badges: `[OK]` 661, `[ADAPT]` 274, `[EXCL]` 45, `[OVER]` 20 (equal to the spec's own summary). The source gives a verdict and a rationale per cell, not a pass rule, so `criterion` is empty and the verdict is in `category`.
- **"100 checklists".** No artifact with that name exists (searched `W:\` top-level md/txt/csv, `W:\ALife-Framework`, `V:\`). Readings available: the 100 LIFE areas (same rows as the 500); the 100-pass ledger; and three other 100-row lists that I indexed but did not extract: `L4_FREAK100` (F-001..F-100), `L6_NOVEL100` (N01..N100), and the 100-milestone build DAG (M001..M100).
- **Two versions of the 500.** v3.0 keeps the same 100 area slots as the v40 original (32 of 100 area names identical by number, the first 26 all identical) but rewrites every sub-check: 0 of 500 v3.0 texts appear verbatim in the Creature 1 PDFs. A row-by-row mapping by position is plausible but unverified (307 of 500 pairs share a content word).
- The index also records `W:\ALife-Framework\spec\implementation-map.json`: 2340 entries keyed by Master-Spec JSON pointers (1000 cube cells, 825 checks, 100 milestones, ...), all marked `implemented` by the earlier TypeScript build. It is a ready-made key scheme for a traceability matrix; its status claims were not checked here.

## Evidence (ran now)

`cd /s/req/checklist500 && python -I extract.py` (18 PASS, 0 FAIL), tail:

```
PASS LIFE 500: csv == json == both md (text, area, group, gate, seeds) :: {}
PASS LIFE areas == 100, each with exactly 5 sub-checks
PASS cube: all 1000 distinct XiYjZk cells present
PASS cube: csv == json cells_1000 (names, badge, status, rationale) :: {}
PASS cube: badge counts == spec summary_counts :: {"[OK]": 661, "[ADAPT]": 274, "[EXCL]": 45, "[OVER]": 20}
PASS cube: md slice badges == csv badges
PASS 825 capability checks in csv and json
PROBLEMS: []
```

`python -I extract2.py`, tail:

```
PASS v40 LIFE-100.md: 500 attribute rows, no unparsed table rows :: 500 rows, odd=[]
PASS v40 LIFE-100.md: status counts == file's own coverage summary (202/27/106/165)
crosswalk: same area names by number: 32 /100; rows sharing a content word by position: 307 /500
PASS v40 PASS-100-LEDGER.md: 100 passes Pass 001..100 :: 100
index lists now: 42 | other agents' sets seen: 13
```

Each set file was reloaded after writing (`verify.py` for the first three, the end of `extract2.py` for the two v40 sets): item counts 500 / 100 / 1000 / 500 / 100, ids unique, every item has exactly the five keys.

Source hashes (SHA-256, first 16 hex): Master-Checklists.csv `80fbe1623f5997ce`, Hybrid-Cube csv `816915b946d289ab`, Master-Spec.json `15ad46d2fab9f3f5` (the copy in `W:\ALife-Framework\spec\` is byte-identical).

## What I could not find or did not do

- No list literally called "100 checklists".
- The v40 LIFE-100 has no ids in its source; `V40-LIFE-Lnnn-Ak` ids are mine (area number, row order).
- Not parsed: the `.xlsx`/`.xls` workbooks, the PDFs (only text-searched), the YAML/XML/TXT copies (only counted ids: 500 LIFE ids in each; all 1000 cell ids in XML and YAML).
- Not extracted (indexed only): the other eight suites of the 825, the 100 milestones, 13 gates, and 26 smaller lists. Other agents' set files seen in this folder are listed inside the index.

## Notes for the native engine

- Wording that assumes the old stack: `LIFE-L001-A2` and `LIFE-L017-A5` name an FP64 ledger (owner law 5 demands integers), `LIFE-L052-A1/A2` name "Tab 19" and WebGPU. Area `L040` is "Quantum-inspired internal sense" (law 7 applies to how it is labelled).
- The uniform statistical gate is a per-row experiment (64 paired seeds, lesion contrast). 500 such experiments is the real cost of this list.
- Cube axes do not match the laws one-to-one: Y includes FP8/FP16/FP32 float precisions, while law 6 asks for integer or fixed-point state; `Y8` is "FP32 / Q16.16", `Y9` (hetero + stochastic rounding) is `[OK]` in all 100 of its cells.
