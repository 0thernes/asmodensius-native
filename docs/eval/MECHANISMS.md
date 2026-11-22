# Native mechanism inventory, SB-03 first pass

This first pass assessed two archived source trees. The pre-sense tree was
canonical at the time of assessment. The second tree was the qualified R8
sensing candidate. The native checkout incorporated sensing and subsequent
field corrections on 2026-11-19 UTC. Its sensing implementation matches the
reviewed R8 source byte for byte. Line numbers and evidence in the three CSVs
refer to the archived trees named below. [Canonical source transfer](../../build/work/w2/rva-codex-01/canonical-p1-integrated/landed.json)

Each of the 958 catalog keys has a row. Of these, 74 identify a proposed native
statistical test and 648 lack an identified mechanism for the exact source
criterion. The classifications describe work available for preregistration and
the gaps still open. This inventory used source searches and archived receipts.
[Mechanism rows](MECHANISMS.csv), [source crosswalk](MECHANISMS-crosswalk.csv), [historical relationships](MECHANISMS-historical.csv).

## Reading an item

Find the source catalog and ID in MECHANISMS.csv. The eight columns give the
candidate function, existing lesion if any, stub dependencies, numbered
source/fixture locations, proposed disposition and an operational test or
specific missing observation. "TESTED" in the *proposed disposition* column
names a future disposition, conditional on the missing arms and instruments.
An existing neighboring test can verify an input or a state transition without
qualifying the full item. For absent items the nearby code describes the
boundary of what the engine does implement. A broad switch is named as such
where it affects more than the proposed specific pathway.

MECHANISMS-crosswalk.csv retains the source wording, criterion, harvest pointer,
historical source locator, links, existing fixture and scoped R8 evidence. The
source files in docs/requirements are authoritative for their original Unicode
wording. The CSV export follows the repository's ASCII convention. Benchmark
thresholds and R21/v40 archive labels have no native verdict here.

| Catalog | Individual keys | Interpretation |
|---|---:|---|
| butlin14 | 14 | Indicator items, including explicit absence where the exact cognitive pathway has no identified substrate |
| consciousness10 | 10 | Nine theory items and the Butlin group rollup |
| cqm25 | 25 | Law axes, with the five indicator axes and engineering/measurement scope marked |
| passes100 | 100 | Historical ledger targets and controls |
| passes100_suite | 100 | Executed suite targets, assessed independently of the ledger numbering |
| checklist500 | 500 | Individual v3.0 subchecks within 100 areas |
| life100_areas | 100 | Grouping rows, five subchecks per area |
| benchmarks100 | 109 | 49 canonical cross-references, 14 historical R variants, 12 capabilities and 34 qualities |

The catalogs overlap. The 958 keys measure inventory coverage and have no
interpretation as a count of distinct capabilities or successful tests.

## Findings that change the next experiments

| Source target | Implemented pathway and existing evidence | Missing selective observation |
|---|---|---|
| RPT-1, delayed recall | Pre-sense job_perceive applies the hidden-lane recurrence mask. The sparse brain separately computes hidden and motor state. See core/az_tick.c:224, core/az_brain.c:63 and tests/test_brain.c:321. | A controlled cue-off delay, selective recurrence timing, feed-forward-matched sham and blinded focal outcome. |
| PP-1 and predictive causal models | Tier 1 subtracts a running energy-reward baseline from realized reward at core/az_tier1.c:208. | PP-1 requires a predicted next sensory input and predictor-lesioned error. The CQM law also needs forecasts under manipulated rules and a model-specific control. A reward-baseline indicator must have its own identity. |
| GWT-4 and GWT-3R | Neighborhood geometry and ATTENTION modify lane gains at core/az_tier1.c:263. GWT-3R explicitly uses a degraded input and broadcast-lesion condition in historical v3.6.md:50. | No workspace-output-to-attention edge or winner-to-consumer broadcast route was identified. The R variant's degraded-input endpoint and clean GWT-3 remain distinct. |
| UAL and LIFE-L064-A3 | Eight lane-specific eligibility values carry centered activity between Tier 1 visits. Reward uses them to alter gains and biases. See core/az_tier1.c:225 and core/az_tier1.c:283. | Clear only prior-trace carry, preserve immediate activity and the remaining learning update, and measure anticipation during a silent gap. PLASTICITY currently also changes other updates. |
| CQM autonomous goals | Current energy/part readiness and motor commands influence intents or reproduction. The brain's recurrent state is distributed. | The source requires persistent goals, distraction, and a selective goal intervention against fixed-priority/open-loop policies. A switch to foraging after an energy drain does not identify this pathway. |
| Environment-to-creature and creature-to-environment | Pre-sense EAT requests cell nutrients and living EXCRETE debits carried waste. A builder reads its actual BUILD motor and deposits a nest mark at core/az_econ.c:223. R8 supplies diet-weighted directional inputs. | Separate the impact of a relocated nutrient stock on the receiver from the impact of excretion/building on other creatures. Log the same intervention in every arm. |
| Committed social stake | R8 core/az_sense.c:239 and :240 write PAYOFF and STAKE. R8 tests/test_sense.c:63 traces an actual committed stake into another body's percept. | Vary commitment while holding initial wealth and positions fixed, shuffle the two channels independently, and test the partner's subsequent behavior. Earlier sensor bytes alone cannot answer the strategic question. |
| Cryptobiosis | Pre-sense core/az_life.c:151 can wake a dormant body. life_check at line 160 makes a discrete dormant-state decision. tests/test_life.c:561 checks this native path. | The source specifically measures graded depth, metabolic suppression, recovery and retained fat against noCrypto. The ledger adds N49 slow dreaming and warm revival. These source targets cannot be inferred from cyst entry. |
| K1R | The v40 organism/summarize_v39.ts:46 and :47 identify the original immediate classification metric. The v3.7 preregistration at line 59 and summary at line 65 define the noisy/degraded input and the two K1aR/K1cR controls. | No matching native v3.1 cortex or STDP-selective lesion exists. The historical objective is determined and can be carried forward without spending a reporting seed or invoking an owner decision. |

## Which source is being cited

At assessment time all 155 C, header and definition files under core and tests
in the pre-sense archive matched the then-canonical source byte for byte. The
corresponding R8 archive holds 158 files. Read unqualified pre-sense locations
in the first tree, and locations labeled R8 in the second. Validation records
both file inventories, the twelve input catalogs, the protocol and the four R8
receipts. [Pre-sense source](../../build/work/w2/rva-codex-01/tree),
[R8 source](../../build/work/w2/rva-codex-01/joint-p1-r8/tree),
[assessment inputs and hashes](../../build/work/w2/rva-codex-01/sb03-first/validation.json).

Pre-sense core/stubs/az_sense.c:15 populates a subset of 64 channels, including
some internal state, level signals and simple neighbor directions. The compiled
physics stub at core/stubs/az_physics.c:6 and :29 adds world-axis thrust and
push, applies drag, and updates position. Full body-frame actuation, terrain and
paid mechanical work await M5 physics. The stats stub at
core/stubs/az_stats.c:4 collects counts and limited means. az_classify at line 27
has no implementation, and az_look_class at line 32 returns a constant.

R8's implemented az_sense_cell and az_sense are in core/az_sense.c:11 and :133
of that tree. The tested candidate includes six-face local inputs, body-frame
projection, per-body TAXIS/CURVE, history, and social stake. Its preserved
focused receipt reports 5,493 checks with zero failures. The independent range
receipt reports 64,000,000 channel checks with zero violations, and the 100,000
founder coverage arm reports 8,087,023 fresh writes, 63 of 64 channels varying
in that interval, zero audit failures and hash 89f4bda3dcaf9dc2. These receipts
are reused by digest. They establish sensing and cover specific causal fixtures.
The full physics and stats implementations remain absent in R8. Positive
catalog-level learning, communication or cognitive verdicts require their own
interventions and measurements.

The ABI declares the old lesion mask at core/az_abi.h:385 and reserves two
extension words. At core/az_world.c:197 the pre-sense configuration
validator rejects nonzero unnamed extension words. R8 adds a real TAXIS switch.
Both snapshots lack the SB-04 probe implementation, logged AZ_EV_PROBE and
the new matched SHAM, SHUFFLE, HIDDEN_OFF and trace-only arms. Each listed
proposed intervention must be implemented or independently justified before
its corresponding assay.

## Crosswalk and review

The pass graph uses target_codes and codes. Of 100 ledger and 100 suite rows,
70 ledger rows and 61 suite rows contain a code, with 92 shared codes. Each
side has 47 rows with exactly one neighbor, and three ledger rows have several.
For the suite the reviewed crosswalk has 51 SELF, 31 PART-OF and 18 DUPLICATE-OF
relations. The graph includes an F64 collision between ledger Daisyworld and
suite blindsight/rivalry. Those two targets stay separate. A same-number row is
never joined without a shared code and wording check. SB-20 owns final suite
adjudication and family-size accounting.

MECHANISMS-historical.csv contains 1,125 additional trace-only rows covering
the 100 v40 ledger mirrors, the two historical 500-item LIFE wordings and the
25 CQM framework rows. Historical LIFE rows map to the v3.0 area level.
Subcheck identity does not follow from matching position. The CQM framework
position crosswalk flags wording drift at axes 09, 14, 24 and 25. Benchmark R
variants retain the section 5.6 administrative link and their own exact control,
endpoint and source disposition. Historical outcomes cannot supply an
experimental result for a revised native item.

The independent reader selected 20 keys across all eight catalogs before the
revision. The first review found that some neighboring pathways had been
attributed to more specific source criteria. The revision changed 230 primary
rows, including 179 proposed classifications. The reader checked the same 20
items again and accepted the revision at the sampled scope. Six of the 20
changed classification. [Original freeze](../../build/work/w2/rva-codex-01/sb03-first/review-v1),
[row changes](../../build/work/w2/rva-codex-01/sb03-first/review-changes.json),
[independent readback](../../build/work/w2/rva-codex-01/sb03-independent/REVIEW.md).

The original generator and validator are in the SB-03 stage. Run either with
Python's -I -B options in a working copy of that stage. The validator checks
958 source keys, 1,125 historical joins, 6,801 row-term references, source and
fixture locations, historical K1, module presence and the 20 selected items.
Search evidence records exact terms, hit counts and example lines for each
source tree. Hits include comments. Each mechanism attribution names a separate
implementation location. The final inventory is due after all M5 modules are
integrated. [Stage](../../build/work/w2/rva-codex-01/sb03-first),
[search evidence](../../build/work/w2/rva-codex-01/sb03-first/search-evidence.json),
[independent validation](../../build/work/w2/rva-codex-01/sb03-first/independent-validation.json).
