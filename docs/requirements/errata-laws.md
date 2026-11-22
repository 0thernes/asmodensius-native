# errata-laws: constraints the native build must not violate

Agent key `errata-laws`, 2026-11-17. Output: `N:/docs/requirements/laws_and_errata.json` (one set, 333 items).
Scripts and intermediate files: `S:/req/errata-laws/` (`parse_errata.py`, `parse_k.py`, `crit_*.py`, `build.py`).

## What is in the set (counts by id group)

| Group | Ids | Count | Source |
|---|---|---|---|
| Owner's laws | `LAW-1..LAW-8` | 8 | `N:/docs/agents/COMMON.md` L16-L34 (added for cross-reference; not in my source list) |
| Errata entries | `A-01..A-11, B-01..B-17, C-01..C-09, D-01..D-10, E-01..E-10, F-01..F-04, G-01..G-04, H-01..H-14, I-01..I-05, J-01` | 85 (1 Blocker, 27 Major, 53 Minor, 4 Note) | `W:/ALife-Framework-v2.0-Errata.md` |
| Part K row corrections | `K1-01..24, K2-01..30, K3-01..18` (my row numbers; the source row label is in `text`) | 72 | same, L1030-L1132 |
| "Correct as written: do not fix" | `CAW-01..17` | 17 | same, L1138-L1154 |
| "Not checked" | `NC-01..08` | 8 | same, L1158-L1165 |
| v3.0 closure matrix | `ALF-001..ALF-092, MD-A01, MD-C01, MD-F01` | 95 | `W:/ALife-v3.0-Errata-Closure-Report.md` L19-L113 |
| v40 agent rules | `AG-1.1..1.9, AG-2.1..2.4, AG-3.1..3.4` (my ids) | 17 | `V:/AGENTS.md` |
| v40 budgets | `BUD-1.1..1.7, BUD-2.1..2.4, BUD-3.1..3.9` (my ids) | 20 | `V:/docs/BUDGETS.md` |
| ADR 0001 | `ADR-0001-C1..C4, -D1..D5, -Q1..Q2` (my ids) | 11 | `V:/docs/decisions/0001-...md` (the only file in that folder) |

`criterion` is my derivation of the native C/assembly implication (DO / AVOID / TEST), not the owner's wording.
A criterion starting `N/A-ENGINE:` marks a document-only or science-wording item (89 of 333) so the matrix can
filter it. The ten "Fix these first" ranks are carried in `category` as `fix-first #n` (rank 8 also names K3; see `K3-01`).

## What matters most for the engine

- `A-01` (the only Blocker): fidelity/LOD and any governor may depend on seed and in-simulation state only; never on
  camera, viewer, wall-clock, temperature or thread count. v3.0 decision (`ALF-017`, `MD-A01`): governor is render-only, mesh fixed per experiment.
- Native laws are stricter than the sources: integer ledger residual exactly 0 replaces every `1e-12` float budget
  (`A-03`, `BUD-1.x`, `AG-2.2`); "same hash for any thread count" is stricter than E-05's single-threaded stepping.
- RNG keying (`E-08`, `ALF-018`, `ALF-025`), deterministic math (`E-05`, `ALF-016`), per-quantity fixed point (`B-15`),
  overflow flags instead of NaN checks (`E-09`), lesions as data masks (`ALF-088`, `ADR-0001-C1`), exact statistics
  with Holm family-wise control and power-sized seed counts (`C-01..C-09`, `ALF-071..074`, `ADR-0001-D3`).
- Words to keep out of code and docs: "symplectic" (`E-01`), unqualified "unconditionally stable" (`E-02`), "FDR" for
  Holm (`C-01`), "quantum" for classical simulation (`D-10`, `K1-21`), "optimal/Pareto" before measurement (`G-04`).

## What I could not find, and defects in the sources

- The closure report's Title column and its Evidence column describe different topics in many rows (example: `ALF-034`
  title is Holm/FDR wording, evidence is the thermodynamic pool list). I derived criteria from the Evidence column.
- Its header says `SYN-093..095`; the rows are `MD-A01`, `MD-C01`, `MD-F01`. It cites `G-05` and `G-06`, which do not
  exist in the errata. Fifteen errata entries are cited by no closure row: A-04, A-05, A-06, A-07, A-08, A-10, A-11,
  B-16, H-03, H-10, H-11, H-12, H-13, I-03, I-04.
- `W:/ALife-v3.0-Errata-Closure-Register.csv` (95 rows with Problem / Required_fix / Closure_checks columns) was not in
  my source list and is not parsed; it would probably resolve the mismatch above.
- No source gives native numbers: quantum size, tick budgets, held-out seed blocks for the native engine. Owner decisions.
- The errata itself says it did not test any implementation (`NC-01`); I did not verify its PDF page references, and I
  did not open `PREREG-V50-HELDOUT.json` or any v40 code.

## Evidence (ran now)

Command: `python -I S:/req/errata-laws/build.py` (after `parse_errata.py` and `parse_k.py`). Output tail:

```
total items 333
  LAW       8  (N/A-ENGINE 0)
  ERRATA   85  (N/A-ENGINE 14)
  K1       24  (N/A-ENGINE 16)
  K2       30  (N/A-ENGINE 9)
  K3       18  (N/A-ENGINE 5)
  CAW      17  (N/A-ENGINE 8)
  NC        8  (N/A-ENGINE 6)
  ALF      92  (N/A-ENGINE 18)
  MD        3  (N/A-ENGINE 0)
  AG       17  (N/A-ENGINE 5)
  BUD      20  (N/A-ENGINE 7)
  ADR      11  (N/A-ENGINE 1)
closure rows 95
default-K rows 26
severity {'Blocker': 1, 'Minor': 53, 'Major': 27, 'Note': 4}
```

The build asserts unique ids and non-empty fields; the file re-loads as JSON with 333 items. No test of any engine was run.
