# Owner words: every quality word and demand in every owner prompt (Rule 0)

Written 2026-11-18 before the ABI 4 layout was changed, from `docs/handoff/PROMPTS.md`
P01 to P42 (P38 to P42 are the five messages of `build/work/api-run/owner-leave-room.md`);
P43 (2026-11-18 11:24:50 UTC) was added the same day; P43 in full, P44 to P51 and P52 to P55 are in section 5.
Only the owner's typed words are listed. The GrokBot and Gemini texts pasted inside P01 and
P07 are inputs that were fact-checked (`docs/GROKBOT-FACTCHECK.md`), not his words (P20); so are Claude's words quoted in P39 and the GPT Astra analysis pasted in P49.

Rule: nothing is dropped, softened or replaced by somebody's judgement. Where his words
conflict, the newest prompt wins and the conflict is in section 3.

Status words:
- **built**: in the tree, with a test that can fail.
- **ABI 4**: carried by the layout frozen in milestone 4 (the data and the mechanism in the
  core); a row that says "ABI 4 + W2" has its layout now and its behaviour in the named
  wave-2 module.
- **W2 (module)**: built when that wave-2 module replaces its stub (milestone 5).
- **later (Mn)**: scheduled in milestone n of `docs/handoff/CONTINUE-BUILD.md`.
- **law conflict**: cannot be built as said under a newer law of his; the reason is given.

The status column is brought up to date at the end of every milestone; the last update is
recorded at the bottom.

## 1. The words of P41 and P42 (the standing correction), one row each

| Word | Where it lives in the design | Status |
|---|---|---|
| DYNAMIC | Nothing about a creature has a compiled-in size: genome, brain and body are variable blocks in the creature store (`AzWorld.store`, handle column `blk`); every population-level table (clades, colonies) has a run-time capacity; the world's fields change every tick. | ABI 4 |
| ADAPTIVE (also P45: "procreates and evolves") | Evolution: variable genome with gene duplication, deletion, insertion and unequal crossover (`az_genome.c`); brain structure mutates (`az_brain.c`); within a life: Tier 1 plasticity (`az_tier1.c`) and growth that follows diet (`az_metab.c`, body parts). | ABI 4 + W2 (tier1, metab, life) |
| RESPONSIVE | Development responds to the environment: which body parts are built depends on what was eaten and where (`az_part_request`, traits SEGMENT, DEV_PLAST, ALLOMETRY); life stages respond to stores and weather (`az_life.c`). | ABI 4 + W2 (metab, life) |
| REACTIVE | Each active creature senses through 64 channels, runs its own brain block and acts through 24 motors. The pair protocol resolves eligible interactions in the tick. | Built for active creatures. Eggs and dormant creatures retain their receptor bytes and have zero motor outputs in the controlled public tick. Physics is at its stub stage. [Public tick evidence](../../build/work/w2/rva-codex-01/canonical-p1-integrated/review-behavior/owner.log) |
| INTERACTIVE | Pair interactions (share, groom, teach, recruit, attack, steal, trade, mate), signals, stigmergy marks, colony stores and the colony signal channel. | built (protocol, test_protocol); W2 (social, econ, mate, colony) |
| SPECTRUMS | Every trait is a continuous 0..255 value developed from gene dosage (copy number times allele times expression), not a switch; founders of one species differ; life stages, diets and mating modes are graded by traits. | ABI 4 (genome); W2 (owners of each trait) |
| VARIANCES | Standing variation in founders, mutation of every kind at heritable rates (traits MUT_RATE, GENE_DUP, GENE_DEL, GENE_NEW, NEURO_MUT, NEURO_GROW, NEURO_PRUNE), per-individual brains, wild-mode entropy. `az_stats_collect` measures trait means; variance columns are added with the stats module. | ABI 4; W2 (stats) |
| GRADIENTS | Cell fields vary continuously in space and time: 8 species, heat, temperature, light, charge, medium density and viscosity, marks; senses read gradients (food gradient channels, AZ_S_GRAD_T, AZ_S_LIGHT_AHEAD); C7 keeps transport from freezing into staircases. | built (fields, commit); ABI 4 (channels); built (weather, chem: landed earlier in milestone 5); built 2026-11-18 (worldgen: the starting gradients, temperature by depth, latitude and the nearest vent plume, toxin hot spots and reefs, terrain); W2 (sense) |
| BREADTHS | Breadth of kinds: 112 traits, 8 field species in 4 phases, 9 interaction kinds, 8 mating modes, 23 measured archetypes, founder species 1 to 100, 64 look classes; none of the counts limits a creature's size. | The native enumeration reserves 23 IDs. The released per-frame scorer can return 21 including Unclassified. Active abstention has no affirmative native measurement, and identifying a change of class requires observations across time. The census exposes 64 look bins. Worldgen supports 1 to 100 founder species across five guilds and five habitat bands. |
| DEPTHS | Depth of organisation inside one creature (genes, parts, neurons, tiles) is unbounded; depth of the world: the z axis is bounded with pressure, light and temperature changing along it. | ABI 4; built (weather, metab: landed earlier in milestone 5); built 2026-11-18 (worldgen: terrain over bedrock, a solvent ocean under carrier gas, temperature by depth, habitat bands from photic to floor and air) |
| LEVELS | Selection and bookkeeping at five levels: body part (parts in the block), creature (uid), colony as superorganism (`AzColony` with its own matter and energy pool), lineage (clade and founder species ids), ecosystem (cells, `AzStats`). No level has a compiled-in count. | ABI 4; W2 (colony, stats) |
| LAYERS | Tiers of state (fixed record, creature block, cold tables), layers of the brain (inputs, recurrent hidden tiles, motor tiles), layers of the world along z, Tier 0/1/2 of cognition. | ABI 4; built 2026-11-18 (worldgen: the world's layers along z, bedrock, the solvent ocean and the carrier-gas air above it; ocean under air is the brief's fixed structure) |
| VECTORIZED | Quantities with direction are vectors: velocity, rotor and angular velocity of a body, cell wind, local gravity (`grav`), light direction (`lvec`), percept offset sums, directed signal sums (`sigv_a`, `sigv_b`). In the code: structure-of-arrays columns, the AVX2 range test of the scan, the VNNI brain twin. | built (kinematics, scan); built (weather: landed earlier in milestone 5); ABI 4 (lvec, sigv); built 2026-11-18 (grav: the bending field written by worldgen); W2 (physics, sense) |
| CURVED | The design calls for pitch, yaw and roll to turn paths into curves. Response curves include dominance modes, saturating brain units, ALLOMETRY, TAXIS and CURVE. | Brain, genome expression and native sensing built. TAXIS scales directional gain and CURVE changes the sensory response. Rotor-driven turns and spirals await the P2 physics implementation. [Public tick evidence](../../build/work/w2/rva-codex-01/canonical-p1-integrated/review-behavior/owner.log) |
| DIRECTIONAL | Directional sensing uses tetrahedral axes, AHEAD channels and signal directions. The design calls for body-frame thrust, turn rates and aimed signals. The public record carries a facing vector. | The shared engine computes the body-frame thrust direction from its rotor, turns in response to motors, and charges energy for powered translation. A matched THRUST intervention removed the powered movement and its heat payment. See [the public-tick review](../../build/work/w2/rva-codex-01/p2-final-owner-lens/REVIEW.md) and [format-9 transfer](../../build/work/w2/rva-codex-01/p2-selected-transfer/landed.json). |
| LINEAR | Linear responses are kept where the physics is linear (integration of velocity, additive gene effects, exact transfers); linear and curved forms exist side by side (dominance mode 0 is additive). | built |
| GAME THEORIES | Outcomes of feeding, fighting, trading, signalling and mate choice depend on what the other side does: the pair protocol (intent, score, resolve), contested cell uptake, colony commons. Strategies are heritable traits (AGGR, BOLDNESS, SUBMISSION, RECIPROCITY, FORGIVE, PUNISH, STEAL, TRUST, STAKE, COMMONS, CASTE, CHOOSY, SIG_HONESTY ...). No payoff table: gains come from the conserved books (rule C6). | built (protocol); ABI 4 (strategy traits, motors AZ_M_STAKE, AZ_M_GIVE); W2 (social, econ, mate, colony) |
| NON DETERMINISTIC (also P45: "non deterministic life that's persistently evolving in unknown chaotic random ways we can't predict") | Wild mode is the default of a living run: the host hands the world fresh operating-system entropy every tick (`AzHost.entropy`), which is folded into the random key, so no seed fixes the run and no two runs are equal. Exact matter and energy balance still holds every tick. Lab mode (seeded) is only the test bench. | ABI 4 |
| WILD | Same mechanism, plus an alien world (not Earth) and no scripted behaviour. `az_run` runs wild unless `--lab` is given. | ABI 4; built 2026-11-18 (worldgen: an alien world drawn on the world key, so wild worlds differ and lab seeds repeat; a wild 100k run, 984e07a7655176a2, books exact) |
| FERAL | No steering, no fitness function, no scripted goals: behaviour comes from evolved brain blocks only (law: no theatre, nothing hardcoded); creatures are born, fight, starve and die by the books. | built (law, lint); W2 (all modules) |
| CHAOTIC (also P45: "unknown chaotic random ways") | Chaotic weather on the gradients (storms, two suns, vents), sensitive dependence through per-tick entropy in wild mode, contests drawn by chance. | ABI 4 (entropy); built (weather: landed earlier in milestone 5); built 2026-11-18 (worldgen: two suns with coprime days, 1 to 64 vents with their reservoir) |
| IMPROVISED | The NOISE sense channel and every random draw use the tick's fresh entropy in wild mode; motor AZ_M_IMPROV and trait IMPROV let a creature choose how much chance enters its action. | ABI 4; W2 (sense, physics) Native sensing built for the NOISE input and its lesion. All 24 tuning-seed trajectories changed. The 64-tick mean-energy verdict was NO EFFECT. Downstream improvisation and physics retain separate criteria. [Sense evidence](../../build/work/w2/rva-codex-01/p1-closure/grant-commission/G-P1.md) |
| ETC / "many more things" | The remaining words of every other prompt are in section 2; nothing is treated as covered by "etc". | - |
| "nothing in the universe is FIXED" | Only the physics is fixed: exact integer matter and energy balance, integer state. Limits are matter, energy and memory. Every remaining number the code needs is a gene or a world parameter with a recorded default and range (`core/az_params.def`), listed in `docs/agents/ABI-V4-NOTICE.md`. | ABI 4 |
| "EVERYTHING IN THE PROMPT" / "Stop ignoring my words" (also P43: "you need to pay attention to my whole prompt and not ignore or omit stuff man.") | This file; it is re-read at every milestone and before every layout change. Master 1.3 maps his newest message part by part. | built (this file) |
| "Yes, leave room" (P38) | u64 tick, uid and birth tick, u32 generation; colony as superorganism; no type that can wrap within a 10-year run; a test that crosses the old 2^32 limits. | ABI 4 |
| "Life isn't fixed or fixed sizes or fixed anything" (P39), "Yes, nothing fixed" (P40); also P45: "from a dot to a gigantic creature" | Variable genome, brain and body in the creature store; no hard caps in the biology; a store that runs out is a recorded refusal, never a crash. | ABI 4 |

## 2. Every other prompt, in order

| Prompt | Word or demand | Where it lives | Status |
|---|---|---|---|
| P01 | ASMODENSIUS; "Alien Life 1-100" | The engine; founder species 1 to 100 (`cfg.n_species`, `az_worldgen`). | built (config); built 2026-11-18 (worldgen: 1 to 100 founder species, each with its niche and one az_genome_fit prototype) |
| P02 | all of Tsotchke's 27-repo stack, "whatever we can get from it" operationally, functionally, computationally | `core/tk_sh`, `tk_hop`, `tk_qgt`, `tk_qsv` derived from libirrep, spin_nn, QGTL, Moonlab (THIRD_PARTY_NOTICES.md); the audit says which repos are scaffold. They act in Tier 1. | built (kernels, tests); W2 (tier1) |
| P03 | stay away from virtual RAM | Locked arena, a failed lock is an error; no pagefile, no mapped files. | built |
| P03 | stay away from DDR5; mostly L1 cache | The fixed record is cache-first (structure of arrays, 128-entity blocks); see conflict 1. | built; conflict 1 |
| P03 | lower to bare metal, threads and cores directly, "whatever is above the transistors" | NASM twins, raw-byte VNNI, core placement, per-stage cache tables (track M); within Windows only (P19). | built (twins, host); later (M6: track M) |
| P04 | incredibly complex game theory | See GAME THEORIES. | ABI 4 + W2 |
| P04 | economics | Trade pairs with reservation prices, inventories, colony stores, market memory (`az_econ.c`, `az_econ_global`). | W2 (econ) |
| P04 | physics | Kinematics with rotors, buoyancy, drag, gravity vectors, pressure, heat. | Built in shared format 9. [Paid movement, buoyancy and drag controls](../../build/work/w2/rva-codex-01/p2-landing-preflight/focused.json). |
| P04 | chemistry engines | 8 species, reactions, diffusion, phases, energy per unit of every form of matter. | W2 (chem) |
| P04 | DNA and hereditary | Diploid variable-length genome, meiosis with unequal crossover, dominance, dosage, mutation; brain blocks inherited from the bearing parent. | ABI 4 |
| P05 | ENDLESS shapes, formations, geometries | Body plan bytes per clade, shape traits, body parts of any number and kind, superformula shapes in the viewer. | ABI 4 (parts); built (view); built 2026-11-18 (worldgen: size, social style and life history drawn per species and world; founder bodies are built from the species' base genome); W2 (stats) |
| P05 | endless skins and shaders, "no limits" | Appearance traits (HUE, SAT, PATTERN, GLOW, SHEEN, SPIKES), software renderer skins. Limits are matter, energy and memory only. | built (view); W2 |
| P06 | 3-dimensional multivector multiaxis world and creatures | 3D grid, positions and rotors (geometric algebra), bivector angular velocity. | built |
| P06 | able to SCALE, grow to millions of entities | 1,000,000-creature reference world today (DRAM-bound); u64 ids so a world can run for years; scaling steps S1 to S4. | built (1M); ABI 4 (ids); later (M9) |
| P06 | grow their intelligences | Brain blocks that can add neurons and connections; Tier 1 learning. | ABI 4; W2 (tier1) |
| P06 | ecology and evolution abilities | Niches on gradients, niche construction (marks, structure, waste), heritable mutation rates. | W2 (metab, chem, econ) |
| P06 | the world and lifecycle and system and environment | Life stages (egg, juvenile, adult, senescent, dormant), weather, seasons. | W2 (life, weather) |
| P06 (also P43, P51) | 14 Butlin, 10 consciousness, 25 CQM, 100 things, 100 checklists, 100 benchmarks, 500 checklist; P43 "goals 10 14 25 100 100 100 500 etc"; P51 "to total completion" | `docs/requirements/` (38 files); run on the complete build as indicator-only scorecards; 07 (the suites protocol), 06 sections 17 and 20, master 6.3. | later (M8) |
| P06 | endless colors | HUE, SAT, GLOW and the viewer palette. | built (view) |
| P06 | colonies and swarms and fleets in a massive sim | Colony records (superorganism), flock alignment channel, colony signal channel, roles. | ABI 4; W2 (colony, social) |
| P07 | "if we go to a million we really need efficient fast powerful computing" | Cache-first fixed record, parallel tick on 24 cores, measured at 100k and 1M before and after every layout change. | built; measured in M4, M6, M8 |
| P08 | endless characters, glyphs, fonts, syntax, semantics for classification | Glyph bytes of a clade (`AZ_B_GLYPH_A/B`), look classes, measured archetypes, the viewer's glyph atlas. | Built in the viewer and production statistics. It reports measured appearance and activity classes. The Live page exposes 112 trait means and variances with their living denominator. [Public readback](../../build/work/w2/rva-codex-01/p3-live-traits/receipt.json). |
| P09 (also P51) | full deep audit of every folder, file and line; report when it is all good; P51 "handles all bugs errors issues problems concerns dilemmas and any holes gaps blindspots" | Reproduce-first audit with skeptics and the 5 mutants; 09 (register, blindspots, closure), 10 (dilemmas). | later (M7) |
| P10 | no virtual machine, no DDR5, no virtual RAM on NVMe: a law | Laws in COMMON.md; lock check in every run; see conflict 1 for DDR5. | built; conflict 1 |
| P10 | no creatures in slow languages; intelligence and speed | Creature and world code is C23, NASM, PTX only. | built |
| P10 | no tokenizer, LLM, prompting, API, MCP, skills, LLM agent, pre-training | Nothing of the kind in the engine; brains are evolved integer weights only. | built (law, lint) |
| P11 | each creature individual and free | Own uid, own genome, own brain block, own body; no global controller. | ABI 4 |
| P11 | interactive adaptive responsive reactive dynamic in breadths and depths and layers | Section 1. | ABI 4 + W2 |
| P11 | hives, groups, teams, units | Colonies with castes and roles, recruiting, colony stores. | ABI 4; W2 (colony, social) |
| P11 | polyamorous or monogamous | Mating modes PROMISC, POLYGYN, MONOG, LEK, EUSOC, BUD, SELF, HERMA; pair bonds, fidelity, guarding. | Native mating modes and bonds are built. The production census distinguishes repeated exclusive reciprocal mating and multiple simultaneous incoming bonds. Restored histories disclose coverage. [Source and native witnesses](../../build/work/w2/rva-codex-01/p3-transfer-readback/REPORT.md). |
| P11 | chaotic and insane disruptors or spoilers | Traits MOODINESS, AGGR, STEAL, PUNISH, IMPROV; nothing forbids a destructive strategy. | W2 (social, tier1) |
| P11 | reclusive or hermits | Low SOCIAL and COLONIALITY, measured archetype HERMIT. | Built in production statistics from observed eligible recruitment refusals and the native traits. The viewer reports refusals, living refusers and history coverage. [Native review](../../build/work/w2/rva-codex-01/p3-skeptic-20261119-variance/REPORT.md). |
| P11 | Alpha Gigachads or Betacucks | Rank, DOMINANCE, SUBMISSION, rank-skewed mating; measured archetypes DOMINANT and SUBORDINATE. | W2 (social, mate, stats) |
| P11 | malevolent or benevolent or neutral or switching | ALTRUISM, CARE, RECIPROCITY, FORGIVE against AGGR, STEAL, PUNISH; measured archetype SWITCHER; brains decide act by act. | Behavioral acts and measured activity classes are built. Social reversal and a change of winning class have separate longitudinal witnesses. Per-frame SWITCHER and affirmative abstention remain unavailable. [Longitudinal evidence](../../build/work/w2/rva-codex-01/p3-switch-history/receipt.json). |
| P11 | "super depends"; wide range; 100 times beyond; create LIFE and diversity | Diversity is the goal inside the laws: nothing prescribes a strategy; 112 traits, variable structure at every level. | ABI 4 + W2 |
| P12 | foods, wastes | Uptake channels, waste pool, excretion, decomposers. | W2 (metab, chem) |
| P12 | solids, liquids, gases, plasmas | Phase of each species per cell (`phase`), plasma above charge 192. | W2 (chem, weather) |
| P12 | toxicity | Radiotoxin species, toxin load, resistance, secretion, mutation term. | built (genome term); W2 (metab) |
| P12 | "everything ... needs to scale and expand and grow and improve ... always, like the universe and life" | No fixed sizes (P39 to P42); trigger register for every deferred step. | ABI 4; ongoing |
| P13 | weather and gravity | Suns, wind, heat flow, vents, storms, seasons; gravity as a vector field. | ABI 4 (grav); built (weather: landed earlier in milestone 5); built 2026-11-18 (worldgen: the grav field that bends, a downward pull plus a lateral pull toward high terrain plus a low-order swirl); shared format-9 bodies respond to local gravity and density. [Gravity tuning trial](../../build/work/w2/rva-codex-01/p2-selected-transfer/shared-gravity/receipt.json) |
| P13 | quantum mechanics "at least how we understand it" | Classical simulation of a small state vector (tk_qsv) and the quantum geometric tensor (tk_qgt) in Tier 1; never called quantum randomness or a speed-up. | built (kernels); W2 (tier1); indicator only |
| P13 | not Earth: alien, weird, crazy, wild; looks and feels different | Worldgen makes an alien world (two suns, a solvent that is not water, vents, plasma); the viewer's look. | built 2026-11-18 (worldgen: two suns, a solvent ocean under carrier gas, sea-floor vents, toxin hot spots and reefs, a bending grav field; the demo shows it); built (view) |
| P14 | the challenge: no garbage, slop, hallucinated or broken code | Evidence rules: run and quoted, failing-then-passing tests, mutants, audits. | built (process) |
| P15, P16 | cutting bleeding edge, extreme, unknowns of unknowns, novel 1/1 attempts, experiments | State-of-the-art choices are cited and measured against the C reference (P35); no novelty claim without a wide prior-art search. | ongoing |
| P15, P16 | new precisions, new models, new neural network | Integer int8 tile network of variable size per creature; fixed-point formats per quantity. | ABI 4 |
| P15, P16 | new geometric tensors, new quantum simulations | tk_qgt preconditioner, tk_qsv circuit, spherical-harmonic descriptors (tk_sh). | built (kernels); W2 (tier1) |
| P15, P16 | new algos; new variance and time constants | Heritable rates and time constants (traits); phased work at per-entity phases. | ABI 4 + W2 |
| P15, P16 | space state models, hybrids, mixed, morphed, mutated, weight, bias, precision model space | Recurrent state in the brain block, neuromodulation (gain, bias), structural mutation of brains and genomes. | ABI 4 |
| P15, P16 | "DO IT ALL", "you just do things", "don't ignore me" | Autonomous run; this file. | ongoing |
| P18 | package all prompts in MD and XML; audit, summarise, catalogue, plan | PROMPTS.md, PROMPTS.xml (P01 to P55), STATUS_AND_PLAN. | built |
| P19 (also P43) | no boot image, no "no OS", no USB boot; everything from Z: (ReFS Dev Drive): a RULE; P43 "work just in Z Drive not W X Y whatever" | Windows program only; builds, runs, logs and snapshots on Z:; no drive letter since 2026-11-18 and `build\z` configured from the real Z: path (AGENTS.md, master 3.4). | built |
| P20 | the pasted Gemini text conflicts with speed and efficiency; L1/L2/L3, ReFS Dev Drive | Pastes are inputs, not laws; cache-first fixed record. | built |
| P20 | below ASM and machine code, down to transistors and lithography, "if possible in research"; what Windows allows | Track M research note and per-stage cache tables; gates and lithography are research background. | later (M6: M1 to M3) |
| P21 (also P51) | continue the full refactor and build plan; the 25, 14, 100, 10 all over once the build is done; P51 "the rest of the milestones 10 total" | Milestones 1 to 10 (master section 4, 06). | M1 to M4 built; M5 in progress (10 of 13 modules real) |
| P21 | point at the prompt list and the reports | CONTINUE-BUILD.md section 1. | built |
| P21 | creatures are L1/L2/L3 at the start, then use CPU, GPU, NPU and DDR5, all of it, once the foundation is built; then scale and grow | Phase 1 cache-first, phase 2 after the foundation gate (S1 to S4). | later (M9); conflict 1 |
| P22, P30, P32 (P43 newest) | no garbage drives; minimal, consolidated; "Remove + stop repeats"; then "Leave them for now"; P43 "work just in Z Drive not W X Y whatever" | P43 is the newest and wins: N:, T:, V:, W: and S: were removed on 2026-11-18 and nothing maps them (05 drive-letter note, AGENTS.md); earlier the launcher kept them (P32). | built 2026-11-18; conflict 5 |
| P23 | refactor, rebuild into more things, scale; meet the 14, 10, 25, 100, 500 checklists | Milestones 4 to 9. | in progress |
| P26, P33, P34 | use the promotional API credit; "Yes, fix it" | The API run (`build/work/api-run/`). | built |
| P35 (also P48) | continue with Fable 5.2 max, ultracode, bypass permissions; P48 "Opus 5.5 MAX Ultracode ByPass Permissions" | This run; for P48 the desktop session on the Claude plan (section 5). | built |
| P35 | no USB boot, no virtual memory ("that was a mistake") | Laws restated. | built |
| P35 | "keep building it but with the SOTA FRONTIER SI"; "Figure it out. Get after it." | Open design choices take the current state of the art, cited and measured. | ongoing |
| P36, P37 | do the launch steps, run autonomously, 100 percent automatic; "I don't want to prompt any more or check till it's done" | The launcher loop; decisions by default recorded in PROGRESS.md. | built |
| P43 | "Use $15 more.  Then stop at $198" | The last invocation of the API run: capped at 15 USD by the launcher, BUDGET-STOP when under 2 USD would be left. | built (process) |
| P43 | "finish it up to a stopping point from Milestones" | Milestones 1 to 4 closed; milestone 5 stops at a module boundary (nine of thirteen modules real when the API run stopped, the rest named stubs; ten since worldgen landed on 2026-11-18, with sense, physics and stats still stubs); nothing half-landed. PROGRESS.md "FINAL" entry. | built |
| P43 | "a solid build" | Clean build of the whole tree with 0 warnings; the full suite green; lint 0 errors; matter and energy exact in every run shown. | built |
| P43 | "shipping poc mvp" | `az_run` and `az_snap` run the living world, wild by default and `--lab` for the bench (ctest `cli_snap_modes` holds both); the demo commands and their outputs are in PROGRESS.md; the reference worlds are in `tests/golden_hashes.txt`. No GUI in this run (left to the desktop session by the kick-off text); the GUI his full text asks for is the live demo (section 5). | built |

## 3. Conflicts between his words (the newest prompt wins)

1. **DDR5.** P03 and P10: stay away from DDR5, "it's a law". P21 (newer): creatures start on
   L1/L2/L3 and later use "CPU GPU NPU DDR5 all of it". Reading in force: the locked arena
   lives in DDR5 as backing store; hot loops are designed for cache; DDR5 as a designed
   streaming tier, the GPU in the tick and the NPU come after the foundation gate (M9).
   The pagefile and NVMe-backed memory stay banned in every phase.
2. **Bare metal.** The texts pasted in P01 and P07 propose UEFI and USB boot. P19 and P35
   (newer, typed): no boot image, nothing without an operating system, no USB boot. The UEFI
   host is parked unbuilt.
3. **Determinism.** The earlier design made every run a pure function of its seed (from the
   pasted text and from the lead's own reading). P41 and P42 (newest): NON DETERMINISTIC,
   WILD, FERAL, CHAOTIC, IMPROVISED. Reading in force: wild mode is the creature's world and
   the default; the seeded lab mode exists only so that tests, lesion contrasts and reference
   hashes can check the code. A Claude session once overrode "non deterministic"; that is not
   to happen again.
4. **Fixed blocks.** The text pasted in P01 says to pad variable brains into fixed-size
   masked blocks, and the first ABI 4 freeze kept fixed sizes for speed (P03, P20). P39 to
   P42 (newest): nothing fixed. Reading in force: variable blocks, with the fixed record kept
   cache-first and the speed cost measured and recorded.
5. **Drive letters.** P22 and P30 (remove) against P32 (leave them): P32 was newer until P43 ("work just in Z Drive not W X Y whatever"), which wins; the letters were removed on 2026-11-18.
6. **"No limits" (P05) against the laws.** The only limits are matter, energy and memory,
   and the owner's laws (no pagefile, no VM, no LLM). A store that fills up refuses the
   allocation and records it.

## 4. Updates

- 2026-11-18, written before the layout change of milestone 4 (step 0). Rows marked "ABI 4"
  are the work of steps A to G recorded in `build/work/PROGRESS.md`.
- 2026-11-18, end of milestone 4 (steps A to G done). Checked row by row against the tree:
  every row marked "ABI 4" has its layout and its engine mechanism in the core, with a test
  that can fail (tests/test_store.c, test_genome.c, test_brain.c, test_wild.c,
  test_protocol.c, test_snapshot.c): the creature store and the block behind the handle; a
  genome, a brain, a body and a memory of any length per creature; no cap constant in the
  biology (world parameters in `core/az_params.def` and one file per module in
  `core/params/`); 112 traits, 64 senses, 24 motors with the gradient, vector, stake, commons
  and improvisation channels; the colony's own energy pool and four signal channels; 64-bit
  tick, uid and birth tick and a 32-bit generation, tested across the old limits; wild mode
  as the default with fresh entropy every tick, lab mode for the test bench, and the journal
  that is written and never read back. Added in step G beyond the rows above, from "nothing
  fixed": the long-term memory (`az_memory`, any number of 64-bit words; the four-pattern
  working memory stays in the fixed record) and one parameter file per wave-2 module, so that
  no module needs a constant of the biology. Measured, as P39 asked (bytes per creature and
  ticks at 100,000 and 1,000,000 before and after): `docs/agents/ABI-V4-NOTICE.md` section
  5.7. The speed cost is real (about 1.3 to 1.9 times the tick of the first freeze) and is
  recorded with what could buy it back (`docs/TRIGGER-REGISTER.md` TR-14 to TR-18).
  Every row marked "W2 (module)" is still a stub: behaviour that the words ask for and that
  only a module can give (strategies, payoffs, weather, an alien world, learning) is not
  built until that module lands.
- 2026-11-18, worldgen landed (wave 2 module 10; `core/az_worldgen.c`, entry "wave 2,
  module 10, worldgen landed" in `build/work/PROGRESS.md`). Ten of thirteen modules are
  real; sense, physics and stats are still stubs. Only the rows that named worldgen (and
  LAYERS, VECTORIZED and the P21 and P43 rows, made stale by it) were brought up to date;
  the full status pass waits for the end of milestone 5. Limits that touch his words: the
  bodies do not yet feel gravity (the physics stub ignores gravity, rock and density, so a
  body can later walk into rock), and founder bodies are built from the species' base
  genome; weather relaxes the depth and latitude gradients over time; some worlds have no
  founder in the air; the ocean always under air, gravity always toward -z and the
  guild-to-band map are the brief's fixed structure, not his words, and stay open against
  "nothing in the universe is FIXED".
- 2026-11-18, brought to P51 after his message of 19:29:42 UTC (`docs/handoff/PROMPTS.md`
  then had P01 to P51; P52 to P55 were added the same day). New words are in section 5. Words already mapped carry the new ids in
  their rows: section 1 ADAPTIVE, NON DETERMINISTIC, CHAOTIC, "EVERYTHING IN THE PROMPT" and
  the P39/P40 row; section 2 P06, P09, P18, P19, P21, P22/P30/P32, P35 and P43. Conflict 5
  now names P43 as the winner; conflict 7 is in section 5. No line above this entry was added
  or removed, so the line numbers other files cite in sections 1 to 4 still hold. No full
  status pass (that waits for the end of milestone 5).

## 5. P43 to P51: the words added on 2026-11-18

Source: `docs/handoff/PROMPTS.md` P43 (his full 1,099-character text of 11:24:50 UTC; the
first record quoted it in short) and P44 to P51 (P44 to P46 are older than P43 but were found
later). "Master" is `docs/handoff/next-agent/00-MASTER-PROMPT.md`; 01 to 10 are the other
files of that folder. Numbers of his own demo world are from
`build/work/logs/demo_owner_world_2026-11-18.json` (run 20261118-103059-46dbdf, frame 228, tick
13,680; frame 226, tick 13,560, is in `demo_owner_world_2026-11-18_f226.json`, 08 section 2.5).

| Prompt | Word or demand | Where it lives | Status |
|---|---|---|---|
| P43 | "Use $15 more.  Then stop at $198" (the full text; the rows of section 2 quoted it in short) | API run invocation 5 (05 section 4). The credit ran out at 190.66 USD by the launcher's count, so 198 was never reached. | built (process) |
| P43 | "give me a full deep dive comprehensive analysis report on what is built and what is working and functional and operational" | `docs/handoff/POC-MVP-REPORT.md` (sections 3 to 7: built, working, operational, not done); master section 2. | built 2026-11-18 |
| P43 | "if we can at least have a live demo you can setup to what's at least the PoC MVP we got.  I want to test it." | The live demo: `demo/README.md`, `demo/live_server.py`, `demo/live.html`, `tools/az_live.c`; POC-MVP-REPORT section 2 ("See it and test it yourself"). He runs it himself: run 20261118-103059-46dbdf held 1,000,166 creatures at tick 13,680. | built 2026-11-18 |
| P43 | "show me here with a link you will open and do it for me to see me test it in our own GUI locally" | The demo page at http://127.0.0.1:8765/, on this machine only (`demo/README.md`); our own page, no outside viewer. Pictures, not frames: the real-time viewer is 08 tracks RV-A (browser) and RV-B (native window). | built (picture GUI); later (M6: real-time viewer) |
| P43 | "If you need to finsih a few things that's fine too to build or code etc." | Chem piece B in invocation 5; worldgen (wave 2 module 10) by the desktop session after the credit (05 section 4). | built |
| P43 | "We will be having the new OpenAI GPT Astra 6.2 because my credits reset.  So then package a prompt to give it so it can work on it all and knows what's up and what we're doing" | The package `docs/handoff/next-agent/` (master and 00-PROMPT to 10), POC-MVP-REPORT, `AGENTS.md` at the root. | built 2026-11-18 |
| P43 | "and stages and devopsa and pipeline and index table of contents" | 02 (stages and status), 03 (devops pipeline), 01 (index); every package file has a table of contents. | built 2026-11-18 |
| P43 | "and goals 10 14 25 100 100 100 500 etc all of that benchmark lists requirements laws stuff" | 04 (goals and benchmarks), 07 (suites protocol), 05 (laws and requirements), `docs/requirements/`; section 2, P06. Running the suites is milestone 8. | built (package); later (M8: the runs) |
| P43 | "Try to also make it so we can work just in Z Drive not W X Y whatever." | No drive letter since 2026-11-18; `build\z` configured from the real Z: path; AGENTS.md (machine and shell rules); master 3.4 (Z: path and shell rules); COMMON.md "Paths"; 05 drive-letter note. Wins over P32 (conflict 5). | built 2026-11-18 |
| P43 | "you need to pay attention to my whole prompt and not ignore or omit stuff man." | This file (every word, nothing dropped); master 1.3 (his newest message, part by part); 06 WP-P0-06; 10 DL-14 (every word counts against a finite plan). The same demand as P42 (section 1). | built (this file); ongoing |
| P44 | "SDKs are awesome for LLMs cause it's like a cron job but without the wheels bells and whistles spinning in front quiet silent efficient and i think uses less tokens right?" | How the next agent works, not the engine (law 3: no LLM in the engine): quiet and headless, no windows (AGENTS.md); long jobs in the background with logs on Z: (master 3.5); the context budget (master 0.4); repository or chat mode (master 0.1). Whether it uses fewer tokens is not measured here. | ongoing (process) |
| P45 | "So tell me what we're actually building and how complete and thorough is this if possible to ship" | POC-MVP-REPORT sections 1 and 3 to 7; 02 (the 10-milestone plan and its status); master section 2. | built (answered) |
| P45 | "considering my massive prompts and the build already and the tests checklists benchmarks previously it met (AMAZING)" | The earlier results are history, not native evidence: 07 sections 5.7 and 6.15; 10 DL-09; master section 2 (v40 R21 A-arm 9/14 Butlin, 6/10 theories, 17/25 CQM, historical only). Native has no held-out score yet. | traced; later (M8: native scores) |
| P45 | "non deterministic life that's persistently evolving in unknown chaotic random ways we can't predict" | Section 1 NON DETERMINISTIC and CHAOTIC; wild by default (AGENTS.md law 6); 10 DL-01 (wild against reproducible science). | ABI 4 |
| P45 | "that fuels it's and persists forever" | Exact matter and energy books every tick (AGENTS.md law 5; his world: unbalanced 0 at tick 13,680); 64-bit tick, uid and birth tick (P38 row); long-run stability is 09 BS-08. | ABI 4 (counters, books); later (long-run evidence, 09 BS-08) |
| P45 | "and procreates and evolves" | Mating and life cycle (W2 mate and life, landed); variable genome with mutation (section 1 ADAPTIVE). His world: 41,580,664 births and generation 24 at tick 13,680. Sustained evolution on held-out seeds is not shown yet: 06 P5 (the decisive chain). | built (mechanism); later (M5 P5, M8: evidence) |
| P45 | "Not sure how this is possible to go from a dot to a gigantic creature" | Nothing fixed in a body (section 1, P39 and P40 row): parts of any number; the colony as superorganism (W2 colony). His world at tick 13,680: 1 to 8 parts per body (mean 2.02), so no gigantic creature yet. 10 DL-03; 09 BS-16 (his expectations against what the science can show). | ABI 4 + W2; later (evidence) |
| P45 | "a gigantic creature that just does things...and is smart...but hey ...." | Evolved brains of any size (his world: 8 to 56 neurons, mean 31.08) and Tier 1 learning; "smart" is measured indicator-only (AGENTS.md law 8): 07 (Butlin 14, the 10 theories), 10 DL-03. | built (tier1); later (M8: indicators) |
| P46 | "It says there is still $50 left to use.  Check how much we spent and how much is left.  Continue to use the rest of the $50 and stop then." | API run invocation 4 lifted the run budget to 189 USD (05 section 4). The credit is used up. | built (process) |
| P46 | "Tell me how far we are in the build progress too." | 02 (status), POC-MVP-REPORT, `build/work/PROGRESS.md`. | built (answered) |
| P47 | "Ok run till it runs out to finish so no errors." | Invocation 5 ran until the API refused for low credit (190.66 USD by the launcher; 05 section 4). | built (process) |
| P47 | "don't be safe.  punch for it." | Master 1.4: work on your own, never wait, take the recorded default and go on. It lifts no law and no act that needs his yes (master 3.1, 3.2); conflict 7. | ongoing |
| P48 | "Finish up with Opus 5.5 MAX Ultracode ByPass Permissions here to a safe stage" | The desktop session landed worldgen with the suite at 46 of 46, then stopped (05 section 4; master section 2; 02). | built 2026-11-18 |
| P48 | "and then package a prompt and files to give to OpenAI GPT Astra 6.2 to continue since my credits reset this week." | The package `docs/handoff/next-agent/`, POC-MVP-REPORT, AGENTS.md; the zip from `build/work/scripts/make_handoff_bundle.py` is regenerated in P0 (master 0.1). His OpenAI credit and its stop line are not on record (master 7.6, OD-14). | built (package); later (P0: the zip) |
| P48 | "Same prompt as before." | P43's package request (rows above); master 1.3 and 1.4. | built |
| P49 | "??????????????  how far is it along?" | Master section 2 (the current state in one screen); 02; POC-MVP-REPORT. | built (answered) |
| P49 | the GPT Astra analysis he pasted (5,039 characters) | An exhibit, not his words: master section 2 ("Previous agent's analysis"); 07 section 5.8 (the crosswalk workbook). | - (input) |
| P50 | "Yes, fix it" | His yes to correcting the stale claim that the tk kernels are "not wired": all four run in the tick inside Tier 1 (POC-MVP-REPORT section 3; 01, corrected 2026-11-18). | built 2026-11-18 |
| P51 | "Make me a massive prompt and package for it to continue working" | Master and the package 00-PROMPT to 10. | built 2026-11-18 |
| P51 | "on the rest of the milestones 10 total to do" | Master section 4 (phases P0 to P10, milestones 5 to 10); 06 (the completion program); 02; section 2, P21. | later (M5 to M10) |
| P51 | "and the real actual goals and checklists and benchmarks to total completion" | 07 (the suites 14, 10, 25, 100, 100, 100, 500 and the gates); 06 sections 17 and 20; master 6.3 (the definition of total completion); section 2, P06. | later (M8, M10) |
| P51 | "and after handles all bugs errors issues problems concerns dilemmas and any holes gaps blindspots." | 09 (the register, blindspots BS-01 to BS-23, "nothing left open" in section 9); 10 (dilemmas DL-01 to DL-14); master 6.1 and 6.3; section 2, P09. | later (every milestone; closed at M10) |
| P51 | "I notice it runs on PICTURES and TICS.  Not Frames Per Second.  Why?" | Master 1.5; 08 section 0; 09 section 5.8 (DEMO); 10 DL-10; 06 section 2.3. The world steps in ticks; the demo draws a picture on the same thread after the tick (`tools/az_live.c:527,534`), so the world stands still while it draws. | built (answered) |
| P51 | "Can that be fixed?" | Yes: 08 tracks PQ (quick wins), RV-A (real-time viewer in the browser) and RV-B (native window); 06 WP-P6-01. Done when frames per second are measured. | later (M6) |
| P51 | "It's really slow." | His world at tick 13,680 (1,000,166 creatures, 8 workers): 397.6 ms per tick (about 2.5 ticks per second; perceive 135.7 ms of it) and 4,383.3 ms per 1600x900 picture. Pictures: 08 PQ and RV-A. Ticks: 08 track ES; 06 WP-P6-06 and WP-P6-07. | later (M6) |
| P51 | "I thought this would be fast." | 08 section 10 (what is honest to promise); 10 DL-04 (speed against exactness) and DL-05 (massive scale against the cache law); 09 BS-16. | later (M6, M9) |

Conflict 7 (kept here so that the line numbers of section 3 do not move):

7. **Safe or not.** P47: "don't be safe.  punch for it." P48 (newer): "to a safe stage". Reading
   in force: punch for the goal in effort and ambition (work on your own, never wait), and stop
   only at safe stages (tree green, books exact, nothing half-landed). Neither word lifts a law
   or an act that needs his yes (master 3.1, 3.2).

Added 2026-11-18 by the second fixer pass, kept at the end so that the line numbers other files
cite above do not move. Three phrases of P43, P47 and P48 had no row, and four typed records of
the desktop session (records P09 to P12 of `build/work/prompts/desktop-a1806fab-2026-11-18b.md:169-200`)
were not numbered then; the lead numbered them P52 to P55 the same day, in time order
(`docs/handoff/PROMPTS.md:978-1016`; master 1.2). Their words count all the same (rows below; P53, a pasted log, is an exhibit and has no row of its own).

| Prompt | Word or demand | Where it lives | Status |
|---|---|---|---|
| P43 | "Ok So you're going to  go $15 more then finish it up to a stopping point from Milestones with a solid build shipping poc mvp stop with the $15." | The budget of API run invocation 5 (05 section 4): the credit ran out at 190.66 USD by the launcher's count, before the $15 were spent. The stopping point is the safe-stage landing of worldgen: milestone 5 at 10 of 13 modules, ctest 46 of 46, the PoC MVP report (master section 2; `docs/handoff/POC-MVP-REPORT.md`). | built (process) |
| P47 | "if it's $9 ok we got 190.66 left.  I think we can." | The same invocation 5 and its budget (05 section 4): 190.66 USD is the launcher's count when the API refused for low credit. | built (process) |
| P48 | "Lets make it happen.  Error got in the way." | Read as the API's "Credit balance is too low", which ended invocation 5 and refused the one-cent test of 14:30 UTC, a minute before P48 (05 section 4; `build/work/api-run/state.json`, `preflight`); "make it happen" is the hand-off package and the safe-stage landing (the P48 rows above; master 1.4). | built 2026-11-18 |
| P52 (record 9, 2026-11-17 23:46:00 UTC) | "??? They dont work" (typed after an API key and its key ID, which are markers in every file: master law 15) | The API launcher refused the key (P53, record 10, a pasted launcher log, is an exhibit: the personal key was not scoped to a workspace). The key must be revoked in the Console by the owner; no file may hold it (master 3.1 law 15). | built (process); open (the owner revokes the key) |
| P54 (record 11, 2026-11-17 23:57:40 UTC) | "The Ultrabuild stopped and tab window closed.  Open and continue please" | The API run was started again: its invocation 1 began at 00:02:28 UTC (`build/work/api-run/state.json`; 05 section 4). Agents never open windows (AGENTS.md machine rules); he opens them. | built (process) |
| P55 (record 12, 2026-11-18 00:07:21 UTC) | "So the A-Life build is what is on GitHub I'm playing with but the BIG ONE is cooking right now with Anthropic's claude and because I had so many prompts so many things I wanted it will take a lot and time and is very cautious and big code base already." | Context, not a demand: this tree is "the BIG ONE"; the A-Life build on GitHub is not in this tree. "so many prompts so many things I wanted": this file and PROMPTS.md (every word). "will take a lot and time": master section 4 (phases P0 to P10). | noted (context) |
| P56 | Build the viewer and continue the engine program | Build RV-A and continue the native work packages. | Viewer deployed and engine work in progress |
| P57 | Restart the test world with the verified viewer | Retire the seed-42 verification world on port 8766 and start a fresh wild world there. | Verified by run ID, native frame, saved world and matching proof picture |
| P58 | Use my existing Codex allowance only, with no additional paid API usage | Run local work under the current Codex allowance and stop before additional paid API usage. | Recorded as DEC-0014 |
| P59 | We are doing the 10 milestones remember and these files to completion achievement ok? | Complete all ten native milestones and track the eight named ALife v3.0 files to their stated outcomes. | Active project scope |
| P60 | That's awesome we're at 1 million and 200 fps.  That machine language refractoring was worth it. | Reaction to the measured million-creature viewer and native refactor. | Context. Native ticks and display refresh are recorded separately. |
| P61 | Do what you need to do and is best | In reply to the question about saving ports 8766 and 8767, taking them offline for the exclusive legacy wild baseline, and restoring them afterward. Leave the separate original demo alone. | Authorized for this test |
| P62 | Tell me how far we are in the milestones and also for the a-life files too. | Report current progress across all ten native milestones and the eight A-Life source files. | Answered with milestone status, row coverage and formal acceptance scope. |
| P63 | Use the verified archive rule | Individually reviewed historical evidence in the work-folder text check may be verified by exact pathname and SHA-256. Check every other in-scope file normally and report archived files separately. |
| P64 | 1 and 2 and 3 all real time live generated adaptive reactive responsive interactive dynamic breadth depth gradients layters margins cause mating and fertility is complex.  Need a seperate weird genetic material engine model compiler assembler etc...I'll build that later.  Make a BLANK MARKDOWN that's Genetic Engine 0.1 for a blueprint and system that will work into 1 2 3. | All three mating and abstention measurement directions remain in scope. The owner will design the genetic-material model and compiler separately. | The requested blank blueprint exists and is zero bytes. Affirmative abstention and lifetime opportunity measures remain open. |
| P65 | Keep in mind I am building a separate repo for the Genetic Engine and called it Zeta Omega ALife Package using these..... (It's from Antigravity Gemini Neon 4 Extra)  Right now working on it..... This should help a lot.  It's being done in machine code byte bit level low level bare metal too.  C ASM etc lower....  | The owner is developing Zeta Omega ALife Package in its own repository, using C and assembly at the bit, byte and machine-code levels with bare metal as the target. The pasted Gemini handoff is a reference. | Owner-reported work in progress. Integration is open. |
| P66 | Accept the private prototype as a documented exception and keep every remaining check required | Accept the timing of the private statistics prototype for the named recovery snapshot, completed design and numeric cost registration. | Documented in DEC-0077. Formal sign-off still requires completion of every other check. |
| P67 | let me test it locally now i want to see what is built. | Test the working simulation locally. | The 3D viewer on 8766 showed a moving wild world. A separate development dashboard on 8768 displayed native pictures and 112 census trait rows. |
| P68 | this is what is used to look like.  The creatures looked better graphically and cooler.  wtf happened?  also more options and more data and more things to do and species too.  3D and detailed.  wtf happened. | Restore detailed 3D creatures, controls, data and founder-species information from the pictured native dashboard. | A separate local wild world on 8769 uses the native renderer and the pictured 5000 founders, 100 founder species and 1600 x 900 output. The standard dashboard also gained the complete parameter catalog, species and appearance tiles, and both camera presets. |
| P69 | Why do the World Viewer (Looks like total SHIT graphically visually horrible awful and boxes controls suck too) and the Live World looks much better and more complete more thorough more cool more visual?  WTF is going on? | Bring the native appearance, data and controls to the World Viewer people actually open. | The normal viewer should open a native view of its own active world and retain its interactive explorer. |
