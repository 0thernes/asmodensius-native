# butlin: consciousness-indicator requirement lists (2026-11-17)

Indicator only. Nothing here is a consciousness claim (V:/AGENTS.md:22).

## Written
- `N:/docs/requirements/butlin14.json` - 14 items: RPT-1, RPT-2, GWT-1..4, HOT-1..4, AST-1, PP-1, AE-1, AE-2.
- `N:/docs/requirements/consciousness10.json` - 10 items: thaler9, iit4, activeInference, attentionSchema,
  fieldIntegration, ual, sensorimotor, projective, ctm, butlin14 (a rollup: passes only at 14/14).

Each item has the five required keys plus extra keys for the matrix: `lab_test_key`, `lab_controls`, `lab_ref`,
`harness_fn`, `harness_metric`, `harness_controls`, `harness_ref`, `lab_instrument`, `r_variants`, `status_r21`,
`quality_id`, `template_criterion`, `template_ref` (and `template_id` F1..F10 in the second file).

## Where the lists live
- Owner wording and ids: `V:/creatures/001/Creature1-Benchmarks.pdf` p.3 (Butlin 14) and p.4 ("The 10 consciousness
  theories (frameworks)"). `W:/Creature1-Benchmarks.pdf` is byte-identical (sha256 d078c43babc67425... both).
- Pass rule and controls: `V:/creatures/001/preregistrations/OFFICIAL-R21.md:21-29` (d >= 0.8 and Welch t >= 2.75
  vs every control, non-ceiling rule, 48 seeds).
- How the lab measures each: organism test keys in `organism/tests_v31.ts:26-89`, mapped to ids in
  `organism/summarize_v39.ts:10-21,36,41` (RPT1 RPT2 GWT1-4 HOT1-4 AST1 PP1 AE1 AE2; NOVEL IIT AINF ASO FIELD UAL SM
  PROJ CTM). Older stand-alone toy tests: `harness/harness.ts:47-320`. Lab instruments (4 rows only: pci,
  workspaceRouting, lesionStats, stateDecoder): `V:/lab/src/instruments/scorecard.ts`.
- Official R21 status (9/14 and 6/10): `V:/creatures/001/HANDOFF-CLAUDE-FABLE.md:82-124`.
- Stricter v3.0 criteria for the same ids: `W:/ALife-Build-Framework-Template.md:476-489` (Suite 1) and `:495-504`
  (Suite 2, F1..F10). Quality re-mapping (Q-RECUR ... Q-BODY): `V:/creatures/001/DELARP-PLAN.md:147-178`.

## Things the matrix must know
- Two different pass rules exist for the same ids: the lab's d/t rule and the Template's paired delta + Wilcoxon +
  Holm with per-item thresholds. The JSON `criterion` is the lab rule; `template_criterion` is the other.
- "10 consciousness" has three other shapes, recorded under `alternate_lists` in consciousness10.json: Template
  F1..F10 (one-to-one, but F2 = PCI*, F5 = phase routing, F10 = profile scorecard); the 1000-creature report
  Domain 42 (RPT, GWT, HOT, AST, PP, IIT/PCI, UAL, CTM, Thaler9, Projective/CEMI; a brainstorm, no pass rule); and a
  ten-researcher reading corpus (the only list holding both IIT and GNW). No GNW test item exists; GWT-1..4 cover it.
- Duplicates: CQM13 = HOT-2, CQM19 = RPT-1, CQM20 = GWT-3, CQM21 = iit4; ctm overlaps GWT-3.
- The owner's own audit calls several passes weak (iit4 circular, thaler9 forced shape, HOT-4 and attentionSchema
  suspicious, AE-1 doubtful). Statuses are copied, not re-run.

## Not found / not done
- No hits in `W:/Frontier AI-ALife Tesseract Lab`. `V:/pipeline` and `V:/tests` hold no indicator list.
- PDF has no line numbers; `source_ref` gives the page. GWT-4's fail reason is not stated in the sources.
- I ran no lab test. `W:/Asmodensius-Creature1-Lists-and-100-Passes.pdf` and `W:/CQM2` code (Xylos
  `BUTLIN_14_INDICATORS`) were seen but not parsed.

## Ran (now)
`python -I S:/req/butlin/validate.py` (schema, counts, id order, every file:line ref probed for its id/key):
```
butlin14 OK 14 items; keys per item: 20
consciousness10 OK 10 items; keys per item: 21
V:/creatures/001/Creature1-Benchmarks.pdf d078c43babc67425
W:/Creature1-Benchmarks.pdf d078c43babc67425
```
No MISMATCH lines were printed. Scratch scripts: `S:/req/butlin/` (dump_pdf.py, harness_extract.py, keys.py).
