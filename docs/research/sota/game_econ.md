# Game theory, economics and social ecology in spatial agent simulations

Key: `game-econ`. Date: 2026-11-17. Web research note; no simulation code was run.

## How to read this note

Evidence tags on every fact:

- **[V]** read in the source text during this session (PDF text extracted locally or page fetched).
- **[S]** taken from a search-engine summary of the cited page; the page itself was not opened.
- **[M]** from memory; not verified in this session. Treat as a pointer, not as evidence.
- **[own]** my integer formulation. It is a proposal, not something a source states, and it is untested.

What I ran (only this; no model was simulated):

```
cd /s/sota/game-econ && python -I pdf2txt.py "$R/webfetch-1794918006870-ataes5.pdf" txt/theraulaz.txt && python -I pdf2txt.py "$R/webfetch-1794918009128-a3pfb1.pdf" txt/hemelrijk.txt && wc -c txt/theraulaz.txt txt/hemelrijk.txt
29953 txt/theraulaz.txt
62512 txt/hemelrijk.txt
92465 total
```

(`$R` is the session's fetch-result folder; the same script was run on six other fetched PDFs. This is text extraction only.)

Extracted texts are in `S:\sota\game-econ\txt\` (riolo, kerr, sugarspec, szabo, martinez, hauert, theraulaz, hemelrijk).

## Integer conventions used below [own]

- Random draw: `r = philox(seed, tick, agent_id, stream)` as u32. No shared RNG state, so thread count cannot change results.
- Bernoulli with rational probability a/(a+b): succeed iff `(u64)r * (a+b) < ((u64)a << 32)`. No division, no float.
- Compare two ratios n1/d1 > n2/d2 by cross-multiplying in u64 (or u128 when products exceed 2^64).
- Sigmoids (Fermi rule, logistic win chance): 256-entry lookup table of u16 thresholds indexed by a clamped integer difference.
- Every transfer is a move between two integer stores. Anything "created" by a published payoff rule must be drawn from a named environment store, and anything "destroyed" (fines, fight costs) must go to a named sink store.

**Conservation problem common to most game models.** Published payoffs are bookkeeping numbers, not matter. Donation games use benefit b greater than cost c (Riolo: c=0.1, b=1.0 [V]; Nowak and Sigmund: c=0.1, b=1 [V]); public goods games multiply the pot by r>1 [V]. Under Law 5 the surplus (b−c, or (r−1) times contributions) cannot appear from nothing. Two honest options [own]: (a) the surplus is paid by an environment store (for example, cooperative foraging unlocks a resource patch more efficiently); (b) transfers are strictly 1:1 and the benefit asymmetry comes from state, because one unit is worth more survival time to a starving recipient than to a full donor.

---

## 1. Spatial prisoner's dilemma

- Source: Nowak and May, "Evolutionary games and spatial chaos", Nature 359:826 (1992), https://www.nature.com/articles/359826a0 [M for the URL; paper not opened]. Model details below are from the review by Szabó and Fáth, "Evolutionary games on graphs", https://arxiv.org/abs/cond-mat/0607344 [V].
- Rule as published [V]: two strategies on a square lattice; one free payoff parameter b (C against C earns 1, D against C earns b, everything else 0). Each round every player plays all neighbours and sums payoffs; then every player copies the strategy of the highest-scoring player in its neighbourhood, itself included. Update is simultaneous and deterministic, so the model is a cellular automaton.
- Regimes [V]: with the Moore neighbourhood and self-interaction, 9/5 < b < 2 gives a dynamic balance of cooperator and defector invasions whose average composition does not depend on the initial state; without self-interaction the window is 8/5 < b < 5/3. The review reports runs on 10^6-site lattices. The often-quoted cooperator fraction 12 ln 2 − 8 ≈ 0.318 is [M].
- Integer form [own]: scale payoffs by 10. C-C = 10, D-C = B with B = 19 (b = 1.9). Score ≤ 9·19 = 171 fits u8. With B = 19 a cooperator score (multiple of 10, ≤ 90) and a defector score (multiple of 19, ≤ 171) are equal only at 0, so cross-strategy ties cannot occur above zero. Two-phase update on a double buffer: phase 1 writes scores, phase 2 writes the strategy of the argmax over 9 cells (fixed scan order, self first). Bit-sliced version: 64 cells per word, neighbour counts by carry-save adders.
- State: 1 bit persistent (strategy); 1 byte transient (score).
- Lesion: replace local imitation by imitation of a random distant cell; cooperation should collapse if the spatial mechanism is causal.
- Engine caveat: a strategy bit is a label. Under Law 4 the "strategy" should be a behaviour produced by the genome, and "imitate the best" becomes differential reproduction into neighbouring space.

## 2. Tag-based cooperation

- Source: Riolo, Cohen and Axelrod, "Evolution of cooperation without reciprocity", Nature 414:441 (2001), https://public.websites.umich.edu/~axe/research/EC_wo_reciprocity.pdf [V].
- Rule as published [V]: each agent has a tag t in [0,1] and a tolerance T ≥ 0. Each generation every agent is a potential donor to P others chosen at random with replacement. A donates to B iff |tA − tB| ≤ TA; A pays c, B receives b. Reproduction: each agent is compared with one random other and the higher scorer gets the offspring. Mutation: with probability 0.1 a new random tag; with probability 0.1 Gaussian noise (sd 0.01) is added to T, floored at 0. Runs: 100 agents, 30,000 generations, 30 replicates. With P = 3, c = 0.1, b = 1.0 the average donation rate was 73.6%; the population cycles through dominant tag clusters that are invaded by low-tolerance mutants.
- Known criticism [S]: Roberts and Sherratt (Nature 418:499, 2002) argue that because of the "≤", agents with identical tags must always donate, so cooperation is built in; allowing non-donation to identical tags was reported to collapse cooperation. https://public.websites.umich.edu/~axe/research/EC_wo_Reciprocity_Comment.pdf
- Integer form [own]: tag u8, tolerance u8; donate iff `abs((int)tA - (int)tB) <= TA`. Mutation thresholds: 0.1 → `r < 0x1999999A`. Tolerance noise: `(popcount(r1) - 16)` scaled, clamped at 0 (binomial stand-in for the Gaussian). Offer both "≤" and "<" as a switch, because the criticism turns on that one character.
- State: 2 bytes (4 bytes with u16 fields for closer fidelity to the real-valued paper).
- Lesion: scramble tags each tick; donation should fall to the level set by tolerance alone.

## 3. Indirect reciprocity by image scoring

- Source: Nowak and Sigmund, Nature 393:573 (1998); IIASA report IR-98-040, https://pure.iiasa.ac.at/id/eprint/5608/ [V via page-query extraction of the PDF].
- Rule as published [V]: every player has a public image score in −5..+5. When chosen as donor, a player with strategy k (k in −5..+6) helps iff the recipient's score ≥ k. Helping raises the donor's score by 1, refusing lowers it by 1; the recipient's score does not change. Values used: b = 1, c = 0.1, population n = 100, m = 125 rounds, mutation 0.001. Analytic condition for cooperation when only a fraction of players know a given image: q > c/b.
- Integer form [own]: score in a 4-bit signed field clamped to [−5,5], k in a 4-bit field. `help = score[j] >= k[i]`; then `score[i] = clamp(score[i] ± 1)`.
- State: 1 byte (two nibbles).
- Engine caveat: a globally readable score is a label shortcut (Law 4). A defensible version makes the score a body state that others must sense locally, or makes observers keep a small private table (cost: memory per remembered individual).
- Lesion: freeze scores at 0; discriminators become unconditional and defectors should invade.

## 4. Kin selection and Hamilton's rule

- Source: Hamilton (1964), J. Theor. Biol. 7:1, https://doi.org/10.1016/0022-5193(64)90038-4 [M for the DOI]. Rule: altruism is favoured when r·b > c. The Szabó–Fáth review restates it as b/c > 1/r and gives the related graph result that cooperation is favoured when b/c exceeds the number of neighbours z [V], https://arxiv.org/abs/cond-mat/0607344.
- Artificial-evolution test [S]: Waibel, Floreano and Keller, PLoS Biology 9:e1000615 (2011), https://pmc.ncbi.nlm.nih.gov/articles/PMC3086867 — simulated foraging robots with 33-gene neural genomes, five c/b ratios, hundreds of generations; altruism appeared when r exceeded c/b.
- Integer form [own]: no rule should enforce kin selection; it should emerge from limited dispersal. What the engine needs is (a) a way for genomes to condition helping on similarity and (b) a measurement. Give each agent a 64-bit neutral marker word inherited bitwise from its parents. Similarity = 64 − popcount(a XOR b). A genome-encoded threshold decides help. Measurement of Hamilton's rule: accumulate integer sums for cost paid, benefit delivered and marker similarity between actor and recipient, then test `sum_sim * B > C * sum_den` by cross-multiplication.
- State: 8 bytes (marker word); 2 bytes if a 16-bit word is accepted (noisier estimate).
- Lesion: shuffle offspring positions at birth (destroys spatial relatedness); helping should decline.

## 5. Hawk-dove and war of attrition

- Sources: Maynard Smith and Price, Nature 246:15 (1973), https://www.nature.com/articles/246015a0 [M]. Results as taught: https://evolution-textbook.org/content/free/notes/ch20_WN20E.html and https://r-heller.github.io/equilibria/tutorials/evolutionary-gt/hawk-dove-war-of-attrition/ [S]; war of attrition: https://college.holycross.edu/faculty/kprestwi/behavior/ESS/warAtt_glossary.html [S]; Bishop and Cannings, J. Theor. Biol. 70 (1978) [S, citation only].
- Rules [S]: resource value V, injury cost C. Hawk is a pure ESS if V > C; if V < C the ESS plays Hawk with probability V/C. War of attrition: both pay a cost per unit time, the one who persists longer wins V; the unique ESS draws its persistence time from an exponential distribution with mean V/c (c = cost rate); no fixed persistence time is stable.
- Integer form [own]: gene `p_hawk` as u8 (probability in 1/256). H-H: winner chosen by a fair draw (or by a size-weighted Bernoulli), takes V; loser moves C energy units to the sink store. H-D: Hawk takes V. D-D: each gets V/2; if V is odd the odd unit stays in the cell. War of attrition per tick: both move c units to the sink store; each quits with hazard q (gene, u16 threshold). The geometric distribution is the discrete, memoryless counterpart of the exponential, so no timer is needed; mean persistence is 1/q ticks and the ESS prediction is q = c/V.
- State: 1 byte (hawk-dove) or 2 bytes (attrition hazard); 4 bytes transient opponent index while a multi-tick contest lasts.
- Lesion: set C to 0; the population should go to all-Hawk.

## 6. Rock-paper-scissors coexistence

- Source: Kerr, Riley, Feldman and Bohannan, Nature 418:171 (2002), https://www.nature.com/articles/nature00823 ; text read from the copy at https://www.bcp.fu-berlin.de/biologie/arbeitsgruppen/zoologie/ag_steppuhn/lehrveranstaltungen/1_Theoretische-Oekologie_Kerr-et-al-2002.pdf [V].
- Rule as published [V]: 250 × 250 lattice, periodic boundaries, states empty / C (colicin producer) / S (sensitive) / R (resistant), asynchronous updating of randomly chosen points. An empty point is filled with type i with probability f_i, the fraction of its neighbourhood occupied by i. An occupied point of type i dies with probability Δ_i; Δ_C and Δ_R are fixed and Δ_S = Δ_S,0 + τ·f_C. Local neighbourhood = the 8 surrounding points; global = the whole grid. Parameters: Δ_C = 1/3, Δ_S,0 = 1/4, Δ_R = 10/32, τ = 3/4, with the requirement Δ_S,0 < Δ_R < Δ_C < (Δ_S,0 + τ)/(1 + τ). One epoch = 62,500 point updates. Result: with local neighbourhoods patches chase each other and all three persist (tested to 10,000 epochs); with the global neighbourhood S goes extinct and then R outcompetes C.
- Integer form [own]: all probabilities are exact in units of 1/96: Δ_C = 32/96, Δ_R = 30/96, Δ_S = (24 + 9·n_C)/96 where n_C is the number of C among the 8 neighbours. Draw r uniform in 0..95 and compare. Empty cell: pick one of the 8 neighbours uniformly and copy it if occupied (this gives probability f_i exactly). Parallel determinism: replace random sequential order by sublattice (coloured) sweeps with counter-based draws; that changes the update statistics and must be re-validated against the published coexistence result.
- State: 2 bits per cell.
- Prior GPU work [S]: a 2025 dissertation on GPU-accelerated spatial cyclic games reports about 28× from a CUDA implementation and grids up to 3200 × 3200 (hardware not captured by my search), https://arxiv.org/abs/2508.16639.
- Lesion: switch the neighbourhood from local to global; diversity should be lost (this is the paper's own control).

## 7. Sugarscape trade

- Sources: Epstein and Axtell, Growing Artificial Societies (1996), as formalised by Kehoe, "The Specification of Sugarscape", https://arxiv.org/abs/1505.06012 [V]; reference implementation notes at https://mesa.readthedocs.io/stable/examples/advanced/sugarscape_g1mt.html [V].
- Rule T as published [V]: MRS = (spice · sugarMetabolism) / (spiceMetabolism · sugar), that is, time-to-starvation in spice over time-to-starvation in sugar. If the two MRS are equal, stop. Spice flows from the higher-MRS agent to the lower-MRS agent, sugar the other way. Price p = geometric mean of the two MRS. If p > 1, p units of spice are exchanged for 1 sugar; if p < 1, 1/p units of sugar for 1 spice. The trade is made, and repeated, only while it raises both agents' welfare and does not make the two MRS cross. Welfare is Cobb–Douglas: W = sugar^(m_s/m_T) · spice^(m_p/m_T), m_T = m_s + m_p. The Mesa code truncates quantities with int() and uses floating point for MRS, price and welfare [V].
- Integer form [own]:
  - MRS as a pair (num = spice·m_s, den = m_p·sugar); compare by cross-multiplication.
  - p > 1 iff numA·numB > denA·denB. Quantity k = floor(p) is the largest k with k²·denA·denB ≤ numA·numB (integer square root, exact). For p < 1 swap numerator and denominator.
  - Welfare test without roots: W' > W iff sugar'^m_s · spice'^m_p > sugar^m_s · spice^m_p (both sides raised to m_T, which is monotone). With metabolisms ≤ 4 and holdings < 2^15 the products fit u128. Larger exponents need a fixed-point log2 table, which is deterministic but no longer exact.
  - The transfer itself is ±1 and ±k on four integer stores, so sugar and spice totals are conserved exactly. Growback (rule G) creates sugar and must be fed from an environment store.
- State: sugar u16, spice u16, two metabolisms and vision as 4-bit fields (2 bytes) = 6 bytes.
- Measured prior art (hardware stated) [V via page-query extraction]: Lysenko and D'Souza, JASSS 11(4):10 (2008), https://jasss.soc.surrey.ac.uk/11/4/10.html — AMD Athlon64 3500+, 1 GB RAM, NVIDIA GeForce 8800 GTX. Sugarscape with rules G, R and M only (no trade): about 56 updates per second with over one million agents on a 2560 × 1024 grid; grids up to 4096 × 4096 with over 16 million agents. StupidModel: about 1667 frames per second against about 8 for MASON at roughly 2000 agents on 256 × 128.
- Lesion: forbid trade; the paper-level prediction is lower carrying capacity and no price convergence.

## 8. Division of labour by response thresholds

- Source: Theraulaz, Bonabeau and Deneubourg, Proc. R. Soc. B 265:327 (1998), https://pmc.ncbi.nlm.nih.gov/articles/PMC1688885 ; text read from https://static.ias.edu/pitp/archive/2012files/18.pdf [V]. Fixed-threshold predecessor: Bonabeau, Theraulaz, Deneubourg, Bull. Math. Biol. 60:753 (1998) [S], https://www.santafe.edu/research/results/working-papers/fixed-response-thresholds-and-the-regulation-of-di.
- Rule as published [V]: worker i engages in task j with probability T = s_j² / (s_j² + θ_ij²). While performing the task, θ_ij decreases by ξ per unit time; while not performing it, θ_ij increases by φ; θ stays inside [θ_min, θ_max]. An active worker quits with probability p per unit time. Stimulus: ds_j/dt = δ − (α/N)·(number active on j). Example run: N = 5 workers, m = 2 tasks, θ(0) = 500, p = 0.2, φ = 1, with the other printed values 3, 1, 10 and 0.1 (the Greek letters were lost in text extraction; by position and from memory they are α = 3, δ = 1, ξ = 10, σ = 0.1 [M]). Identical workers differentiate into specialists.
- Integer form [own]: θ as u16 in [1, 1000]; engage iff `(u64)r * (s*s + th*th) < ((u64)(s*s) << 32)` with s, θ < 2^15. Update: `th -= xi` or `th += phi`, clamped. Keep the stimulus in units of 1/N: `sN += delta*N - alpha*n_active`. Better under Law 5: let the stimulus be a real quantity (waste in a cell, unfed brood count) so that "work" is a matter transfer and nothing is invented.
- State: 2 bytes per task plus 1 byte current task; 9 bytes for 4 tasks.
- Lesion: set ξ = φ = 0 (fixed thresholds); specialisation from identical starts should not appear.

## 9. Quorum sensing

- Sources [S]: agent-based and hybrid models of autoinducer signalling, for example the Agrobacterium population model in PLoS Comput. Biol. 1:e37 (2005), https://doi.org/10.1371/journal.pcbi.0010037, and a 2D finite-difference agent model of LuxIR activation, https://pmc.ncbi.nlm.nih.gov/articles/PMC4829230. I did not open these; no equation below is quoted from them.
- Mechanism in words [S]: cells emit a diffusible autoinducer; at low density it is lost by diffusion; above a threshold concentration cells switch state, and in LuxI-type systems the switched state emits more (positive feedback).
- Integer form [own]: field c (u16 per voxel). Each agent moves e0 units per tick from its own store into its voxel, or e1 > e0 when induced. Conservative diffusion: for each neighbour pair, flux = (c_i − c_j) >> k, added to one side and subtracted from the other. Decay: c −= c >> d, moved to a sink store. Switch with hysteresis: induced := 1 if c ≥ θ_on; induced := 0 if c < θ_off, θ_off < θ_on. A graded Hill response can reuse the Bernoulli test of section 8.
- State: 1 bit (induced) plus up to 3 bytes of genes (θ_on, θ_off, emission); 2 bytes per voxel for the field.
- Lesion: set e0 = e1 = 0 for a subset; the collective switch density should rise.

## 10. Mating systems

- Source: Emlen and Oring, Science 197:215 (1977) [S: paper not opened]. Summaries used: https://dalspace.library.dal.ca/bitstream/handle/10222/28926/Weir_657918.pdf?sequence=1 and https://www.kokkonuts.org/p/Cornerstones.pdf.
- Content [S]: the operational sex ratio (OSR) is the ratio of sexually active males to fertilisable females at a given time; the more it is biased, the stronger the expected sexual selection. Polygamy needs the ability to monopolise mates, directly (female defence) or through resources (resource defence), or through display aggregations (leks). Kokko and co-authors caution that the link from OSR to sexual selection is weaker than usually assumed [S].
- Integer form [own]: this is a classification and a set of measurements, not a rule. Per region and tick keep two counters (active males, receptive females) and compare regions by cross-multiplication. Monopolisability = index of dispersion of receptive females per cell (integer sum and sum of squares) and receptivity synchrony (maximum simultaneous receptive count over season total). Outcome per individual: `mates_this_season` (u8); the system is read from the per-sex distribution of that counter.
- State: sex 1 bit, receptive 1 bit, refractory timer u8, mates counter u8 = 3 bytes.
- Lesion: make female receptivity synchronous; variance in male mating success should fall.

## 11. Sexual selection: runaway and handicap

- Sources: Lande, PNAS 78:3721 (1981), https://doi.org/10.1073/pnas.78.6.3721 [S for content]; Grafen, J. Theor. Biol. 144:517 (1990), https://smg.media.mit.edu/library/Grafen.BioSignalsHandicaps.pdf [S]; Zahavi (1975) [M].
- Content [S]: Lande's polygenic model: a male trait and a female preference become genetically correlated through non-random mating; whether a runaway occurs depends on the genetic variance parameters and initial conditions. Grafen: honest signalling is stable when lower-quality signallers pay a higher marginal cost for the same signal. Lande's line of equilibria and the exact covariance threshold are [M].
- Integer form [own]: genes `trait` (u8, expressed in males) and `pref` (u8 signed, expressed in females), both carried by both sexes so that a covariance can build up. Choice: a female samples n males within range and takes the argmax of an integer score, either open-ended (`pref * trait`) or absolute (`-abs(trait - pref)`). Cost: the ornament consumes energy every tick, `cost = (trait - t_opt)^2 >> k`, moved to the sink store. Handicap: signal = `a + ((b * reserve) >> 8)` with genes a, b. If the signal is paid in real energy, the same payment hurts a poor individual more than a rich one, so Grafen's condition comes from the energy ledger rather than from a rule.
- State: 2 bytes (trait, preference); 4 bytes with the reaction-norm genes.
- Lesion: randomise mate choice; trait should fall back to the viability optimum.

## 12. Dominance hierarchies and winner-loser effects

- Source for exact rules: Hemelrijk, Wantia and Dätwyler, "Female co-dominance in a virtual world", Behaviour 140 (2003), https://www.rug.nl/research/gelifes/tres/publications/_pdf/hwd03_behav.pdf [V].
- Rule as published [V]: each agent has a value Dom. Agent i wins iff Dom_i / (Dom_i + Dom_j) > RND(0,1). Update with w = 1 for a win, 0 for a loss: Dom_i += (w − Dom_i/(Dom_i + Dom_j)) · StepDom, and Dom_j is reduced by the same amount. StepDom in (0,1) is the intensity of aggression. An unexpected win moves the values more than an expected one. Minimum Dom is 0.001. Winners chase, losers flee; a spatial structure with dominants in the centre results.
- Related [S]: Bonabeau, Theraulaz and Deneubourg, Bull. Math. Biol. 58:661 (1996) — fights with history-dependent win probability, an egalitarian-to-hierarchical transition with density; lattice and fully connected variants, https://arxiv.org/abs/physics/0502118. Dugatkin, Behav. Ecol. 8:583 (1997) — four individuals with resource holding power; winner effects alone give a linear hierarchy, loser effects alone fix only the top rank (summary in https://link.springer.com/10.1007/s11538-016-0186-9). De Vries (2009): the ratio win chance with additive update produces unrealistically large swings among low-ranked agents; ratio with multiplicative update is equivalent to Elo rating, https://catalog.comses.net/publications/1602.
- Integer form [own]: Dom as u16 (Q8.8), minimum 1. Win iff `(u64)r * (Di + Dj) < ((u64)Di << 32)`. If i wins: `d = (Step * Dj) / (Di + Dj)`; `Di += d; Dj -= d` (clamped). The update is antisymmetric, so the sum of Dom is constant except at the clamp. An Elo-style variant uses the sigmoid table on `Di − Dj` and avoids the division.
- State: 2 bytes. Individual recognition (remembering k opponents) costs about 5 bytes per remembered opponent and does not scale to full pair tables.
- Measurement [own]: for sampled groups of ≤ 16, a win-count matrix and the share of transitive triads.
- Lesion: StepDom = 0 (no winner-loser effect); the hierarchy should not form from identical starts.

## 13. Niche construction

- Source: Laland, Odling-Smee and Feldman, PNAS 96:10242 (1999), https://pmc.ncbi.nlm.nih.gov/articles/PMC17873 [V via page-query extraction].
- Rule as published [V]: two loci. Locus E sets niche-constructing activity (allele frequency p). A resource R in (0,1) follows R_t = λ1·R_{t−1}·(1 − γ·p_t) + λ2·p_t + λ3, with γ = 0 for positive and λ2 = 0 for negative niche construction; λ3 is independent renewal. Fitness at locus A has a term proportional to R (ε·R, ε·(1+R)/2 … ε·(1−R) across genotypes). Findings: fixation of otherwise deleterious alleles, new or destroyed polymorphisms, time lags, momentum and inertia when R depends on several past generations.
- Integer form [own]: R becomes a per-cell field (u16). An agent with construction gene e moves e units per tick from its store into the cell (positive) or removes a fraction (negative). Decay `R -= R >> k` to a sink; renewal from the environment store. The response gene a converts local R into intake or reduced mortality. Because the field outlives its builders, "ecological inheritance" is automatic.
- State: 2 bytes per agent (e, a); 2 bytes per cell.
- Lesion: reset the field every generation; lags and momentum should vanish.

## 14. Food-web stability

- Sources: Williams and Martinez, Nature 404:180 (2000), read from https://www.sccs.swarthmore.edu/users/08/bblonder/phys120/docs/martinez.pdf [V]; May, Nature 238:413 (1972), https://www.nature.com/articles/238413a0 [M].
- Niche model as published [V]: inputs are species number S and connectance C. Each species gets a niche value n_i uniform in [0,1]. It eats every species inside a range r_i = x·n_i, where x is drawn from a beta distribution with expectation 2C (x = 1 − (1 − y)^(1/b), y uniform). The range centre c_i is uniform in [r_i/2, n_i], which allows loops and cannibalism. Reported fit is about ten times better than the cascade model.
- May's criterion [M]: a random community is almost surely stable only if σ·sqrt(S·C) < 1.
- Integer form [own]: do not generate webs; measure the one that emerges. Diet genes per agent: niche value n (u8), diet centre c (u8), diet range r (u8); "can eat" iff `abs(n_prey − c) * 2 <= r`. Log every predation as an exact energy move from prey lineage to predator lineage into a sparse species-pair table. From the table: S, links L, C = L/S² (compare as integers), flux-weighted interaction strength. Stability is measured, not asserted: species persistence over T ticks, biomass variance, recovery after removing one species.
- State: 3 bytes per agent; the flux table is per species pair, not per agent.
- Lesion: widen all diet ranges to the maximum; the published expectation is loss of structure and of persistence.

## 15. r/K strategies

- Sources: Pianka, Am. Nat. 104:592 (1970) [S]; Reznick, Bryant and Bashey, Ecology 83:1509 (2002), https://basheyla.sitehost.iu.edu/pdfs/RezBryBas02.pdf [S]; Boyce (1979), https://era.library.ualberta.ca/files/k643b252q/AN_114_1979_569.pdf [S].
- Content [S]: the terms come from the logistic growth parameters; a phenotype with higher r wins at low density and one with higher K wins at high density. Reznick and co-authors conclude that age-specific demographic theory has replaced the r/K scheme. Use r/K as a descriptive axis only.
- Integer form [own]: genes clutch size (u8), energy per offspring (u16), age at maturity (u8). The trade-off is forced by the ledger: a parent with reproductive budget E gives `E / clutch` to each offspring and keeps the remainder. Density dependence must come from resource competition, not from a (1 − N/K) factor. Report per lineage: growth rate at low density and equilibrium count.
- State: 4 bytes.
- Lesion: give every offspring a fixed endowment from the environment store; the size-number trade-off disappears.

## 16. Public goods and punishment

- Sources: Szabó and Hauert, "Phase transitions and volunteering in spatial public goods games", https://arxiv.org/abs/cond-mat/0207569 [V]; Fehr and Gächter, Nature 415:137 (2002), https://www.nature.com/articles/415137a [S]; Brandt, Hauert and Sigmund, Proc. R. Soc. B 270:1099 (2003), https://pure.iiasa.ac.at/id/eprint/6918/ [S].
- Rule as published [V]: groups of N = 5 on a square lattice; strategies cooperate, defect, loner. With n_c cooperators and n_d defectors in the group, a cooperator earns r·n_c/(n_c + n_d) − 1, a defector r·n_c/(n_c + n_d), a loner σ, with 0 < σ < r − 1. A lone participant acts as a loner. Update: a random player x compares with a random neighbour y and adopts its strategy with probability 1/(1 + exp[(P_x − P_y + τ)/K]). The three strategies dominate each other cyclically; for r < 1 + σ only loners survive.
- Punishment [S]: a punisher pays a cost to reduce a defector's payoff (Fehr and Gächter: cooperation flourishes when this is possible and breaks down when it is not). Brandt et al.: groups of three on a lattice; cooperation and fairness are reached once players carry a reputation for punishing. The experimental 1:3 cost-to-fine ratio is [M].
- Integer form [own]: contribution unit 60 (divisible by 1..5, so shares are exact); pot = n_c · 60 · r_num / r_den; share = pot / (n_c + n_d); the remainder returns to the environment store, which also funds the (r − 1) surplus. Punishment: each punisher moves γ units of its own to the sink and β units of the defector's to the sink. Fermi rule through the sigmoid table.
- State: 2 bits strategy; 2 bytes transient payoff.
- Lesion: remove the loner option; defection should take over at low r (the paper's own comparison).

## 17. Speciation measures

- Sources: Dieckmann and Doebeli, Nature 400:354 (1999), https://pure.iiasa.ac.at/id/eprint/5700/ [S] — individual-based model with multilocus genetics in which assortative mating evolves and splits an ecologically diverging population. Dolson, Vostinar, Wiser and Ofria, "The MODES toolbox", Artificial Life 25:50 (2019), https://peerj.com/preprints/27249/ [S] — change, novelty, complexity and ecology metrics, tested on NK landscapes and Avida, with phylogenetic metrics among its keywords.
- Integer measures [own]:
  1. Genetic distance: Hamming distance by popcount on genome words.
  2. Realised gene flow: an integer matrix of matings between clusters that produced surviving offspring. Reproductive isolation between two clusters is the cross-multiplied comparison of between-cluster and within-cluster rates.
  3. Clusters: single-linkage with a distance threshold on a deterministic sample of genomes (union-find). A "species" is a cluster above a minimum size that persists for a minimum number of ticks.
  4. Ancestry: each agent carries a lineage id; births append (child lineage, parent lineage, tick) to a log. Phylogenetic diversity = sum of branch lengths in ticks; depth of the most recent common ancestor.
  5. Bimodality of a mating-relevant trait: integer histogram with a dip test on counts.
- State: lineage id u32 plus cached cluster id u16 = 6 bytes.
- Caution: a species count depends on the threshold in step 3. Report the whole curve of cluster count against threshold, not one number.

---

## State budget [own]

| Mechanism | Persistent state per agent | Shared or transient |
|---|---|---|
| 1 Spatial PD | 1 bit | 1 byte score |
| 2 Tags | 2 bytes | — |
| 3 Image scoring | 1 byte | — |
| 4 Kin markers | 8 bytes | integer sums |
| 5 Hawk-dove / attrition | 1–2 bytes | 4 bytes opponent |
| 6 Rock-paper-scissors | 2 bits per cell | — |
| 7 Sugarscape trade | 6 bytes | — |
| 8 Response thresholds (4 tasks) | 9 bytes | stimulus field |
| 9 Quorum sensing | 1 bit + 3 bytes | 2 bytes per voxel |
| 10 Mating systems | 3 bytes | 2 counters per region |
| 11 Sexual selection | 2–4 bytes | — |
| 12 Dominance | 2 bytes | — |
| 13 Niche construction | 2 bytes | 2 bytes per cell |
| 14 Food web | 3 bytes | species-pair table |
| 15 r/K life history | 4 bytes | — |
| 16 Public goods | 2 bits | 2 bytes payoff |
| 17 Speciation | 6 bytes | lineage log |

Sum with everything enabled: about 55–60 bytes per agent, under one 64-byte line. Arithmetic, not a measurement: one million agents × 1 byte = 1 MB per column, so any single mechanism's columns fit a 3 MB P-core L2; all columns together (about 60 MB) do not fit L2 + L3 (40 + 36 MB), so mechanisms should be swept one at a time over structure-of-arrays columns.

## What would be genuinely new in an engine like ours

Written conservatively. "Found no prior example" means only that the searches listed here returned none; each was a single web query unless stated, and none was a database search.

1. **All seventeen mechanisms in one evolving, spatial, million-agent world.** Query: "artificial life simulation combining trade economy sexual selection dominance hierarchy kin selection evolved agents large scale Sugarscape OR Polyworld OR Avida". Result: the components exist separately (Sugarscape for trade, Polyworld for mating and speciation, DomWorld for dominance); no single platform combining them was returned. Weak claim: combination, not invention.
2. **Sugarscape trade (rule T) at million-agent scale.** The only measured million-agent Sugarscape I found (Lysenko and D'Souza 2008) implements rules G, R and M and not trade. Integer holdings are not new (the Kehoe specification types them as naturals). What I did not find is an exact-rational implementation of the price and welfare tests; the Mesa reference uses floating point. Query: "integer arithmetic deterministic agent-based model Sugarscape trade exact conservation fixed-point bit-reproducible parallel counter-based random number".
3. **Putting b > c games on a conserving ledger.** Query: "evolutionary game payoffs energy conservation closed system artificial life cooperation energy budget donation benefit greater than cost physically grounded". Result: one close relative, a 2026 preprint on "autopoietic game theory" (https://arxiv.org/abs/2609.10817) [S], in which computation, interaction and replication draw on one energy budget and transfers cannot increase a pair's total; its social act is theft, not donation or public goods. So a conserving treatment of games is **not** new in general. I found no example that does it for donation, public-goods multiplication and trade together with integer exactness.
4. **Bit-sliced (64 cells per word) Nowak–May update.** Query: "bit-parallel bitwise prisoner's dilemma cellular automaton bit-sliced SIMD implementation spatial game lattice multi-spin coding". Nothing returned. Low confidence: multi-spin coding is standard for Ising models and this is the same trick, so an unpublished or unindexed implementation is likely.
5. **Thread-count-independent state hashes for these stochastic lattice rules.** Counter-based generators for order-independent agent models are already proposed in the literature my search returned [S] (https://arxiv.org/abs/2503.10796 notes that its parallel modes are only statistically reproducible and points to counter-based generators). Not new as an idea; I found no report of it applied to the Kerr or Sugarscape rules.

Not claimed as new: any of the seventeen mechanisms; integer or fixed-point agent models; GPU agent models at 10^6–10^7 agents.

## Open items

- Papers cited but not opened ([S] or [M] above): Nowak and May 1992, Emlen and Oring 1977, Lande 1981, Grafen 1990, May 1972, Pianka 1970, Fehr and Gächter 2002, Brandt et al. 2003, Dieckmann and Doebeli 1999, Dugatkin 1997, Bonabeau et al. 1996, Maynard Smith and Price 1973, Hamilton 1964. Their formulas here are the standard textbook forms and need checking before they enter a specification.
- Every [own] formulation is unimplemented. Each needs a small C test that reproduces the published qualitative result (for example Kerr coexistence under local but not global neighbourhoods) before it is trusted.
- FLAME GPU 2 throughput: my fetch returned a different paper; no number is reported here.
