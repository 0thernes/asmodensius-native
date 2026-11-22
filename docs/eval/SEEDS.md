# Native seed-use register

Audited 2026-11-19T01:52:57+00:00. This is a read-only survey. No simulated world was started by the survey. Paths below are relative to the canonical checkout. The decision for CHAIN-01 and CHAIN-02 is DEC-0048.

## Seed allocations

| Range | Use and disposition | Evidence |
|---|---|---|
| 1-49 | Development and measurement. The archived exploratory lesion comparisons used 1-24. The reference world uses 42. | build/work/logs/lesion_z_worldgen_1_24.txt:1 and build/work/logs/sense_baseline_100k.txt:5 |
| 50-99 | Exclude from confirmation. Worldgen previously tuned on the full 1-99 range, including 78 and 91-98. The scan fixture uses 51-53 and the colony fixture uses 68. | build/work/w2/worldgen/PROGRESS.md:28,53,160 and tests/test_scan.c:550 and tests/test_colony.c:515 |
| 201-208 | Mate unit-test comparisons. These seeds have already been observed. | tests/test_mate.c:681 |
| 211-214 | Metabolism unit-test temperature and reproduction measurements. These seeds have already been observed. | tests/test_metab.c:739 |
| 5001-5024 | First reporting block. Freeze the protocol and record the one reporting look per item before drawing any result. | docs/handoff/next-agent/00-MASTER-PROMPT.md:881 |
| 5101-5124 | Second reporting block. The stub-world lesion baseline used this block once as allowed at the time. Every subsequent look requires its item-level history and frozen protocol. | build/work/PROGRESS.md:25 and docs/handoff/next-agent/00-MASTER-PROMPT.md:881 |
| 5201-5224 and 5301-5324 | Proposed substitute reporting blocks. A burned item awaits a separate owner decision before use. | docs/handoff/next-agent/07-SUITES-AND-BENCHMARKS-PROTOCOL.md:286 |
| 6001-6024 | Reserve 24 CHAIN-01 confirmation seeds. First look follows the G-P4 gate and a SHA-256-pinned preregistration. No matching prior run was identified in the inspected sources. | docs/handoff/next-agent/00-MASTER-PROMPT.md:874 and the numeric-hit inspection below |
| 6101-6124 | Reserve 24 CHAIN-02 spare confirmation seeds. Use only after a recorded CHAIN-01 burn and a new preregistration. No matching prior run was identified in the inspected sources. | docs/handoff/next-agent/00-MASTER-PROMPT.md:874 and the numeric-hit inspection below |

Every range was classified on the audit date above. The individual record and test sources retain the historical dates of execution when these are known. A test that accepts an arbitrary seed is evidence of parser capability. A recorded invocation or fixture using a particular seed is evidence of use.

## Parser checks

The az_run --seed parser accepts unsigned 64-bit values (tools/az_run.c:156). The az_lesion --seeds parser accepts unsigned values and inclusive ascending ranges, limited by its number of seed slots (tools/az_lesion.c:157). Both 24-seed confirmation blocks are representable. az_lesion initializes its default selection to 5101-5124 (tools/az_lesion.c:205). Pass an explicit development block when tuning. At this inspection tools/az_chain.c was absent in the staged tree, so its parser remains unverified.

## Inspection of the full 6001-6124 interval

Scanned 119 files across tests/*.c, tools/*.c, build/work/logs and build/work/w2/*/PROGRESS.md. Checked the whole interval with integer-token boundaries, including values that occur without the word seed. The 10 matches below name observed values or counters. The audit cannot establish uses outside these paths, unlogged manual runs or every value constructed indirectly at runtime.

| File and line | Integer | Surrounding text |
|---|---:|---|
| build/work/logs/demo_owner_world_2026-11-18_f226.json:2393 | 6060 | "tick": 6060, |
| build/work/logs/demo_owner_world_2026-11-18_f226.json:2415 | 6120 | "tick": 6120, |
| build/work/logs/demo_owner_world_2026-11-18_f226.json:2729 | 6093 | "mean_energy": 6093.36, |
| build/work/logs/demo_owner_world_2026-11-18_f226.json:3895 | 6087 | "mean_energy": 6087.86, |
| build/work/logs/lesion_z_1_24.txt:17 | 6040 | 24): intact live 8613 births 6040 energy 3092.39 hash e9fb |
| build/work/logs/lesion_z_1_24.txt:23 | 6094 | 24): intact live 8631 births 6094 energy 3229.25 hash de9f |
| build/work/logs/lesion_z_worldgen_1_24.txt:20 | 6081 | 24): intact live 6671 births 6081 energy 1764.54 hash 9245 |
| build/work/logs/run_tests_z.txt:66 | 6122 | 3ea on 0, 1, 3 and 8 workers; 6122 births refused for want |
| build/work/logs/run_tests_z.txt:67 | 6105 | hatched), 4308 stage changes, 6105 deaths (by cause 0..8: 0 |
| build/work/logs/wave1_digest.txt:129 | 6035 | cues 0% (k<=3) to 5% (k=12); 6035 of 296094 sync steps rai |

## Seed reference index

587 seed references across 66 files. The index includes declarations, comments, fixture seeds and command lines. The preceding numeric scan independently checked every file for the proposed interval.

| Source | Lines with seed references |
|---|---|
| build/work/logs/demo_owner_world_2026-11-18.json | 12 |
| build/work/logs/demo_owner_world_2026-11-18_f226.json | 46 |
| build/work/logs/lesion_z_1_24.txt | 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 28, 51 |
| build/work/logs/lesion_z_worldgen_1_24.txt | 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 28, 51 |
| build/work/logs/measure_z.txt | 20 |
| build/work/logs/REVIEW_BRIEF.md | 51 |
| build/work/logs/review_detail_2.txt | 4, 18, 108, 110, 116, 117, 118, 119, 120, 122, 198, 201 |
| build/work/logs/review_result.json | 20, 115, 157, 273, 285, 383, 831, 917, 918, 1112, 1187, 1198, 1199, 1288, 1423, 1435, 1541, 1615, 1867, 1877, 1897, 1938, 2008, 2118, 2140, 3652 |
| build/work/logs/run_100k_t1.log | 5 |
| build/work/logs/run_100k_t13_r7_split99.log | 5 |
| build/work/logs/run_100k_t8_pinned.log | 5 |
| build/work/logs/run_100k_t8_twins.log | 5 |
| build/work/logs/run_100k_t8_twins_no_pin.log | 5 |
| build/work/logs/run_1M_t16_twins_no_pin_ecores.log | 5 |
| build/work/logs/run_1M_t8_twins_no_pin.log | 5 |
| build/work/logs/run_1M_t8_twins_pinned.log | 5 |
| build/work/logs/run_tests_z.txt | 7, 12, 27, 43, 44, 72, 76, 94, 95, 150 |
| build/work/logs/sense_baseline_100k.txt | 5 |
| build/work/logs/test_results_full_build.txt | 21, 22, 23, 48, 49 |
| build/work/logs/tests_full_z.txt | 21, 22, 23, 48, 49 |
| build/work/logs/tests_verify.txt | 21, 22, 23, 53, 54 |
| build/work/logs/verify_100k_t1.log | 5 |
| build/work/logs/verify_100k_t8.log | 5 |
| build/work/logs/verify_1M_twins_t16.log | 5 |
| build/work/logs/wave1_digest.txt | 18, 25, 49, 70, 126, 136, 148 |
| build/work/logs/wg_matrix.txt | 5 |
| build/work/logs/wg_ref100k_t1.txt | 5 |
| build/work/logs/wg_ref100k_t24.txt | 5 |
| build/work/logs/wg_ref100k_t8.txt | 5 |
| build/work/logs/wg_ref1m.txt | 5 |
| build/work/logs/wg_wild100k.txt | 5 |
| build/work/w2/worldgen/PROGRESS.md | 25, 28, 31, 53, 57, 58, 59, 67, 70, 77, 118, 119, 120, 145, 147, 160, 162, 179, 180, 183, 184, 208, 225, 226, 228 |
| tests/test_brain.c | 516, 518, 563, 576, 578, 641, 642, 694, 697, 722, 723, 726, 732, 737, 743, 752, 754, 773, 774 |
| tests/test_chem.c | 28, 35, 37, 311 |
| tests/test_colony.c | 25, 34, 36, 383, 388, 511, 515 |
| tests/test_core.c | 9, 27, 31, 34, 354 |
| tests/test_econ.c | 41, 43, 495, 499, 547, 551 |
| tests/test_field.c | 669, 671, 698, 700, 701, 702, 703, 704, 705, 706 |
| tests/test_genome.c | 2, 16, 17, 113, 115, 116, 299, 301, 414, 415, 455, 457, 483, 484, 694, 696, 721, 722, 804, 806, 854, 861, 863, 886, 889, 890, 900, 904, 944, 946, 947, 948, 949, 963, 965, 970, 971, 973, 975, 978, 985, 986, 987, 988, 989, 990 |
| tests/test_grid.c | 69, 74, 85, 87, 239, 241, 265, 275, 278, 284, 334, 335, 347, 350, 366, 369, 371, 389, 390, 399, 401, 507, 513, 514, 515, 516, 517, 518, 519 |
| tests/test_ledger.c | 281, 287, 292, 294, 309, 362, 373, 410, 415, 419, 428, 445, 455, 462, 530, 534, 557, 582, 589, 628 |
| tests/test_life.c | 18, 653, 654, 694, 695 |
| tests/test_mate.c | 21, 22, 141, 143, 634, 638, 677, 682, 683, 684, 687, 692, 697, 698, 699 |
| tests/test_metab.c | 24, 41, 43, 686, 690, 739, 740, 741, 746, 751, 752, 759, 760, 763, 764 |
| tests/test_protocol.c | 30, 37, 76, 240, 290, 293, 296, 299, 337, 363, 386, 436, 447, 449 |
| tests/test_rng.c | 204 |
| tests/test_scan.c | 81, 83, 496, 499, 550, 562 |
| tests/test_snapshot.c | 17, 193, 195, 211, 212, 292, 294, 330, 331, 361, 363, 458, 460, 561, 563, 683, 684, 685, 686, 687, 693, 699, 705, 726 |
| tests/test_social.c | 54, 56 |
| tests/test_statsutil.c | 9, 366 |
| tests/test_tier1.c | 36, 38, 49, 51 |
| tests/test_tk_hop.c | 11, 12, 31, 388, 392, 429, 432, 433, 434, 440, 443, 467, 721 |
| tests/test_tk_qsv.c | 552, 802, 803, 808 |
| tests/test_tools.c | 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 384 |
| tests/test_view.c | 134, 136, 138, 145, 149 |
| tests/test_weather.c | 18, 19, 24, 33, 35, 382, 399, 460 |
| tests/test_wild.c | 5, 141, 328 |
| tests/test_worldgen.c | 3, 10, 11, 20, 21, 31, 52, 55, 66, 68, 313, 314, 317, 939, 946, 1008, 1036, 1291, 1293, 1294, 1304, 1309, 1310 |
| tools/az_bench.c | 4, 105, 163, 320, 333, 361, 375, 388, 400, 465, 466, 561 |
| tools/az_budget.c | 143 |
| tools/az_ledgerdump.c | 118 |
| tools/az_lesion.c | 3, 8, 10, 12, 23, 29, 31, 33, 35, 112, 125, 133, 136, 175, 212, 251, 256, 270, 273, 348, 363, 364, 366, 367, 368, 370, 382, 392, 393, 397, 398, 403, 409, 410, 441, 463, 465 |
| tools/az_live.c | 3, 17, 18, 19, 53, 60, 64, 65, 293, 300, 316, 317, 322, 356, 445 |
| tools/az_rngcheck.c | 11, 12, 106, 111, 117, 121, 272 |
| tools/az_run.c | 4, 6, 14, 15, 17, 29, 30, 76, 77, 86, 89, 93, 94, 146, 156, 167, 185, 186, 219, 264, 273, 334, 391, 394 |
| tools/az_snap.c | 4, 14, 15, 16, 36, 43, 45, 46, 93, 101, 109, 110, 128, 183, 186 |

Binary artifacts excluded from text matching: build/work/logs/live_smoke_z/lab1/frame-00000000.png, build/work/logs/live_smoke_z/lab1/frame-00000001.png, build/work/logs/live_smoke_z/lab1/frame-00000002.png, build/work/logs/live_smoke_z/lab1/frame-00000003.png, build/work/logs/live_smoke_z/lab1/frame-00000004.png, build/work/logs/live_smoke_z/lab2/frame-00000000.png, build/work/logs/live_smoke_z/lab2/frame-00000001.png, build/work/logs/live_smoke_z/lab2/frame-00000002.png, build/work/logs/live_smoke_z/lab2/frame-00000003.png, build/work/logs/live_smoke_z/lab2/frame-00000004.png, build/work/logs/live_smoke_z/pipe/frame-00000000.png, build/work/logs/live_smoke_z/pipe/frame-00000001.png, build/work/logs/live_smoke_z/pipe/frame-00000002.png, build/work/logs/live_smoke_z/pipe.azsnap, build/work/logs/live_smoke_z/wild/frame-00000000.png, build/work/logs/live_smoke_z/wild/frame-00000001.png, build/work/logs/live_smoke_z/wild/frame-00000002.png, build/work/logs/live_smoke_z/wild/frame-00000003.png, build/work/logs/live_smoke_z/wild/frame-00000004.png, build/work/logs/live_smoke_z/wild2/frame-00000000.png, build/work/logs/live_smoke_z/wild2/frame-00000001.png, build/work/logs/live_smoke_z/wild2/frame-00000002.png, build/work/logs/live_smoke_z/wild2/frame-00000003.png, build/work/logs/live_smoke_z/wild2/frame-00000004.png.

## Pre-sense P0 requalification, 2026-11-19 UTC

The current source survey covered 129 files. Of those, 105 contained text and 24 were binary artifacts. Its 605 seed-reference lines include declarations, commands and measurements. All 28 integer-token matches from 6001 through 6124 are identified below. The 528 lesion arms used the 24 development seeds 1-24. The range allocations, users and purposes in the table above still apply.

The compressed JSON indexes every named seed line, the lines for every 1-99 integer token, all reserved-interval hits, literal CSV seed values and the SHA-256 of every inspected file. The scope covers tests/*.c, tools/*.c, the complete pre-entry build/work/logs tree and each immediate module PROGRESS.md under build/work/w2. Binary artifacts were inventoried by SHA. No confirmation invocation was found in the inspected sources. Unlogged runs and indirectly constructed values outside the inspected evidence remain unknown. [Complete source and line index](../../build/work/w2/rva-codex-01/p0-closure/current-qualification-20261119/seed-survey.json.gz).

The az_run parser accepts 0 through UINT64_MAX at tools/az_run.c:164. The az_lesion parser reads unsigned integers at tools/az_lesion.c:143, processes inclusive ranges at :157-177, and limits the number of slots at :174. Either reserved block contains 24 values. The planned az_chain has no source in the canonical tools directory at this cutoff. Its eventual parser must be checked before its first use. No reporting or confirmation run was launched for the survey.

| Source and line | Integer | Observed role |
|---|---:|---|
| build/work/logs/demo_owner_world_2026-11-18_f226.json:2393 | 6060 | simulation tick |
| build/work/logs/demo_owner_world_2026-11-18_f226.json:2415 | 6120 | simulation tick |
| build/work/logs/demo_owner_world_2026-11-18_f226.json:2729 | 6093 | mean energy |
| build/work/logs/demo_owner_world_2026-11-18_f226.json:3895 | 6087 | mean energy |
| build/work/logs/lesion_p0-literal-codex-20261119.csv:76 | 6076 | births endpoint for development seed 4 |
| build/work/logs/lesion_p0-literal-codex-20261119.csv:252 | 6123 | live endpoint for development seed 12 |
| build/work/logs/lesion_p0-literal-codex-20261119.csv:274 | 6116 | live endpoint for development seed 13 |
| build/work/logs/lesion_p0-literal-codex-20261119.csv:356 | 6052 | live endpoint for development seed 17 |
| build/work/logs/lesion_p0-literal-codex-20261119.csv:362 | 6077 | live endpoint for development seed 17 |
| build/work/logs/lesion_p0-literal-codex-20261119.csv:376 | 6081 | births endpoint for development seed 18 |
| build/work/logs/lesion_p0-literal-codex-20261119.csv:378 | 6101 | births endpoint for development seed 18 |
| build/work/logs/lesion_p0-literal-codex-20261119.csv:381 | 6041 | births endpoint for development seed 18 |
| build/work/logs/lesion_p0-literal-codex-20261119.csv:383 | 6052 | births endpoint for development seed 18 |
| build/work/logs/lesion_p0-literal-codex-20261119.csv:384 | 6089 | births endpoint for development seed 18 |
| build/work/logs/lesion_p0-literal-codex-20261119.csv:385 | 6093 | births endpoint for development seed 18 |
| build/work/logs/lesion_p0-literal-codex-20261119.csv:389 | 6022 | births endpoint for development seed 18 |
| build/work/logs/lesion_p0-literal-codex-20261119.csv:390 | 6096 | births endpoint for development seed 18 |
| build/work/logs/lesion_p0-literal-codex-20261119.csv:391 | 6037 | births endpoint for development seed 18 |
| build/work/logs/lesion_p0-literal-codex-20261119.csv:392 | 6122 | births endpoint for development seed 18 |
| build/work/logs/lesion_p0-literal-codex-20261119.csv:394 | 6002 | births endpoint for development seed 18 |
| build/work/logs/lesion_p0-literal-codex-20261119.csv:395 | 6099 | births endpoint for development seed 18 |
| build/work/logs/lesion_p0-literal-codex-20261119.txt:20 | 6081 | birth count |
| build/work/logs/lesion_z_1_24.txt:17 | 6040 | birth count |
| build/work/logs/lesion_z_1_24.txt:23 | 6094 | birth count |
| build/work/logs/lesion_z_worldgen_1_24.txt:20 | 6081 | birth count |
| build/work/logs/run_tests_z.txt:66 | 6122 | birth refusals |
| build/work/logs/run_tests_z.txt:67 | 6105 | death count |
| build/work/logs/wave1_digest.txt:129 | 6035 | synchronization counter |
