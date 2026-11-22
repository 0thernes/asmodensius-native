# cqm25: the "25" lists and the gate lists (extracted 2026-11-17)

All sets were produced by `python -I S:/req/cqm25/extract.py` (ran now; read-only on `V:` and `W:`).
Tail of its output:

```
{"cqm25": 25, "cqm25_framework": 25, "cqm25_rules": 19, "cqm_scrutiny25": 25, "apex25": 25,
 "antilarp13": 13, "antilarp13_v40impl": 13, "antilarp_mutants5": 5, "gates9": 9}
```

`python -I S:/req/cqm25/verify.py` (ran now) reported `bad_refs=[] unresolved=0` for all nine files:
every cited `file:Lnnn` exists.

## What "the 25" is: there are two, plus a look-alike

| File | Count | What it is | Ids |
|---|---|---|---|
| `cqm25.json` | 25 | **CQM-25 capability law v1.0.0** (binding, ADR 0018, adopted 2026-10-22). Source: `W:/Cosmogonic Quantum Mechalogodrom/docs/alife/cqm-capability-profile.json` (`axes[0..24]`). Criterion = measures + acceptance protocol + falsifier. | the owner's stable axis ids, e.g. `reproduction_heredity` |
| `cqm25_framework.json` | 25 | **CQM-25 as Suite 3 of the ALife Build Framework v3.0** (`L3_CQM25`): same 25 constructs, creature-level wording, quantitative gate (paired Wilcoxon, Holm, m=25, 24 seeds). Source: `W:/ALife-Framework-v3.0-Master-Checklists.csv` L26-50. | `CQM01..CQM25` |
| `apex25.json` | 25 | **APEX Ultimate 25/25** suite of the v40 lab. Criterion = the exact `ok:` expression from `apex_ultimate_suite.ts`. | full check ids, e.g. `APEX_18_M004_GATE03_...` |
| `cqm25_rules.json` | 19 | 7 governance rules + 12 evaluation-protocol clauses of the CQM-25 law. | assigned here: `CQM25-RULE-n`, `CQM25-PROT-nn` |
| `cqm_scrutiny25.json` | 25 | 25-point scrutiny scorecard (2026-07-01). A scored audit, not a law; no pass rule. | assigned here: `SCRUTINY-nn` |

The crosswalk `CQMnn` <-> axis id is positional (item n of one list = item n of the other). It is
**inferred** from order and name; neither source states it. Constructs drift at 09, 14, 24 and most at 25
(`Competence-validated scale` vs `visual_embodied_scale`).

## Gate lists

| File | Count | Source |
|---|---|---|
| `antilarp13.json` | 13 | `GATE-01..GATE-13`, framework wording and criteria (CSV L927-939; same table in the Template and Clean-Slate markdown). |
| `antilarp13_v40impl.json` | 13 | The 13 gates as actually wired in the v40 lab (`apex_ultimate_suite.ts` L1084-1096). **Names and scope differ from the framework**: e.g. framework GATE-02 is zero-allocation, v40 `GATE_02` is originality + LLM-free; framework GATE-10 is ontogeny, v40 `GATE_10` is the seed firewall. |
| `antilarp_mutants5.json` | 5 | GATE-04's five negative mutants (Leaky-2e-11, Cosmetic, Oracle, Frozen, Shuffled) with the v40 catch rule each. Ids assigned here. |
| `gates9.json` | 9 | `V:/pipeline/run_all.mjs` header L3-11; ids are the owner's labels `[1/9]..[9/9]`. |

## Not found / caveats

- No list literally titled "13-Gate Anti-LARP". The phrase in the lab is "13-Gate Mutant-Catch Suite";
  "Anti-LARP" names GATE-04 (5 mutants) and principle P7. `antilarp13.json` is therefore the 13 release gates.
- Nothing about Anti-LARP or 13 gates exists in the CQM or CQM2 repos (grep, worktrees excluded).
- `W:/CQM2` has no profile JSON; its `public/alife/cqm-capability-profile.md` has the same 25 axis names (checked, 0 mismatches).
- The three CQM `masters/*.xml` files only point at the law (`<capability-standard id="cqm-25">`); they hold no item list.
- `apex25` "25/25 passed" is what the stored artifact says. I did not run the v40 pipeline.
- `gates9` criteria combine the command run by `run_all.mjs` with the status wording in `V:/docs/SUMMARY.md`;
  the per-gate numeric thresholds live inside each suite and were not extracted.
- The APEX `text` fields are the report's result sentences (they contain measured values and LaTeX), not neutral requirement prose.
