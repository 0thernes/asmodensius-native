# Preregistration and Seed-Allocation Protocol (`docs/preregistration.md`)

`Asmodensius-Native` enforces a strict separation between **development/tuning seeds** and **reserved confirmation/reporting seeds** so hypothesis tests are never contaminated by parameter tuning (Project Law 4 & Law 9; audited in [docs/eval/SEEDS.md](eval/SEEDS.md)).

## 1. Frozen Seed Allocation Table

| Seed Block | Designated Role | Current Burn Status | Audit Source |
|---|---|---|---|
| `1..49` | Development, debugging, and exploratory lesion tuning (`az_lesion --seeds 1-24`, reference seed `42`). | Observed / Development | `docs/eval/SEEDS.md` |
| `50..99` | Excluded from confirmation (historically used by worldgen, scan, and colony fixtures). | Observed / Excluded | `docs/eval/SEEDS.md` |
| `201..208`, `211..214` | Unit-test comparisons (`test_mate`, `test_metab`). | Observed / Unit Tests | `docs/eval/SEEDS.md` |
| `5001..5024` | First reporting block (24 paired seeds). | **RESERVED (Unburned for v1.1.0)** | `docs/eval/SEEDS.md` |
| `5101..5124` | Second reporting block (used once for historical pre-W2 stub baseline; requires item-level history before reuse). | **GATED / Historical Stub Look Only** | `docs/eval/SEEDS.md` |
| `6001..6024` | Primary confirmation block for `CHAIN-01` / `M5` ecological trials (24 paired seeds). | **RESERVED (0 confirmation invocations audited)** | `docs/eval/SEEDS.md` |
| `6101..6124` | Spare confirmation block (`CHAIN-02`, 24 paired seeds; only after a recorded `CHAIN-01` burn and new SHA-256 preregistration). | **RESERVED (0 confirmation invocations audited)** | `docs/eval/SEEDS.md` |

## 2. Pre-Specified Falsification Rules for Confirmatory Trials

Before executing any confirmatory run on `5001..5024` or `6001..6024`:

1. **Freeze Binary & Protocol SHA-256:** Record the exact git commit SHA, `az_run.exe` / `az_lesion.exe` SHA-256, exact CLI flags, tick horizon, primary endpoint, and multiple-comparison family prior to execution.
2. **Paired Intact-vs-Lesion Controls:** Every lesion arm pairs intact world (`--lesion 0`) against single-bit lesioned world (`--lesion <bit>`) on the exact same `--lab --seed` across all 24 seeds.
3. **Statistical Reporting Standard:** Report paired non-parametric test statistics (Wilcoxon signed-rank / exact permutation), standardized effect sizes with 95% confidence intervals, and Holm-Bonferroni / Benjamini-Hochberg multiple-comparison adjustments across the tested lesion family.
4. **Honest Null Reporting:** Null and negative effects are published verbatim alongside positive effects (no selective reporting, no switching seeds after peeking, no composite "intelligence" or "consciousness" score).

## 3. Current Pre-Confirmation Status (v1.1.0)

- Exploratory development runs on seeds `1..24` (64 ticks) verified that `23/23` lesion bits alter final state hashes and `8/23` passed prespecified short-horizon primary effect thresholds (`docs/ARCHITECTURE.md`, `docs/eval/MECHANISMS.md`).
- Full mechanism crosswalk covers `958` catalog keys across `butlin14`, `consciousness10`, `cqm25`, `passes100`, and `passes100_suite` (`docs/eval/MECHANISMS.csv`), honestly marking `648` keys as lacking an identified substrate mechanism and `74` with proposed native statistical tests.
- Reserved confirmation blocks `6001..6024` and `6101..6124` remain untouched as of v1.1.0.
