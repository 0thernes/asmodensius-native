# Additional ALife Framework v3.0 catalogs

These nine catalogs contain 491 source records. The five capability catalogs contribute 276 of the 825 checks. The remaining 215 records describe milestones, shared checks, citations and the source's errata dispositions. Source gates, controls and historical status labels are transcribed as requirements and provenance. Native verdicts require their own evidence.

| Catalog | Records | Primary sources and independent row check |
| --- | ---: | --- |
| [framework825_freak100](framework825_freak100.json) | 100 | Master CSV and JSON, checked against Astra Framework 825 |
| [framework825_novel100](framework825_novel100.json) | 100 | Master CSV and JSON, checked against Astra Framework 825 |
| [framework825_wse12](framework825_wse12.json) | 12 | Master CSV and JSON, checked against Astra Framework 825 |
| [framework825_soft40](framework825_soft40.json) | 40 | Master CSV and JSON, checked against Astra Framework 825 |
| [framework825_hw24](framework825_hw24.json) | 24 | Master CSV and JSON, checked against Astra Framework 825 |
| [framework825_milestones100](framework825_milestones100.json) | 100 | Master CSV and JSON, checked against SpreadsheetML Milestones_100 |
| [framework825_mirrors5](framework825_mirrors5.json) | 5 | Master JSON, checked against Astra Framework mirrors |
| [framework825_citations15](framework825_citations15.json) | 15 | Master JSON, checked against CITATION_15 rows of SpreadsheetML Forensic_36_Gates_13 |
| [framework825_errata95](framework825_errata95.json) | 95 | Closure CSV and Master JSON, checked against SpreadsheetML Errata_Closure_95 |

## Source comparison

The extractor checks all 825 capability IDs and all five mirror IDs against the two Astra worksheets. It resolves each workbook JSON locator to the exact array row and verifies the CSV row number, suite, title, group, specification, complete contrast and control arms, gate, historical status, cost, paired seeds, family size, prerequisites and mirror association. Only the five requested capability catalogs are written.

Astra's two worksheets contain capability and mirror records. The milestone rows are 827-926 of the master CSV. The 15 citation records are in rows 51-65 of the master SpreadsheetML forensic sheet. The 95 errata rows have a separate CSV closure register. The extractor checks those three ID sets against their named SpreadsheetML sheets and cross-checks their text against the Master JSON. The master CSV has no citation or mirror records.

Each item has five keys: id, text, category, criterion and source_ref. A criterion retains source controls, falsifiers, gate wording and associated metadata where available. For a citation it retains the framework scope and the source's resolution label. Each locator identifies a one-based logical CSV or worksheet row, counting its header, or a zero-based JSON array element. CSV parsing respects quoted embedded newlines. JSON uses ASCII escapes for the original Unicode and uses LF line endings.

## Reproduction

Run `python -I build/work/eval/scripts/extract_825.py` from the repository. The default reads the master files beside the repository and the SB-01 Astra workbook copy in `build/work/eval/inputs/astra-2026-11-18`. Add `--check` to recompute and compare the outputs in place. Input hash or row disagreement exits nonzero before publication. Identical generated files retain their modification times.

## Input SHA-256

| File | SHA-256 |
| --- | --- |
| `ALife-Framework-v3.0-Master-Checklists.csv` | `80fbe1623f5997ce1395cbe69e0baeabaaeb89e84809b8d2cb8ca7040564c389` |
| `ALife-Framework-v3.0-Master-Spec.json` | `15ad46d2fab9f3f59f904cc96f4cb0ff5644a8ea3139c5dcc04324e364adb908` |
| `ALife-v3.0-Errata-Closure-Register.csv` | `06fe8a914ee77b96916915aefffeb72ecfaa98db40aa1d0409c34956d320fd4f` |
| `ALife-Framework-v3.0-Master-Workbook.xls` | `b37670a4d8fa68761833eb51d132c658cdb5f40468ad17fc002ab4a6f8c6bb72` |
| `Asmodensius-evidence-crosswalk.xlsx` | `5a6894d1111860963b4566378b63a61322e660cacc23841270adb64f87a47e37` |
