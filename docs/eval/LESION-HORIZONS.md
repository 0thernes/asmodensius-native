# Lesion effects at 64, 512 and 2,048 ticks

These exploratory trials used the private 294-input M5 candidate, 3,000 founders and development seeds 1 through 24. Each horizon paired the same 24 intact worlds with 23 single-switch interventions. Each of the 1,728 worlds used locked memory. Every world ended with exact matter and energy and zero cumulative field underflows and dropped events. This candidate predates subsequent mate-choice and metabolic corrections.

| Ticks | Worlds | Primary effect | No primary effect | Inert |
| ---: | ---: | ---: | ---: | ---: |
| 64 | 576 | 8 | 15 | 0 |
| 512 | 576 | 8 | 15 | 0 |
| 2048 | 576 | 5 | 18 | 0 |

The primary outcome for each switch was fixed before these runs. EFFECT requires absolute paired Cohen d of at least 0.8 and a two-sided paired signed-rank p below 0.05 after Holm correction across the 23 switches at that horizon. INERT means every endpoint state hash matched its intact counterpart. NO EFFECT means at least one hash changed while the primary outcome fell short of the statistical criterion. The table gives signed differences in lesion-minus-intact order.

| Switch | Primary outcome | 64 ticks | 512 ticks | 2,048 ticks | 2,048-tick paired d | Hashes changed at 2,048 |
| --- | --- | --- | --- | --- | ---: | ---: |
| BRAIN | energy | EFFECT | EFFECT | EFFECT | 0.802 | 24/24 |
| CARE | deaths | NO EFFECT | NO EFFECT | NO EFFECT | -0.004 | 24/24 |
| CHEM | energy | NO EFFECT | EFFECT | NO EFFECT | -0.260 | 24/24 |
| COLONY | live | NO EFFECT | NO EFFECT | NO EFFECT | -0.037 | 24/24 |
| GRAVITY | energy | EFFECT | NO EFFECT | NO EFFECT | 0.075 | 24/24 |
| KIN | interactions | EFFECT | EFFECT | EFFECT | -1.056 | 24/24 |
| MATING | births | EFFECT | EFFECT | NO EFFECT | -0.696 | 24/24 |
| MEMORY | energy | NO EFFECT | NO EFFECT | NO EFFECT | 0.149 | 24/24 |
| METRIC | energy | NO EFFECT | NO EFFECT | NO EFFECT | 0.089 | 24/24 |
| MUTATION | clades | EFFECT | EFFECT | EFFECT | -1.368 | 24/24 |
| NOISE | energy | NO EFFECT | NO EFFECT | NO EFFECT | 0.177 | 24/24 |
| PLASTICITY | energy | NO EFFECT | NO EFFECT | NO EFFECT | 0.184 | 24/24 |
| RECURRENCE | energy | NO EFFECT | NO EFFECT | NO EFFECT | 0.065 | 24/24 |
| SENSES | energy | NO EFFECT | NO EFFECT | NO EFFECT | 0.194 | 24/24 |
| SIGNALS | interactions | NO EFFECT | NO EFFECT | NO EFFECT | 0.134 | 24/24 |
| SOCIAL | interactions | EFFECT | EFFECT | EFFECT | -1.477 | 24/24 |
| TAXIS | energy | NO EFFECT | NO EFFECT | NO EFFECT | 0.105 | 24/24 |
| THRUST | energy | EFFECT | NO EFFECT | NO EFFECT | 0.124 | 24/24 |
| TIER1_Q | energy | NO EFFECT | NO EFFECT | NO EFFECT | 0.079 | 24/24 |
| TIER1_SH | energy | NO EFFECT | NO EFFECT | NO EFFECT | 0.081 | 24/24 |
| TOXIN | deaths | NO EFFECT | NO EFFECT | NO EFFECT | -0.472 | 24/24 |
| TRADE | interactions | EFFECT | EFFECT | EFFECT | 0.961 | 24/24 |
| WEATHER | energy | NO EFFECT | EFFECT | NO EFFECT | 0.410 | 24/24 |

## P7 A1 mechanism follow-up

Each switch below had NO EFFECT at 2,048 ticks. The middle column records a measured native effect. The final column gives an untested explanation for the population result. P7 A1 owns these causal checks.

| Switch | Measured mechanism | Hypothesis to test |
| --- | --- | --- |
| CARE | Parental care transferred 5,000 energy units to a young creature, versus 19 when care was disabled. [Native log](../../build/work/w2/rva-codex-01/p4-reviewed-stub-compiler/C15/standalone-test_social/output.log) | Eligible relatives may account for a small share of total deaths. |
| CHEM | The chemistry fixture recorded oxidation, settling and burial, and a world with different births under the lesion. [Native log](../../build/work/w2/rva-codex-01/p4-reviewed-stub-compiler/C15/standalone-test_chem/output.log) | The quantities of accessible reactants may constrain the energy response. |
| COLONY | The intact world founded 532 colonies. Disabling colonies prevented all foundations. [Native log](../../build/work/w2/rva-codex-01/p4-reviewed-stub-compiler/C15/standalone-test_colony/output.log) | Colony membership changes how resources are distributed while births and deaths can offset in the living count. |
| GRAVITY | At 500 ticks mean organic-matter height was 2,145 under downward gravity and 3,596 with gravity disabled (height in 1/1,024 of a cell). [Native log](../../build/work/w2/rva-codex-01/p4-reviewed-stub-compiler/C15/standalone-test_chem/output.log) | Redistribution across depth may change where food is found with little consistent effect on mean energy. |
| MATING | The intact endpoint included 485 living creatures with two parents and the lesioned endpoint had zero. [Native log](../../build/work/w2/rva-codex-01/p4-reviewed-stub-compiler/C15/standalone-test_mate/output.log) | Asexual births could compensate for lost two-parent births. |
| MEMORY | Forty long-term writes cost four energy units each. The lesion fixture also requires zero learned patterns and written words. [Native log](../../build/work/w2/rva-codex-01/p4-reviewed-stub-compiler/C15/standalone-test_tier1/output.log) | Recall depends on repeated cues, and returns may offset storage costs. The independent review found a zero-recall defect. |
| METRIC | Using the metric changed the learning update on eight of eight lanes in the controlled fixture. [Native log](../../build/work/w2/rva-codex-01/p4-reviewed-stub-compiler/C15/standalone-test_tier1/output.log) | A local change in the learning step may have little consistent effect on mean energy. Angle wrap can also distort the step. |
| NOISE | check_tick_lesions compared sensory bytes by founder ID after a native tick and checked that disabling noise zeroed its channel while preserving the others. [Native log](../../build/work/w2/rva-codex-01/p4-reviewed-stub-compiler/C15/standalone-test_sense/output.log) | Different creatures can turn in opposing directions when sensory noise is removed. |
| PLASTICITY | The rewarded lane led by 40 after six or nineteen visits under two learning rates. Disabling plasticity removed that lead. A later review showed this test still passes when brain modulation is disconnected. [Native log](../../build/work/w2/rva-codex-01/p4-reviewed-stub-compiler/C15/standalone-test_tier1/output.log) | An internal update can fail to reach motion or can yield variable returns. |
| RECURRENCE | On tick two, 96 matched creatures had identical sensory inputs and different motor outputs after the recurrence intervention. [Native log](../../build/work/w2/rva-codex-01/p4-reviewed-stub-compiler/C15/standalone-test_brain/output.log) | Changed movements can produce similar energy intake across a population. |
| SENSES | In a controlled developed cohort, CURVE changed 16 receptors and 14 motor frames. Disabling external senses reduced both counts to zero. [Native log](../../build/work/w2/rva-codex-01/p4-reviewed-stub-compiler/C15/standalone-test_life/output.log) | Other sensors and broadly distributed food may sustain intake. |
| SIGNALS | One fixture sent a signal of 198 for six energy units. Disabling signals suppressed the emission. [Native log](../../build/work/w2/rva-codex-01/p4-reviewed-stub-compiler/C15/standalone-test_social/output.log) | Signal-dependent decisions may be a small fraction of all pair interactions. |
| TAXIS | The measured TAXIS differences reached ten receptors, six motor frames and two positions. The directional lesion reduced all three counts to zero. [Native log](../../build/work/w2/rva-codex-01/p4-reviewed-stub-compiler/C15/standalone-test_life/output.log) | A usable gradient, motion and accessible food must coincide to affect mean energy. |
| THRUST | Developed thrust changed signed motion and energy cost in sixteen comparisons. The masked motor changed neither. [Native log](../../build/work/w2/rva-codex-01/p4-reviewed-stub-compiler/C15/standalone-test_life/output.log) | The cost of propulsion and the energy obtained from movement can offset. |
| TIER1_Q | The native fixture checks that disabling state-vector updates preserves the state-buffer sentinel. An independent angle intervention produced 0 versus 128 odd lanes in two sets of 128 native draws. [Native log](../../build/work/w2/rva-codex-01/p4-reviewed-stub-compiler/C15/standalone-test_tier1/output.log) | Changes to the distribution of motor choices may have variable returns. |
| TIER1_SH | At attention 255, enabling the module moved the gains by 118 in total. An independent neighbor-direction fixture changed yaw from 95 to 22. [Native log](../../build/work/w2/rva-codex-01/p4-reviewed-stub-compiler/C15/standalone-test_tier1/output.log) | Directional neighbors may be sparse and their position may favor different actions in different worlds. |
| TOXIN | Carrying 100 toxin cost 400 health per tick in the controlled case and zero when toxin was disabled. [Native log](../../build/work/w2/rva-codex-01/p4-reviewed-stub-compiler/C15/standalone-test_metab/output.log) | Detoxification and competing mortality causes can reduce differences in total deaths. |
| WEATHER | Top-cell light rose from 109 to 167 intact while the weather-lesioned comparison retained 109. [Native log](../../build/work/w2/rva-codex-01/p4-reviewed-stub-compiler/C15/standalone-test_weather/output.log) | Changing light and temperature can affect metabolic returns in opposite directions. |

## Reproduction

The native tool selects lab mode internally. Each command produced a separate 576-world CSV. Independent calculations recovered the Q8 energy values, used exact signed ranks with ties, and reproduced all 69 native decisions.

~~~powershell
& "Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native\build\work\w2\rva-codex-01\p4-reviewed-integration\build\az_lesion.exe" --seeds 1-24 --n 3000 --ticks 64 --csv "Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native\build\work\w2\rva-codex-01\p4-horizon-development\64\worlds.csv"
& "Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native\build\work\w2\rva-codex-01\p4-reviewed-integration\build\az_lesion.exe" --seeds 1-24 --n 3000 --ticks 512 --csv "Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native\build\work\w2\rva-codex-01\p4-horizon-development\512\worlds.csv"
& "Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native\build\work\w2\rva-codex-01\p4-reviewed-integration\build\az_lesion.exe" --seeds 1-24 --n 3000 --ticks 2048 --csv "Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native\build\work\w2\rva-codex-01\p4-horizon-development\2048\worlds.csv"
~~~

Design SHA-256 66312f791a9d2ab8748d5c449dba4497a4bb14014ccac446561ecbd45dbb91bc. Native executable SHA-256 b6d54a01866fd9f6dbda1750724d550dfed107a2e3b1051107da3303e296c03e. The raw and adjusted probabilities, effect sizes and signed differences for every horizon are in the linked calculations.

[Pre-run design](../../build/work/w2/rva-codex-01/p4-horizon-development/design.json) | [64-tick recalculation](../../build/work/w2/rva-codex-01/p4-horizon-development/64/independent.json) | [512-tick recalculation](../../build/work/w2/rva-codex-01/p4-horizon-development/512/independent.json) | [2048-tick recalculation](../../build/work/w2/rva-codex-01/p4-horizon-development/2048/independent.json) | [Independent Tier 1 review](../../build/work/review/p4-06/tier1-kernels/r1-01a1f512/REPORT.md)
