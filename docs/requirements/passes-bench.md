# passes-bench: the "100 passes" and the "100 benchmarks"

Extracted 2026-11-17 by `S:/req/passes-bench/build.py` (tables via `tables.py`, PyMuPDF 1.26.5). Sources were only read.

## What was found

**100 passes: found, exactly 100, but in two numberings that do not line up.**
- `passes100.json` (100, ids `PASS-001..PASS-100`): the owner's ledger `V:/creatures/001/PASS-100-LEDGER.md` L26-125 (same table as `W:/Asmodensius-Creature1-Lists-and-100-Passes.pdf` Part I, pp. 2-8; all 100 rows located, implementation token matches 100/100). `text` = target item + weakness fixed, `criterion` = the "Lesion / Control & Verification" cell, `extra.implementation` = file:line.
- `passes100_suite.json` (100, ids = suite test ids): the executed tests in `V:/creatures/001/organism/passes_100_suite.ts`, with the literal pass expression (`ok := ...`) plus the global rule "First-Law ledger error < 1e-11" (L69), and the recorded result from `passes_100_v40.json` (100/100 ok, worst ledger 3.62e-15).
- **Mismatch to carry into the matrix:** ledger Pass N and suite pass N are different items in waves 1-2 (ledger 001 is "Osmoregulation & ADH" while suite pass 001 is the CQM10 satiation brake, which the ledger lists as Pass 013) and are shifted by one for passes 22-25. Only 46/100 same-number pairs share an explicit item code. Each item carries its same-number counterpart in `extra`; no mapping was guessed.

**100 benchmarks: no list with that name or size exists.** `W:/Creature1-Benchmarks.pdf` (36 pp., identical to the copy in `V:/creatures/001/`) holds nine goal lists. All were extracted:

| file | items | content |
|---|---|---|
| `benchmarks100.json` | 109 | scored tests: Butlin 14, frameworks 10, CQM 25, R variants 14, A-Life capabilities 12, DELARP qualities 34 |
| `bench_freak100.json` | 100 | FREAK-100 ideas (15 built-not-tested, 85 not yet) |
| `bench_life100.json` | 500 | LIFE-100, 100 areas x 5 attributes (202 / 27 / 106 / 165, equals the PDF totals) |
| `bench_novel100.json` | 100 | N01..N100 (2 built-not-tested, 98 not yet; 8 starred) |
| `bench_foundations.json` | 99 | F01..F100 without F49 (7 done, 38 partial, 54 not yet) |
| `bench_budgets.json` | 20 | `V:/docs/BUDGETS.md`: the only benchmarks with real units, targets and hard limits |

Per benchmark, `extra.measured / unit / target` are filled. For Butlin, frameworks, CQM and R variants the pass rule is not in the PDF (it says so on p. 2); it was taken from `V:/creatures/001/preregistrations/OFFICIAL-R21.md` (d >= 0.8 and Welch t >= 2.75 against every control, 48 seeds 1064-1111, non-ceiling rule) with each item's control, and the arm-A result from `results/official-r21/SCORECARD.md`. PDF status and scorecard status agree for all 49 items (9/14, 6/10, 17/25). Combo A (11 re-runs) is attached to the matching items as `extra.combo_a_result`.

## Not found / limits

- No per-test numeric criterion for the 12 capabilities, the 34 DELARP qualities ("not yet scored"), FREAK, LIFE, NOVEL or Foundations: `criterion` is `""` there.
- `V:/benchmark` is the 150-project comparison survey (projects, URLs, 12-capability matrix), not a benchmark list; only the 12 capabilities were taken. `V:/docs/REPORT.md` is the Grade-A inspection report (I-/T-/D- rows), not passes or benchmarks; not extracted here.
- F49 is absent from the PDF (99 Foundations). The PDF stars only 8 N items although it calls the star "top-20".
- Table cells wrapped by the PDF were re-joined with spaces; a word split mid-token by the layout can survive in a `text` (framework keys were repaired; ids were checked for whitespace).
- Overlap with other agents' files in this folder: `passes100_v40.json`, `butlin14.json`, `consciousness10.json`, `cqm25*.json`, `life100_*.json` cover some of the same lists from other sources; ids here are the PDF's own.
- All statuses are the owner's recorded statuses for the TypeScript lab. Nothing was re-run; indicator items are indicators only.

## Evidence (ran now)

`python -I S:/req/passes-bench/build.py` (tail):
```
ledger rows located in PDF Part I: 100 pages [2, 3, 4, 5, 6, 7, 8]
ledger MD vs PDF implementation-token agreement: 100 / 100
WROTE passes100 100
WROTE passes100_suite 100
combo A rows 11 attached 11
WROTE benchmarks100 109
WROTE bench_freak100 100
LIFE areas with != 5 attributes: []
WROTE bench_life100 500
NOVEL count 100 missing []
FOUND count 99 missing [49]
WROTE bench_budgets 20
```
Lint over the eight files: no duplicate ids, no ids with whitespace, no empty text or source_ref.
