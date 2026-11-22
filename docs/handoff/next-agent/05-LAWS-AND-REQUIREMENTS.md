# 05 Laws, owner words and requirements

> Compiled read-only on 2026-11-18 between 11:30 and 11:40 UTC from the files themselves (every claim
> cites path:line), then corrected by the desktop session for what changed afterwards: chem piece B
> landed, the suite reached 44/44 (then 45 of 45 (32 test programs, 13 gates) in 386 s on build\z), new reference hashes,
> the move to Z: only (no drive letters; `build\z` is the integration build), the live demo (`tools\az_live.c`,
> `demo\`), and, later on 2026-11-18, the worldgen landing (wave 2, module 10, built by the desktop session on the
> Claude plan after the API credit ran out): 46 of 46 (33 test programs, 13 gates) in 253 s on build\z and new
> reference hashes again.
> Line numbers in citations are as of the compile time; files have grown since. Section 2 and the
> PROMPTS.md and OWNER-WORDS.md citations were brought to P01-P51 later on 2026-11-18, then P52-P55 were added and every PROMPTS.md line citation was moved by 5 (1 header-note line, 4 index rows).


Root: `Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native` (written as `<R>` below). Every citation is path:line. I only read files. I ran nothing.

**Drive letters, settled 2026-11-18:** the letters `N:`, `T:`, `V:`, `W:` and `S:` no longer exist (checked with `subst` and Test-Path after the run), and the launcher no longer maps them. The owner's newer request (P43: "work just in Z Drive not W X Y whatever") wins over P32 ("Leave them for now"). What the research found before the fix:
- `Confirm-Letters` runs `subst` for any missing letter, and `Remove-Letters` deletes them (`<R>\build\work\api-run\Start-ApiRun.ps1:121-143`; now `:127-143`, and `$Letters` is empty at `:65`, so they map nothing).
- The old build tree `build\full` is hard-wired to `N:`: `CMAKE_HOME_DIRECTORY:INTERNAL=N:/` and `asmodensius_native_BINARY_DIR=N:/build/full` (`<R>\build\full\CMakeCache.txt:285,291,379`). It is not the live build. The live build is `build\z`, configured on 2026-11-18 from the real Z: path (`<R>\build\z\CMakeCache.txt:285,291,379` name `Z:/[Vibe Coded (AI)]/CLAUDECODE/Asmodensius-Native`); never use `build\full`.
- Scripts also carry drive-letter paths. Matches for `[NTVWS]:\`: make_status.py 22, clang_check.ps1 6, m_check.ps1 5, snapshot.py 4, run_tests.ps1 1, measure.ps1 1. Also tests\golden_hashes.txt 41, and CMakeLists.txt 5 (comments only, lines 8-13 and 44). Now (re-counted 2026-11-18 after the fixes): snapshot.py, clang_check.ps1, m_check.ps1, run_tests.ps1 and measure.ps1 0; make_status.py 2, both on one history line (`make_status.py:643`, the S: audit move).
- Working "just in Z:" therefore means:
  1. a fresh CMake configure with the real Z: source path, in a new build folder;
  2. editing those script defaults;
  3. then dropping the subst letters.

  All three are done (03-DEVOPS-PIPELINE.md, section 6).

### 1. The laws

**COMMON.md laws 1-10** (`<R>\docs\agents\COMMON.md:14-41`):

| # | Law (one line) | Source prompt |
|---|---|---|
| 1 | Native only. No VM, JIT or interpreted language in the sim; TS/JS/Python/Java/Lisp only as dev tooling. | P10 ("creatures in slow languages are not allowed") |
| 2 | Cache-resident. Hot loops never wait on DRAM and never touch a pagefile; one pre-allocated page-locked arena; L1-sized SoA blocks. | P03, P10, P20; sequenced by P21 |
| 3 | No LLM anything: no tokenizer, prompts, API, MCP, pretrained weights or datasets. | P10 |
| 4 | Zero cosmetic theatre. Every mechanism has a measurable causal effect (a lesion changes outcomes); no hardcoded steering. | P14, P15/16 (lead's reading) |
| 5 | Exact conservation: integer matter and energy that balance exactly. | lead's design, kept by owner-leave-room item 4/7 |
| 6 | Deterministic: integer/fixed-point state, Philox4x32-10, same seed gives the same hash for any thread count. | lead's design. **Superseded for living runs by P41/P42** (wild mode). COMMON.md:26-28 is NOT reworded yet. |
| 7 | Indicator only. Never claim consciousness or sentience, quantum randomness or a quantum speed-up. | P06/P13 (lead's reading) |
| 8 | Honest evidence. "Ran" only with the command and its output tail; "unknown" is OK, invented is not; no novelty claim without a prior-art search. | P14 |
| 9 | Windows only, from Z:. No boot image, no OS-less code, no USB boot, no VM; builds and runs on Z: (ReFS Dev Drive). | P19, P35 |
| 10 | Below assembly, inside Windows. Code goes as low as a Windows program can reach; gates, transistors and lithography are research only. | P20 |

COMMON.md's Paths section now says Z: only, with the path rules for the bracketed folder (rewritten 2026-11-18).

**ARCHITECTURE law table** (`<R>\docs\ARCHITECTURE.md:72-82`):
- **Native only:** `core/` is freestanding C23 plus NASM twins; no libc, OS or float in core.
- **Cache-resident:** one arena; SoA tiers; hot tier "exactly 140 bytes". This is stale: PROGRESS says the fixed record is now 124 B (`<R>\build\work\PROGRESS.md:424`).
- **No pagefile:** `VirtualLock`.
- **Windows only, from Z: (L6):** the UEFI host is parked in `build/dropped-uefi-2026-11-17/`.
- **Below assembly (L7):** raw-byte VNNI, cache-set layout, port analysis.
- **Deterministic (v76 reworded):** the hash is measured equal across threads, reorder and C/asm. CPU=GPU is a target only, for S2.
- **Exact conservation:** `==` checked every tick.
- **Zero cosmetic theatre (v77 reworded):** the line records, as measured 2026-11-18, that 5 of 96 traits are read and that, of 21 lesion bits, 11 are honoured and 8 are tested. That count is stale: the lint now reports 9 of 112 traits unread (PROGRESS:1151).
- **Indicator only.**

**CONTINUE-BUILD.md section 2** (`<R>\docs\handoff\CONTINUE-BUILD.md:20-35`), one line per rule:
- Native only: C23, NASM, PTX; never suggest QEMU, a VM or USB (:21).
- No LLM on any device, the NPU included (:22).
- Everything builds, runs, logs and snapshots on Z:; nothing on C: (:23).
- Integer state and Philox; matter and energy equal every tick, never within a tolerance (:24).
- No theatre: a lesion per mechanism, every trait read (:25).
- Indicator only; wide prior-art search before any novelty claim (:26).
- Scope: an alien 3D world; game theory, economics, physics, chemistry, DNA; phases, toxicity, weather, gravity; classical "quantum" (tk_qsv, tk_qgt); Tsotchke parts that work; hermits to hives; colonies, swarms, fleets; shapes and glyphs (P02, P04-P08, P11-P13) (:27).
- Scaling (P21 with P03, P10, P19, P20) (:28):
  - Phase 1, cache-first, until the milestone-9 gate: 24 cores and a locked DDR5 backing store are OK. More than about 0.5M creatures is DRAM-bound and must be labelled so.
  - Phase 2, after the gate: a DDR5 streaming tier, 2 MB pages, the GPU tick, the NPU. Each enters as a hash-equal twin with an off switch, a measured gain and a TRIGGER-REGISTER row.
- Banned in every phase: the pagefile, virtual or NVMe RAM, memory compression, GPU oversubscription, VMs, pretrained models. `--allow-pageable` and `--no-lock` are diagnostics only (:29).
- No installs, downloads or privilege changes without the owner's yes; T, V and W read-only; generic web queries (:30).
- `core/az_abi.h` and the `.def` files belong to the lead (:31).
- Never weaken a check; a test that cannot fail is a defect; no guessed tool flags (:32).
- Tests run only via `run_tests.ps1` (:33).
- Watch for Codex (:34).
- No windows, servers or browsers; ASCII, LF, no tabs (:35).
- Section 3 adds the seed rules: tune on 1-99; report on 5001-5024 and 5101-5124 only after preregistration (:40).

### 2. Newest owner prompts, in his words (`<R>\docs\handoff\PROMPTS.md`)

Numbering: PROMPTS.md has 55 prompts, P01-P55 (re-extracted and completed 2026-11-18; P44-P46 and P52-P55 are older than P43 but were found later, so they carry later numbers; P51 is the newest). `api-run\P22.md` "P22-P24" are PROMPTS.md P35-P37 (PROGRESS:151). PROMPTS.md P22 is a different message, the drives question.

- **P19 (:496-500):** "No Boot Image the NO OS ... no Virtual RAM ... same for the USB Boots. Everything boots from Z: ... This is a RULE. Z is a Dev Drive ReFS."
- **P20 (:502-506):** he did not read the Gemini paste well; it conflicts with speed. He wants L1/L2/L3, the ReFS Dev Drive, below ASM down to transistors and lithography "in research", whatever Windows allows.
- **P21 (:508-512):** continue the full refactor on the $200 API credit; "the 25 14 100 10 ... all over once the build is done"; point it at the prompt list and the reports; creatures are "L1 L2 L3 as a start but will scale ... CPU GPU NPU DDR5 all of it once the foundation is figured out".
- **P22 (:514-520):** why does he have "Gargage Code N and T and V and W drives" and a Windows S drive; he wants things "minimal and basic ... consolidate".
- **P23 (:522-526):** rebuild into more things and scale; "has to meet ... 14 10 25 100 500 whatever checklists benchmarks standards requirements".
- **P26 and P33 (:594-598, :723-727):** his promotional API credit does not work in the CLI.
- **P30 (:614-618):** "Remove + stop repeats". **P32 (:717-721):** "Leave them for now". P32 was newer, so the letters were kept until P43 ("work just in Z Drive not W X Y whatever"); they were removed on 2026-11-18 (OWNER-WORDS conflict 5).
- **P34:** "Yes, fix it".
- **P35 (:735-739) = P22.md "P22":** use the $200 with "FABLE 5.2 MAX Ultracode Bypass Permissions"; "No USB Boot type shit using Virtual Memory that was a mistake"; "keep building it but with the SOTA FRONTIER SI. Figure it out."
- **P36 and P37 (:803-813):** run it fully autonomously; "I dont want to prompt anymore ... it's all you now."
- **P38-P42 (:815-870; exact transcript times 01:11:52-01:28:38 UTC; also `<R>\build\work\api-run\owner-leave-room.md:3-17`):**
  - P38: "Yes, leave room".
  - P39: "Life isn't fixed or fixed sizes or fixed anything." In full (PROMPTS.md :824-846): he pasted 410 characters of Claude's own words ("every creature has a fixed size", 128 genes, 32 hidden brain units, 16-bit mass) and typed " ....yeah a problem.  Life isn't fixed or fixed sizes or fixed anything."
  - P40: "Yes, nothing fixed".
  - P41: "Dynamic Adaptive Responsive reactive interactive spectrums variances gradients breadths depths levels layers game theory non deterministic wild feral chaotic ... nothing in the universe is like FIXED FIXED".
  - P42: "Stop ignoring my words. (DYNAMIC ... NON DETERMINISTIC, WILD, FERRAL, CHAOTIC, IMPROVIZED, ETC ...) Nothing in the US is FIXED FIXED. EVERYTHING IN THE PROMPT!!!"
- **P43 (:872-880), 11:24:50 UTC, in full (1,099 characters; the earlier record quoted it in short from `<R>\build\work\api-run\kickoff.md`):** "Use $15 more.  Then stop at $198 and give me a full deep dive comprehensive analysis report ... if we can at least have a live demo you can setup to what's at least the PoC MVP we got.  I want to test it. ... test it in our own GUI locally. ... package a prompt to give it ... stages and devopsa and pipeline and index table of contents and goals 10 14 25 100 100 100 500 etc all of that benchmark lists requirements laws stuff.  Try to also make it so we can work just in Z Drive not W X Y whatever.  you need to pay attention to my whole prompt and not ignore or omit stuff man."

His later messages, one line each (UTC, 2026-11-18; PROMPTS.md line ranges; every word is mapped in OWNER-WORDS.md section 5, `:196-264`):
- **P44 (:882-888), 00:52:33:** "SDKs are awesome for LLMs cause it's like a cron job but without the wheels bells and whistles spinning in front quiet silent efficient and i think uses less tokens right?"
- **P45 (:890-896), 00:56:58:** "So tell me what we're actually building and how complete and thorough is this if possible to ship ... non deterministic life that's persistently evolving in unknown chaotic random ways we can't predict that fuels it's and persists forever and procreates and evolves.  Not sure how this is possible to go from a dot to a gigantic creature that just does things...and is smart...but hey ...."
- **P46 (:898-904), 09:20:03:** "It says there is still $50 left to use.  Check how much we spent and how much is left.  Continue to use the rest of the $50 and stop then. ok?  Tell me how far we are in the build progress too."
- **P47 (:906-912), 14:26:04:** "Ok run till it runs out to finish so no errors. ... don't be safe.  punch for it."
- **P48 (:914-920), 14:31:49:** "Ok's really out.  Finish up with Opus 5.5 MAX Ultracode ByPass Permissions here to a safe stage and then package a prompt and files to give to OpenAI GPT Astra 6.2 to continue since my credits reset this week.  Same prompt as before."
- **P49 (:922-960), 19:24:43:** "??????????????  how far is it along?" followed by a 5,039-character paste of the GPT Astra analysis (an exhibit, data, not his instructions).
- **P50 (:962-968), 19:26:37:** "Yes, fix it" (his yes to correcting the stale claim that the tk kernels are "not wired").
- **P52-P55 (:978-1016), four older desktop messages numbered last, in time order** (2026-11-17 23:46 to 2026-11-18 00:07 UTC; by time between P37 and P44): P52 an API key and its key ID (markers in every file), then "??? They dont work"; P53 a pasted launcher log, the key masked (an exhibit); P54 "The Ultrabuild stopped and tab window closed.  Open and continue please"; P55 "So the A-Life build is what is on GitHub I'm playing with but the BIG ONE is cooking right now with Anthropic's claude ... very cautious and big code base already."

**P51 (:970-976), 19:29:42 UTC, his newest message, verbatim (his double spaces kept):**

````text
Make me a massive prompt and package for it to continue working on the rest of the milestones 10 total to do and the real actual goals and checklists and benchmarks to total completion and after handles all bugs errors issues problems concerns dilemmas and any holes gaps blindspots.  I notice it runs on PICTURES and TICS.  Not Frames Per Second.  Why?  Can that be fixed?  It's really slow.  I thought this would be fast.
````

Where each part of P51 is handled: `00-MASTER-PROMPT.md` section 1.3. P47 against P48 (safe or not) is OWNER-WORDS conflict 7 (`:244-249`).

**owner-leave-room.md (desktop session's instruction file):**
- **"Everything in the prompt", Rule 0 (:21-23):** read every prompt and write OWNER-WORDS.md. Nothing is dropped or softened; the newest prompt wins; conflicts are recorded. A Claude session "overrode 'non deterministic' once. Do not do that again" (:19).
- **"Nothing fixed" (:25-49):**
  - The fixed per-creature record is bookkeeping only.
  - Genome, brain and body live in variable-size stores inside the locked arena.
  - No hard caps in the biology: the only limits are matter, energy and memory. An unavoidable bound is a run-time parameter, and running out is a recorded refusal, never a crash.
  - Counters are widened (u64 `birth_tick`, u32 `generation`).
  - A colony can be a superorganism.
  - Every biological parameter is heritable or a world parameter, never a buried constant (:54-56).
  - Game theory with evolvable strategies and no payoff table (:61-63); gradients and niche construction (:64-66); multilevel selection (:67).
- **"Non deterministic" (:68-74):**
  - Wild mode is the default for living runs: fresh OS or hardware entropy throughout; no seed decides the run; no two runs are equal; exact matter and energy balance still holds.
  - Lab mode is a seeded, repeatable test bench only (suite, lesion contrasts, reference hashes): "a tool, not the creature's world".
  - An optional entropy journal is never fed back.
  - As built: ABI-V4-NOTICE.md:375-396 (`az_run` is wild unless `--lab`; `--seed` needs `--lab`).
- **Vectorized, curved, directional (:75-79).**
- **Priority (:80):** M4 layout first; then W2 in the order life, mate, tier1, metab -> social, econ, colony -> weather, chem, worldgen.
- **"Work just in Z drive":** P19 (Z: as the rule), COMMON law 9, P22 (unwanted letters), P32 (leave them for now), P43 ("work just in Z Drive not W X Y whatever"). P43 is the newest and wins: settled, see the drive-letter fact at the top.

### 3. OWNER-WORDS.md (`<R>\docs\requirements\OWNER-WORDS.md`)

**Section 1 (:24-54), one row per word of P41/P42 (P45 and P43 ids added to ADAPTIVE, NON DETERMINISTIC, CHAOTIC, "EVERYTHING IN THE PROMPT" and the P39/P40 row):**
- **DYNAMIC:** variable blocks; run-time capacities. ABI 4.
- **ADAPTIVE:** genome duplication, deletion, insertion and unequal crossover; brain mutation; tier1 plasticity; diet-driven growth. ABI 4 + W2 (tier1, metab, life).
- **RESPONSIVE:** development follows the environment (SEGMENT, DEV_PLAST, ALLOMETRY). ABI 4 + W2.
- **REACTIVE:** 64 senses, own brain, 24 motors per tick. Built, plus W2 (sense, physics, social).
- **INTERACTIVE:** pair interactions, signals, marks, colony channel. Built (protocol) + W2.
- **SPECTRUMS:** continuous 0..255 traits from gene dosage. ABI 4 + W2.
- **VARIANCES:** heritable mutation rates; wild entropy; variance columns come with stats. ABI 4; W2 (stats).
- **GRADIENTS:** continuous cell fields and gradient senses. Built + ABI 4 + W2 (weather, chem, worldgen, sense).
- **BREADTHS:** 112 traits, 8 species, 9 interaction kinds, 8 mating modes, 100 founder species. ABI 4; W2.
- **DEPTHS:** unbounded internal depth; a bounded z axis. ABI 4; W2.
- **LEVELS:** part, creature, colony, lineage, ecosystem. ABI 4; W2 (colony, stats).
- **LAYERS:** tiers of state, brain layers, z layers, cognition tiers 0/1/2. ABI 4.
- **VECTORIZED:** velocity, rotor, wind, `grav`, `lvec`, `sigv`; AVX2 and VNNI twins. Built + ABI 4 + W2.
- **CURVED:** rotor curves, saturating units, ALLOMETRY, TAXIS. Built + ABI 4 + W2.
- **DIRECTIONAL:** tetrahedral senses, aimed signals. Built + ABI 4 + W2.
- **LINEAR:** additive or linear physics where it is real. Built.
- **GAME THEORIES:** pair protocol, strategy traits, no payoff table. Built + ABI 4 + W2 (social, econ, mate, colony).
- **NON DETERMINISTIC:** wild default with `AzHost.entropy` every tick; lab only for tests. ABI 4.
- **WILD:** same, plus an alien world; wild unless `--lab`. ABI 4; W2 (worldgen).
- **FERAL:** no steering, no fitness function. Built (law, lint) + W2.
- **CHAOTIC:** storms, two suns, per-tick entropy. ABI 4; W2 (weather).
- **IMPROVISED:** NOISE sense, AZ_M_IMPROV motor, IMPROV trait. ABI 4; W2.
- **ETC:** carried into section 2.
- **"nothing ... FIXED":** only the physics is fixed; every other number is a gene or a `core/az_params.def` parameter. ABI 4.
- **"EVERYTHING IN THE PROMPT":** this file. Built.
- **P38:** u64 counters, colony as superorganism, a test across 2^32. ABI 4.
- **P39/P40:** variable genome, brain and body; refusals instead of caps. ABI 4.

**Section 2 (:56-126):** every other prompt, P01-P43 (rows that P43, P48 and P51 repeat carry those ids too), with where it lives and its status. Built: native-only, no LLM, locked arena, 3D rotors, the 1M reference world, Windows-from-Z, the prompt packaging, the API run. Waiting on W2: econ, physics, chem, the 8 mating modes, hermit/alpha/switcher archetypes, phases, toxicity, an alien worldgen. (Of these, econ, chem and worldgen have since landed, and worldgen gives every world all 8 mating modes once it has 8 or more species; physics is still a stub.) Later: M6 track M, M7 audit, M8 the 14/10/25/100/500 suites, M9 GPU/NPU/DDR5 scaling.

**Section 3, conflicts (:128-151):**
1. DDR5 ban (P03, P10) against P21: P21 wins; DDR5 is a backing store now and a streaming tier after the gate.
2. UEFI/USB pastes against P19/P35: no boot image.
3. Determinism against P41/P42: wild is the default, lab is the test bench.
4. Fixed blocks against P39-P42: variable blocks.
5. Drive letters: P22/P30 against P32; P32 won until P43 ("work just in Z Drive"), which wins; the letters were removed on 2026-11-18.
6. "No limits" against the laws: the limits are matter, energy and memory.
7. (In section 5, `:244-249`.) P47 "don't be safe.  punch for it." against P48 "to a safe stage": push in effort, stop only at safe stages; neither lifts a law or an act that needs his yes.

**Section 4 (:153-194):** M4 closed and checked row by row; the measured speed cost is about 1.3-1.9x. Every "W2 (module)" row is still a stub until that module lands. As of the worldgen landing (2026-11-18) 10 of 13 have landed; the rows that wait on sense, physics or stats are still stubs.

**Section 5 (:196-264), added 2026-11-18:** every new word of P43 (in full) to P51, one row each, mapped to the package (master 1.3, 1.4, 1.5, 6.3; 06; 07; 08; 09; 10), the demo (`demo\README.md`), AGENTS.md and the Z: rules; then conflict 7 (`:244-249`); then (`:251-264`) three more phrases of P43, P47 and P48 and the words of the four older desktop records numbered P52 to P55 (`PROMPTS.md:978-1016`; the note at `:18-21`).

### 4. Budget history

- **Pre-API, priced at Fable 5.2 rates (CONTINUE-BUILD:64):**
  - Lead thread: about $828 in all ($88 own loop plus workflows of $38, $229, $288 and $184).
  - Repair thread: $49.
- **CLI session 7e5cd444 (Ultra plan, Opus 5.5; the spend.py figure is a pacing estimate, not a bill) (PROGRESS:4):**
  - $10.26 after the baseline (:26).
  - $97.46 at the end of M1 (:103).
  - $117.55 -> $121.38 at the end of M2 (:129).
  - $129.61 before m3-stack (:138).
  - The desktop scratch session a1806fab is a separate $31.13 (:26).
- **API run (`claude -p`, Fable 5.2, the $200 API credit), from 2026-11-18T00:02:**
  - Launcher cap: 180 (state.json `capUsd`; `Start-ApiRun.ps1:31`).
  - Invocation 4 lifted the run budget to 189 after the owner's P46 (09:20:03 UTC) "Continue to use the rest of the $50 and stop then" (PROGRESS:935-938).
  - Invocations (state.json lines 16-68):
    1. 72 min, NO-RESULT, cumulative 37.53.
    2. 6.5 min, NO-RESULT, 40.90.
    3. 302.7 min, BUDGET-STOP, 149.48 (result cost 115.05).
    4. 112.3 min, BUDGET-STOP, 181.93 (result cost 33.40).
  - Totals: `spentUsd` 181.93 after invocation 4 (ended 11:15:48 UTC, "the agent stopped at the budget line"); `resultCostSum` 148.45.
  - Invocation 5 (the owner's final 15 USD, P43): 11:28-12:04 UTC, cost 8.73 USD by the harness; landed chem piece B and the PoC MVP records; then the API refused with "Credit balance is too low" at 12:02 UTC and the invocation ended at 12:04 UTC. Launcher total 190.66 USD. The credit is used up; Console Billing has the real figure.
  - After that the owner asked for the last of the credit (P47, 14:26:04 UTC). A new launcher's one-cent test at 14:30 UTC was refused too ("Credit balance is too low"), and nothing changed (PROGRESS.md:1547-1549).
- **Desktop session on the Claude plan (after the credit, 2026-11-18):** the owner then asked the desktop session (P48, 14:31:49 UTC; Claude Opus 5.5, max effort, ultracode, on the Claude plan, not the credit) to finish to a safe stage and package the hand-off for OpenAI GPT Astra 6.2. It built and landed wave 2 module 10, worldgen, through workflows (design, staging tree, three adversarial reviews, a fixer), with no API spend (PROGRESS.md:1545-1611). No spend figure for this session is recorded here.
  - spend.py lines in PROGRESS: 20.78 (:196), 49.60 (:209), 103.54 (:449), 148.70 (:915), 149.80 (:938), 166.14 (:1067), 173.98 (:1177), 181.41 (:1243).
  - STATUS:58 says "about 184 of the 189 USD".
- **An earlier instruction (history, P43):** spend $15 more from 181.93 and stop at $198, above the 189 run budget and the 180 launcher default. Invocation 5 was that run; it ended when the credit ran out at 190.66 USD by the launcher's count, so the 198 line was never reached. There is no API budget left to plan with.
- **Measured lesson:** a tool call that blocks for more than 5 minutes loses the prompt cache; one cost $3.34 (CONTINUE-BUILD:76, PROGRESS:1247-1250).
