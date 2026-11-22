# Engine issue register

| State | Severity | Rows |
|---|---|---:|
| CLOSED | S0 | 9 |
| CLOSED | S1 | 3 |
| CLOSED | S2 | 3 |
| DEFERRED-TR | S2 | 17 |
| FIXED | S1 | 1 |
| FIXING | S0 | 1 |
| FIXING | S1 | 2 |
| NEEDS-OWNER | D | 18 |
| NEW | D | 11 |
| NEW | S0 | 1 |
| NEW | S1 | 49 |
| NEW | S2 | 78 |
| NEW | S3 | 25 |
| REPRODUCED | S0 | 1 |
| REPRODUCED | S2 | 1 |
| TRIAGED | S2 | 1 |

## BS

| ID | State | Milestone | Title | Source |
|---|---|---|---|---|
| BS-01 | NEW | M7 | Measurement validity and Goodhart risk | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:597 |
| BS-02 | NEW | M7 | Tests that cannot fail | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:610 |
| BS-03 | NEW | M7 | Oracles that copy the implementation | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:623 |
| BS-04 | NEW | M8 | Lab against wild divergence | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:637 |
| BS-05 | NEW | now | Seed leakage and tuning on test seeds | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:650 |
| BS-06 | NEW | M8 | Ceiling, floor and zero-variance measures | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:663 |
| BS-07 | NEW | M8 | Open-endedness is not measured | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:673 |
| BS-08 | NEW | M7 | Long-run stability and counter overflow | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:686 |
| BS-09 | NEW | M6 | Memory pressure and lock limits | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:701 |
| BS-10 | NEW | M7 | Windows-only assumptions | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:713 |
| BS-11 | NEW | M7 | Numerical determinism across compilers and CPUs | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:723 |
| BS-12 | NEW | M7 | Races hidden by the determinism checks | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:733 |
| BS-13 | NEW | M6 | Security of the local server | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:743 |
| BS-14 | NEW | M7 | Supply chain | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:750 |
| BS-15 | NEW | M7 | Documentation drift | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:760 |
| BS-16 | NEW | M10 | The owner's expectations against what the science can show | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:768 |
| BS-17 | NEW | now | Agent-process blindspots | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:782 |
| BS-18 | NEW | M5 | Format evolution and saved data | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:795 |
| BS-19 | NEW | M6 | Event and ledger completeness at scale | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:803 |
| BS-20 | NEW | M7 | What the state hash does not cover | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:810 |
| BS-21 | NEW | M8 | Refusals shape the evidence | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:818 |
| BS-22 | NEW | now | Concurrent use of this machine | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:826 |
| BS-23 | NEW | now | The agent's own harness (sandbox, approvals, network, child processes) | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:833 |

## DEMO

| ID | State | Milestone | Title | Source |
|---|---|---|---|---|
| DEMO-01 | NEW | now | The owner expected speed and sees 2 to 6 pictures per second in small... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:445 |
| DEMO-02 | NEW | M6 | Nobody has split the 0.1 to 0.5 s per picture into its parts; fixing the... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:446 |
| DEMO-03 | NEW | M6 | Display is coupled to the tick loop: while a picture is drawn and written,... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:447 |
| DEMO-04 | NEW | M6 | PNG on disk plus 250 ms polling is the slowest possible transport. | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:448 |
| DEMO-05 | NEW | M6 | One CPU thread draws every body, many as full silhouettes | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:449 |
| DEMO-06 | NEW | M6 | The picture does not show terrain, ocean, vents, temperature or gravity, so... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:450 |
| DEMO-07 | NEW | M6 | Two full state hashes per picture cost time that grows with the world | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:451 |
| DEMO-08 | NEW | M6 | The native Windows viewer is not built; the web demo is the only view. | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:452 |
| DEMO-09 | NEW | M6 | The controls name the coupling, not what the owner wants to see. | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:453 |
| DEMO-10 | NEW | M6 | Every picture writes about 1.5 MB to Z: (the Dev Drive) and deletes an old one. | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:454 |
| DEMO-11 | NEW | M6 | Python 3.9 is past its upstream end of life (October 2025; check the date on... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:455 |
| DEMO-12 | NEW | M6 | The server binds 127.0.0.1, checks Host on every request and Origin on POST,... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:456 |
| DEMO-13 | NEW | M6 | The viewer keeps shapes for 4096 clades; later clades are drawn with the... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:457 |
| DEMO-14 | NEW | M6 | az_live runs one world; `fps` is limited to 1..60 and size to 64..4096; wild... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:458 |
| DEMO-15 | NEW | M6 | The picture is drawn synchronously between ticks on the thread that ticks | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:459 |
| DEMO-16 | REPRODUCED | M5 | Fresh world creation silently reduces an unsupported founder species request | build/work/w2/rva-codex-01/p3-landed-review-owner/REPORT.md;build/work/review/p4-06/weather-worldgen/r01-01a1f530/findings.jsonl:4;build/work/review/p4-06/weather-worldgen/skeptic-01a1f542/findings.jsonl:4 |

## DOC

| ID | State | Milestone | Title | Source |
|---|---|---|---|---|
| DOC-01 | NEW | M7 | 85 files fail the text rule (non-ASCII 42, tabs 34, CR 17, no final newline... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:504 |
| DOC-02 | NEW | M7 | The owner's verbatim words contain non-ASCII characters; the text rule says... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:505 |
| DOC-03 | NEW | now | **FIXED 2026-11-18 (second fixer pass):** R2 at `make_status.py:152` now... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:506 |
| DOC-04 | NEW | now | Operational documents still use drive letters: CONTINUE-BUILD.md (29 hits),... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:507 |
| DOC-05 | NEW | M5 | OWNER-WORDS has not had its full status pass since worldgen ("the full... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:508 |
| DOC-06 | NEW | now | The run discipline is written for the Claude API (Fable 5.2 prices,... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:509 |
| DOC-07 | NEW | M5 | The index said PROMPTS holds P01 to P42 (P43 exists; after the lead's merge... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:510 |
| DOC-08 | NEW | M5 | The rewording "target plus measured state" is true only on the day it was... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:511 |
| DOC-09 | NEW | M7 | The same numbers (suite counts, hashes, ms per tick, lesion counts) are... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:513 |
| DOC-10 | NEW | now | The text rule's checker skips every file the next agent will write most: the... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:512 |

## EVD

| ID | State | Milestone | Title | Source |
|---|---|---|---|---|
| EVD-01 | NEW | M8 | 14 switches show NO EFFECT: RECURRENCE, PLASTICITY, WEATHER, CHEM, SIGNALS,... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:360 |
| EVD-02 | NEW | M8 | Several EFFECT rows are close to definitional: SOCIAL removes social pairs,... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:361 |
| EVD-03 | NEW | M8 | The bootstrap interval is of the mean difference, not of d_z; milestone 8... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:362 |
| EVD-04 | NEW | M8 | The 1M reference is scale, not evolution: 20 ticks, max_generation 1, the... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:363 |
| EVD-05 | NEW | M8 | The original Grok Bot build and the Windows v40 export disagree on Combo A... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:364 |
| EVD-06 | NEW | M8 | The owner's three "100s" cannot be added together: the 100-pass ledger and... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:365 |
| EVD-07 | NEW | M8 | The native engine has no held-out score on any suite | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:366 |
| EVD-08 | NEW | M8 | No selection result yet: a display preference raised the first-generation... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:367 |
| EVD-09 | NEW | M8 | The chain gradient -> body-frame receptor -> motion through the real medium... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:368 |
| EVD-10 | NEW | now | Seed custody: (a) reporting block 5101-5124 was used once for the stub... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:369 |
| EVD-11 | NEW | M8 | The 500 checklist runs on 48 native reporting seeds where the framework asks... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:370 |
| EVD-12 | NEW | M8 | No `docs\prereg\`, no `docs\eval\`, no `tools\az_suite.c` exist; per-item... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:371 |
| EVD-13 | NEW | M7 | The "differs 24/24" column compares one terminal digest of selected state;... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:372 |
| EVD-14 | NEW | now | The GPT Astra analysis (comparative review, 958-record crosswalk,... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:373 |
| EVD-15 | FIXED | M5 | Two chemistry mutation definitions fail before compilation | build/work/review/p4-06/metab-chem/r01/findings.jsonl:4;build/work/review/p4-06/metab-chem/r02-ledger-skeptic/findings.jsonl:4 |
| EVD-16 | FIXING | M5 | The documented metabolism mutation command has no module definition | build/work/review/p4-06/metab-chem/r01/findings.jsonl:5;build/work/review/p4-06/metab-chem/r02-ledger-skeptic/findings.jsonl:5 |

## GPU

| ID | State | Milestone | Title | Source |
|---|---|---|---|---|
| GPU-01 | NEW | M9 | The PTX GPU component is verified standalone (Philox and dense brain kernels... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:465 |
| GPU-02 | NEW | M9 | The GPU twin is the dense batch kernel; the engine's brain is the... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:466 |
| GPU-03 | NEW | M9 | PTX is built by clang and loaded by the driver; without ptxas, nvdisasm and... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:467 |
| GPU-04 | NEW | M9 | NPU: only a probe through Level Zero or DirectML is planned; no NPU result... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:468 |

## LNT

| ID | State | Milestone | Title | Source |
|---|---|---|---|---|
| LNT-01 | NEW | M5 | Nine traits have no read in their designated module: DENSITY, DRAG, THRUST... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:300 |
| LNT-02 | NEW | M5 | GRAVITY is honoured by chem (settling) but not by physics: bodies have no... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:301 |
| LNT-03 | NEW | M5 | Lesion bits RECURRENCE, NOISE and SENSES have no test that the checker... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:302 |
| LNT-04 | NEW | M5 | ctest is green while strict lint fails; "green" is read as "done" by people... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:303 |
| LNT-05 | NEW | M7 | W1 counts a trait as read when its owner file names it or reads the phen... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:304 |

## MOD

| ID | State | Milestone | Title | Source |
|---|---|---|---|---|
| MOD-01 | NEW | M5 | With the stub metabolism most newborns starved (5,676 of 5,759 deaths in the... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:324 |
| MOD-02 | NEW | M5 | Ornaments cost nothing to carry until metab charges for them; choice is by... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:325 |
| MOD-03 | NEW | M8 | Learning reaches behaviour only through gain, bias, context and arousal; the... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:326 |
| MOD-04 | NEW | M5 | No world-level result that any metabolic trait is selected (the thermal... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:327 |
| MOD-05 | NEW | M5 | No world-level result for cooperation or punishment; PAYOFF has no column... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:328 |
| MOD-06 | NEW | M5 | Only two of four goods exist; no creature senses a price; deposits to and... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:329 |
| MOD-07 | NEW | M5 | No result that a colony does better than its members alone; only signal... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:330 |
| MOD-08 | NEW | M5 | Cells where bodies die get hot for tens of ticks, up to 32767 (the top of... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:331 |
| MOD-09 | NEW | M7 | When the quarter binds, the sides of a cell are served in a fixed order (-x... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:332 |
| MOD-11 | REPRODUCED | M5 | Diffusion through one open face can exceed the specified eighth of stock | build/work/review/p4-06/metab-chem/r01/findings.jsonl:2;build/work/review/p4-06/metab-chem/r02-ledger-skeptic/findings.jsonl:2 |
| MOD-12 | FIXING | M5 | Shedding a body part depends on an unregistered motor threshold | build/work/review/p4-06/metab-chem/r01/findings.jsonl:3;build/work/review/p4-06/metab-chem/r02-ledger-skeptic/findings.jsonl:3 |
| MOD-13 | FIXING | M5 | Colony recruitment uses a fixed lottery weight outside the parameter table | build/work/review/p4-06/social-econ-colony/review-01a1f52b/findings.jsonl:8;build/work/review/p4-06/social-econ-colony/skeptic-01a1f53e/findings.jsonl:8 |
| MOD-10 | CLOSED | M5 | A body part can withdraw more biomass than its donor holds | build/work/review/p4-06/metab-chem/r01/findings.jsonl:1;build/work/review/p4-06/metab-chem/r02-ledger-skeptic/findings.jsonl:1 |

## OW

| ID | State | Milestone | Title | Source |
|---|---|---|---|---|
| OW-01 | NEW | M5 | Sense, physics; signal response (MOD-05) | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:548 |
| OW-02 | NEW | M5 | Variance columns (stats) | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:549 |
| OW-03 | NEW | M5 | Colony and ecosystem statistics | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:550 |
| OW-04 | NEW | M5 | NOISE channel from wild entropy in sense; IMPROV motor in physics | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:551 |
| OW-05 | NEW | M5 | Default table sizes 4096 (WG-11), worldgen fixed numbers and structure... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:552 |
| OW-06 | NEW | M5 | The physics module | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:553 |
| OW-07 | NEW | M8 | M8 suites | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:554 |
| OW-08 | NEW | M5 | Measured archetypes and look classes | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:555 |
| OW-09 | NEW | M7 | M7 | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:556 |
| OW-10 | NEW | M5 | Measured archetypes | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:557 |
| OW-11 | NEW | M5 | Bodies do not feel gravity | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:558 |
| OW-12 | NEW | M6 | Track M research note M1 | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:559 |
| OW-13 | NEW | M9 | M9 | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:560 |
| OW-14 | NEW | M6 | Recorded 2026-11-18: PROMPTS.md P51 (`:970-976`); OWNER-WORDS section 5 | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:561 |
| OW-15 | NEW | now | FIXED 2026-11-18 (`OWNER-WORDS.md:116`: the letters are gone, P43 wins) | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:562 |

## OWN

| ID | State | Milestone | Title | Source |
|---|---|---|---|---|
| OWN-01 | NEEDS-OWNER | now | D1(a): the first git commit as a baseline (REP-01) | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:522 |
| OWN-02 | NEEDS-OWNER | now | D1(b): a licence | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:523 |
| OWN-03 | NEEDS-OWNER | now | D1(c): the "Lock pages in memory" privilege for 2 MB pages (TR-03) | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:524 |
| OWN-04 | NEEDS-OWNER | now | D1(d): installs (CUDA toolkit, Windows Performance Analyzer, OpenVINO or... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:525 |
| OWN-05 | NEEDS-OWNER | now | Law-2 wording for phase 2 (DDR5 as a designed tier, GPU, NPU) | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:526 |
| OWN-06 | NEEDS-OWNER | now | A quiet-machine window for M2 and E3 timings | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:527 |
| OWN-07 | NEEDS-OWNER | now | Worldgen fixed structure and fixed shape numbers (WG-06, WG-07) | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:528 |
| OWN-08 | NEEDS-OWNER | now | Lead decisions in worldgen (WG-08) | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:529 |
| OWN-09 | NEEDS-OWNER | now | new: the target display frame rate for the demo and native viewer (DEMO-01... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:530 |
| OWN-10 | NEEDS-OWNER | now | new: a budget rule for the new agent (DOC-06) | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:531 |
| OWN-11 | NEEDS-OWNER | now | new: the non-ASCII verbatim owner files (DOC-02) | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:532 |
| OWN-12 | NEEDS-OWNER | now | new: investigate Grok against v40 Combo A (EVD-05) | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:533 |
| OWN-13 | NEEDS-OWNER | now | The 500 checklist on 48 seeds instead of 64 (EVD-11) | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:534 |
| OWN-14 | NEEDS-OWNER | now | v09: sex dimorphism from the sex locus | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:535 |
| OWN-15 | NEEDS-OWNER | now | The NPU path (GPU-04) | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:536 |
| OWN-16 | NEEDS-OWNER | now | new: an off-volume backup (REP-02) | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:537 |
| OWN-17 | NEEDS-OWNER | now | new: disabling the old launcher (REP-05) | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:538 |
| OWN-18 | NEEDS-OWNER | now | new: empty habitat bands in some worlds (WG-05) | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:539 |

## PRF

| ID | State | Milestone | Title | Source |
|---|---|---|---|---|
| PRF-01 | NEW | M6 | 100k at 8 workers: 19.24 ms/tick at the ABI-4 freeze before the biology,... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:379 |
| PRF-02 | NEW | M6 | 1M: 1339 ms/tick on 8 workers; tier1 633 ms, life 313, perceive 254, grid 59 | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:380 |
| PRF-03 | NEW | M6 | The trigger of TR-17 (life above 10 percent of the tick) is met at 100k and 1M | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:381 |
| PRF-04 | NEW | M6 | With the real metabolism, Tier 1 is surprised more often and asks for more... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:382 |
| PRF-05 | NEW | now | The same 100k command gave 73.7 ms/tick (idle) and 108.6 ms/tick (busy; same... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:383 |
| PRF-06 | NEW | M9 | 24 workers with E cores give 59.9 ms against 73.7 on 8 P cores; the default... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:384 |
| PRF-07 | NEW | M6 | The bulk Philox AVX2 twin is built and tested but no stage calls bulk Philox. | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:385 |
| PRF-08 | NEW | M9 | The creature store is two halves (compaction copies to the other half), so... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:386 |

## REP

| ID | State | Milestone | Title | Source |
|---|---|---|---|---|
| REP-01 | NEW | now | The git repository has never had a commit | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:489 |
| REP-02 | NEW | now | Every copy (tree, snapshots, logs) is on the same Z: volume | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:490 |
| REP-03 | NEW | now | The toolchain (WinLibs gcc 16.1, NASM, CMake, Ninja, Python) lives on C:,... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:491 |
| REP-04 | NEW | now | About 20 build folders were configured from the old N: letter (`build\full`,... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:492 |
| REP-05 | NEW | now | Downgraded from S0 on 2026-11-18 by the fixer pass: `$Letters =... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:493 |
| REP-06 | NEW | M7 | The tools keep escape hatches that run the world on pageable memory (with a... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:494 |
| REP-07 | NEW | M7 | Python writes bytecode caches into the source tree (`-I` does not stop it). | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:495 |
| REP-08 | NEW | now | Two agents (or an agent and the owner's desktop session) editing the same... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:498 |
| REP-09 | NEW | now | Snapshots skip `build\` except `build\work\scripts` and `build\work\logs`,... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:496 |
| REP-10 | NEW | now | No disk budget: still to come are three module staging trees with two builds... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:497 |

## RF

| ID | State | Milestone | Title | Source |
|---|---|---|---|---|
| RF-01 | NEW | M6 | At 100k (131072 cells) `grid_hist_parts` returns 1: counting, prefix and... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:268 |
| RF-02 | NEW | M6 | The field stage scans every cell four times per tick over two 8 MB columns... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:269 |
| RF-03 | NEW | now | Finding said Tier 1 was an empty stub and the tk kernels ran only in tests | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:270 |
| RF-04 | NEW | now | Generation was a u16 that wraps after 65535 | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:271 |
| RF-05 | NEW | M5 | `trait_mean[k]` is a u32 sum of up to 255 per creature; it overflows above... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:272 |
| RF-06 | NEW | M5 | Founders beyond the clade limit got clade 0 | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:273 |
| RF-07 | NEW | M9 | The contract said the state hash is equal on CPU and GPU; no tick stage runs... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:274 |
| RF-08 | NEW | M5 | The law table says every mechanism has a lesion and a test and every trait... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:275 |
| RF-09 | NEW | M6 | `az_gpu_open` uses `cuCtxCreate`, which pushes the context on the calling... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:276 |
| RF-10 | NEW | now | A skipped colony store (negative or overflowing result) left dep/taken in... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:277 |
| RF-11 | NEW | now | `az_life_kill` zeroed a negative energy store without booking it, creating... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:278 |
| RF-12 | NEW | M5 | Still present by reading: founder species clades are founded with... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:279 |
| RF-13 | NEW | M5 | `az_delta_ns` has no caller; `az_offer_u16` (the reader of the offer bytes)... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:280 |
| RF-14 | NEW | M5 | Six FIELD columns (phase, wind, charge, solid, med, temp) were never written... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:281 |
| RF-15 | NEW | M6 | The column file claims GPU upload is generated from it; no GPU upload of... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:282 |
| RF-16 | NEW | M5 | Fifteen entity columns (131 of 510 saved bytes) had no reader and no writer... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:283 |
| RF-17 | NEW | M7 | The work-splitting policy (part budget, serial fallback, debug-seed cut... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:284 |
| RF-18 | NEW | M6 | On every reorder tick the grid is built twice (reorder builds it, then the... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:285 |
| RF-19 | NEW | M7 | **RESOLVED in the stub (re-read 2026-11-18):** the file has 5 lines and only... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:286 |
| RF-20 | NEW | M6 | The GPU README lists a free-list ABI need that `core\az_abi.h` already... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:287 |
| RF-21 | NEW | M6 | `az_memset`/`az_memcpy` defined twice in gpu/ as stand-ins for the core... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:288 |
| RF-22 | NEW | M6 | The bench header says the AVX-VNNI twin "is the CPU kernel the simulation... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:289 |
| RF-23 | NEW | M6 | `job_scratch_reset` rewrites the pair record and `birth.n` of every slot... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:290 |
| RF-24 | NEW | M6 | Four driver queries ignore their `CUresult`; a failure leaves zero or empty... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:291 |

## SEAM

| ID | State | Milestone | Title | Source |
|---|---|---|---|---|
| SEAM-01 | NEW | M5 | The senses are a stand-in: directional inputs stay neutral, the PAYOFF sense... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:310 |
| SEAM-02 | NEW | M5 | Physics ignores gravity, rock and medium density: bodies can walk into... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:311 |
| SEAM-03 | NEW | M5 | `az_classify` does nothing and `az_look_class` returns 0... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:312 |
| SEAM-04 | NEW | M5 | PAYOFF should encode what the last interaction gained or lost, but no column... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:313 |
| SEAM-05 | NEW | M5 | The cross-module seam review is not done: each module was tested mostly... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:314 |
| SEAM-06 | NEW | M5 | Nobody but the author has checked life, mate, tier1, metab, social, econ,... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:315 |
| SEAM-07 | CLOSED | M5 | Signed pending cell heat loses booked energy during ordinary native ticks | build/work/w2/rva-codex-01/thermal-seam-fix/public-v6.log |
| SEAM-08 | CLOSED | M5 | Funded matter transfers wrap the signed pending amount for every species | build/work/w2/rva-codex-01/matter-seam-oracle/red.json |
| SEAM-09 | CLOSED | M5 | Valid vent pulses silently suppress heat input when the product exceeds 2^30 | build/work/w2/rva-codex-01/thermal-seam-oracle/r8-vent.json |
| SEAM-10 | CLOSED | M5 | Large legal local reaction products wrap before the field transfer | build/work/w2/rva-codex-01/thermal-seam-oracle/r8-decay.json |
| SEAM-11 | CLOSED | M5 | Reference chemistry silently stops processing the top 15 legal stocks | build/work/w2/rva-codex-01/thermal-seam-oracle/v7-stub-decay.json |
| SEAM-12 | CLOSED | M5 | Funded incoming species surplus repeatedly signals a withdrawal error | build/work/w2/rva-codex-01/thermal-seam-oracle/v8-species-cap.json |
| SEAM-13 | CLOSED | M5 | Large matter transfers transport too little heat across a cell face | build/work/w2/rva-codex-01/chem-heat-audit/native.json |
| SEAM-14 | CLOSED | M5 | Aggregate uptake demand wraps and grants more than a cell contains | build/work/w2/rva-codex-01/field-grant-audit/full-api.log |
| SEAM-15 | CLOSED | M5 | Reference field wraps a valid mark deposit before byte saturation | build/work/w2/rva-codex-01/stub-mark-fix/old-stub.log |
| SEAM-16 | CLOSED | M5 | Queued mark strengths wrap before the field clamps to a byte | build/work/w2/rva-codex-01/canonical-p1-integrated/review-books/native.json |
| SEAM-17 | CLOSED | M5 | Funded colony pantry deposits lose matter and energy above a 32-bit sum | build/work/w2/rva-codex-01/canonical-p1-integrated/review-books/cli.json |
| SEAM-18 | CLOSED | M5 | Nearby targets at equal bearings produce inconsistent AHEAD channels | build/work/w2/rva-codex-01/canonical-p1-integrated/review-behavior/bearing.json |
| SEAM-19 | CLOSED | M5 | Large brood refunds and death deposits can reverse sign or drop matter | build/work/w2/rva-codex-01/p2-final-review-books/wide-native.log |
| SEAM-20 | CLOSED | M5 | A wide funded birth can misallocate provision, body mass or energy | build/work/w2/rva-codex-01/p2-final-review-books/birth-native.log |

## TR

| ID | State | Milestone | Title | Source |
|---|---|---|---|---|
| TR-17 | TRIAGED | M6 | Parallel birth commit | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:588 |
| TR-01 | DEFERRED-TR | M9 | S1: locked DDR5 as a designed streaming tier with prefetch | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:572 |
| TR-02 | DEFERRED-TR | M9 | Populations above 1,000,000 | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:573 |
| TR-03 | DEFERRED-TR | M9 | 2 MB pages | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:574 |
| TR-04 | DEFERRED-TR | M9 | S2: GPU whole-tick path (closes v76) | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:575 |
| TR-05 | DEFERRED-TR | M9 | GPU-resident world, CPU+GPU split | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:576 |
| TR-06 | DEFERRED-TR | M9 | S3: NPU probe | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:577 |
| TR-07 | DEFERRED-TR | M9 | S4: placement scheduler | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:578 |
| TR-08 | DEFERRED-TR | M9 | R2: audit of host, GPU and NPU code | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:579 |
| TR-09 | DEFERRED-TR | M8 | Lesion masks beyond 21 bits | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:580 |
| TR-10 | DEFERRED-TR | M9 | Installs (CUDA toolkit, WPA, OpenVINO) | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:581 |
| TR-11 | DEFERRED-TR | M6 | Larger ledger event budget | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:582 |
| TR-12 | DEFERRED-TR | M10 | Workers on SMT siblings | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:583 |
| TR-13 | DEFERRED-TR | M6 | Bulk Philox through the dispatch slot | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:584 |
| TR-14 | DEFERRED-TR | M9 | Pack the store in place | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:585 |
| TR-15 | DEFERRED-TR | M6 | Shared brains (copy on write) | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:586 |
| TR-16 | DEFERRED-TR | M6 | Heritable thinking period | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:587 |
| TR-18 | DEFERRED-TR | M9 | GPU twin of the tile brain | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:589 |

## VER

| ID | State | Milestone | Title | Source |
|---|---|---|---|---|
| VER-01 | NEW | M7 | The five whole-system negative mutants are not built: Leaky (a tiny matter... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:474 |
| VER-02 | NEW | now | The second-compiler build (clang 21.1.8 at `Z:\Tools\LLVM-21`) and the trap... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:475 |
| VER-03 | NEW | now | `m_check.ps1` now carries `--store-units` but was not run again; before the... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:476 |
| VER-04 | NEW | now | No ctest gate compares the reference worlds with the recorded hashes | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:477 |
| VER-05 | NEW | now | The engine matrix checks that all configurations equal the 1-worker run of... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:478 |
| VER-06 | NEW | M5 | The lesion table was re-run after worldgen, but not after every module; it... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:479 |
| VER-07 | NEW | M5 | Snapshots saved before worldgen no longer load (the parameter table grew) | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:480 |
| VER-08 | NEW | M7 | Planted-defect campaigns are documented for worldgen (31 of 31 caught) and... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:481 |
| VER-09 | NEW | M7 | Several tests moved to `az_test_uniform` (the old uniform world) so that... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:482 |
| VER-10 | NEW | M5 | The determinism matrix in CONTINUE-BUILD (threads 1, 2, 5, 8, 12, 16, 24;... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:483 |

## WG

| ID | State | Milestone | Title | Source |
|---|---|---|---|---|
| WG-01 | NEW | M5 | Founder bodies are built from the species' base genome; no ABI call rebuilds... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:341 |
| WG-02 | NEW | M5 | The physics stub ignores gravity, rock and density, so bodies can walk into... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:342 |
| WG-03 | NEW | M5 | Weather relaxes the depth and latitude temperature gradients over time, so... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:343 |
| WG-04 | NEW | M5 | Crowds of founders boil a few ocean cells (at SURFACE_SOLID 16: 1-12 of 795... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:344 |
| WG-05 | NEW | M5 | Some worlds have no founder in the air (4 of 20 worlds at 16 species; 6 of 8... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:345 |
| WG-06 | NEW | M5 | Shape numbers stay fixed: octave doubling, the linear fade of the rock's... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:346 |
| WG-07 | NEW | M5 | Fixed structure: the ocean is always under the air, gravity always points to... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:347 |
| WG-08 | NEW | M5 | Lead decisions accepted without the owner: WORLDGEN_W_REST (the 46th... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:348 |
| WG-09 | NEW | M6 | The cost of worldgen at 100k and 1M was never measured on a labelled idle... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:349 |
| WG-10 | NEW | M5 | With trait variation on (VARY 3) 241 of 16,000 founders sit outside their... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:350 |
| WG-11 | NEW | M5 | Default table sizes bind in both reference worlds: clades default 4096... | docs/handoff/next-agent/09-ISSUES-GAPS-BLINDSPOTS.md:351 |
