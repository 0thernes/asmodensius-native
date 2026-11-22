/* tk_hop.h - 64-spin Hopfield associative memory, patterns stored as words.
 *
 * Derived from the classical-spin modules of tsotchke's
 * spin_based_neural_network (MIT licence, Copyright (c) 2024-2026 tsotchke):
 * src/thermodynamic/hopfield.c (Hebbian store, synchronous sign update,
 * recall, energy) and src/neuromorphic/pbit.c (heat-bath sampler). No
 * upstream text is carried over; the unmodified upstream file is the test
 * oracle in tests/upstream/spin_nn/.
 *
 * Model. N = 64 spins, bit i of a word = spin i, bit 1 = +1, bit 0 = -1.
 * K stored patterns xi^mu are K words. The Hebbian weight matrix
 *     J_ij = (1/N) sum_mu xi_i^mu xi_j^mu   (i != j),   J_ii = 0
 * is never built. With the overlaps o_mu = 64 - 2*popcount(xi^mu XOR s) the
 * integer local field is
 *     H_i = N * sum_j J_ij s_j = sum_mu xi_i^mu * o_mu  -  K * s_i
 * (the last term removes the self-coupling). Update rule: s_i <- sign(H_i),
 * and a spin with H_i == 0 keeps its value. Upstream sets a tied spin to +1
 * instead; a tie needs an even K (H_i has the parity of K), so for odd K the
 * two rules are the same function.
 *
 * Energy unit. tk_hop_energy = 64*K - sum_mu o_mu^2 = 128 * E_upstream,
 * where E_upstream = -(1/2) s^T J s. Flipping spin i changes it by
 * 4 * s_i * H_i, so every flip made by a one-spin update lowers it by
 * 4*|H_i| >= 4 and one-spin (asynchronous) dynamics always ends in a fixed
 * point.
 *
 * Risks that survive the port (see tests/test_tk_hop.c for measurements):
 *  - Synchronous updates do NOT have that guarantee: tk_hop_step can enter a
 *    2-cycle (a <-> b). tk_hop_recall detects it and stops; check the result
 *    with tk_hop_is_fixed, or use tk_hop_settle, which cannot cycle.
 *  - The complement of every stored pattern is an attractor too.
 *  - Reliable capacity for random patterns is about 6 of 64.
 *
 * Everything here is a pure function of its arguments: integers only, no
 * memory beyond the arguments, no RNG of its own. Thread-safe.
 */
#ifndef TK_HOP_H
#define TK_HOP_H

#include "az_types.h"

#define TK_HOP_N      64u          /* spins per memory                        */
#define TK_HOP_KMAX   16u          /* patterns used at most; larger k is clamped */
#define TK_HOP_ITERS  256u         /* hard cap on recall / settle iterations  */
#define TK_HOP_TMAX   4095u        /* temperatures above this are clamped     */
#define TK_HOP_NONE   0xFFFFFFFFu  /* tk_hop_store: nothing was written       */
#define TK_HOP_P_ONE  65536u       /* probability 1 in tk_hop_prob_up units   */

/* Integer local field H_i of one spin (site 0..63; larger sites wrap). */
i32  tk_hop_field(const u64 *patterns, u32 k, u64 state, u32 site);

/* One synchronous update of all 64 spins (bit-sliced). */
u64  tk_hop_step(const u64 *patterns, u32 k, u64 state);

/* true if state is a fixed point of tk_hop_step (and of every one-spin update). */
bool tk_hop_is_fixed(const u64 *patterns, u32 k, u64 state);

/* Synchronous recall: at most max_iters steps from cue. Stops at a fixed
 * point. If a 2-cycle is entered it stops and returns the cycle member
 * nearer to the cue (the smaller word if both are equally near), so the
 * result does not depend on the parity of max_iters. */
u64  tk_hop_recall(const u64 *patterns, u32 k, u64 cue, u32 max_iters);

/* Energy in units of 1/128 of the upstream energy; see the header comment. */
i32  tk_hop_energy(const u64 *patterns, u32 k, u64 state);

/* One-spin update of spin `site` (the asynchronous variant). */
u64  tk_hop_step_one(const u64 *patterns, u32 k, u64 state, u32 site);

/* One asynchronous sweep: spins 0..63 in order, each seeing the spins
 * already updated in this sweep. Never raises the energy. */
u64  tk_hop_sweep(const u64 *patterns, u32 k, u64 state);

/* Asynchronous recall: sweeps until nothing changes (at most max_sweeps). */
u64  tk_hop_settle(const u64 *patterns, u32 k, u64 state, u32 max_sweeps);

/* Probability, in units of 1/65536, that a spin with field h is +1 after a
 * heat-bath update: round(65536 * sigma(q/32)) with q = round(32*|h|/t),
 * mirrored for h < 0, from a 384-entry integer table (certain beyond it).
 * Temperature unit: t = 32 is the upstream temperature T = 1 (= coupling
 * scale), i.e. P(+1) = sigma(2 * beta * H_i / 64) with beta = 32/t.
 * t = 0 returns 0, 32768 (tie) or 65536. */
u32  tk_hop_prob_up(i32 h, u32 temperature);

/* One synchronous heat-bath update: every spin is redrawn from
 * tk_hop_prob_up(H_i, temperature), all fields taken from the old state.
 * rnd[0..3] is a 256-bit stream of random bits supplied by the caller
 * (two Philox4x32 blocks), consumed from bit 63 of rnd[0] downwards, spins
 * in order 0..63. A spin compares the stream with its 16-bit probability
 * bit by bit and stops at the first difference (2 bits on average), spins
 * with probability 0 or 1 consume nothing. If the stream runs out (for
 * random bits the probability is below 1e-16: fewer than 64 differing bits
 * in 256) the remaining spins take the deterministic rule.
 * temperature == 0 is exactly tk_hop_step and ignores rnd. */
u64  tk_hop_heatbath(const u64 *patterns, u32 k, u64 state, u32 temperature,
                     const u64 rnd[4]);

/* Hebbian storage = writing a word into a slot. If the word or its
 * complement is already stored, nothing changes (both give the same J).
 * Otherwise it is appended while *k < cap; when full it overwrites the
 * stored pattern most similar to it (largest |overlap|, lowest slot on a
 * tie), which keeps the store decorrelated. Returns the slot that now
 * holds the pattern, or TK_HOP_NONE if cap == 0. cap is clamped to
 * TK_HOP_KMAX. */
u32  tk_hop_store(u64 *patterns, u32 cap, u32 *k, u64 pattern);

#endif /* TK_HOP_H */
