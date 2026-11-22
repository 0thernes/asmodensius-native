/* az_tier1.c - slow cognition: what a creature learns, remembers and feels.
 *
 * Once in tier1_period ticks (az_tier1_due) a creature that has a working
 * brain (not an egg, not a cyst) takes one step of its inner life. Nothing
 * here is a script: every rate is its own trait, every unit a world
 * parameter (core/params/az_tier1.def), and all of it acts on the creature's
 * own brain through the eight modulation lanes of its AzBrain header (gain
 * and bias per hidden lane h & 7) and through its arousal, which it senses.
 *
 *  1. Reward and surprise. reward = energy gained since the last visit;
 *     surprise = reward - rbase, the baseline it has come to expect (rbase
 *     follows the reward by 1 / 2^TIER1_BASE_SHIFT per visit). The signed
 *     surprise on a scale of 255 = TIER1_SURPRISE_EU is the teaching signal
 *     (reward prediction error: Schultz, Dayan and Montague 1997).
 *  2. Arousal decays by AROUSAL_DECAY, rises with the size of the surprise
 *     by AROUSAL_GAIN and wanders by MOODINESS.
 *  3. Eligibility. Each lane keeps a trace of how far its hidden units were
 *     above the creature's own mean activity; PLASTICITY is how much of the
 *     trace outlasts a visit.
 *  4. The state vector (tk_qsv; lesion TIER1_Q). The context ctx[8] is read
 *     as the eight angles of a program of 1 + Q_DEPTH / 8 gates on three
 *     qubits. The state is kept in qstate; one sample of it names a lane,
 *     which gets Q_COUPLE / 8 of eligibility and Q_COUPLE / 16 of context:
 *     the creature's own way of trying a lane.
 *  5. Memory (tk_hop; lesion MEMORY). The cue is the present as 64 spins
 *     (each sense above or below the middle). The working set (engram, at
 *     most AZ_HOP_PATTERNS words) recalls from the cue at a temperature that
 *     falls with HOPFIELD; the recalled word moves the context, one byte per
 *     lane. A surprise is stored with a chance that rises with MEMORY. With
 *     a chance of MEMORY * HOPFIELD / 2^16 the stored state also goes to the
 *     long-term memory (az_memory, any length): the creature asks for one
 *     more word, and writes it at a later visit for TIER1_WORD_EU of energy,
 *     paid as heat. A long-term word that agrees with the cue in
 *     TIER1_FAMILIAR of 64 spins is recognised: it calms (arousal falls by
 *     HOPFIELD / 8) and comes back into the working set. A creature that
 *     cannot pay for a word gives its newest one up.
 *  6. The neighbourhood (tk_sh; lesion TIER1_SH). The directions to the
 *     neighbours give a power spectrum of degree 0 to 3 (how one-sided, how
 *     flat, how branched the crowd around it is); ATTENTION pulls the gains
 *     of the lanes toward it.
 *  7. Culture. In a colony, CONFORMITY pulls the context toward the colony's
 *     shared prototype.
 *  8. Learning (lesion PLASTICITY). The raw step of a lane is surprise *
 *     eligibility (a three-factor rule: Fremaux and Gerstner 2016). It is
 *     divided by the lane's entry of the Fubini-Study metric of the state
 *     vector with respect to its angle (tk_qgt, natural gradient: Amari
 *     1998; Stokes et al. 2020; lesion METRIC and lesion TIER1_Q use the
 *     identity), scaled so that the mean lane keeps its size. LEARN_RATE of
 *     it moves the lane's bias, half of that its gain and its context.
 *     EXPLORE, and IMPROV times the brain's own drive on AZ_M_IMPROV, add
 *     noise of the creature's own to the bias and to the trace, so that a
 *     try that pays is kept.
 *  9. CTX_GAIN couples the context to the bias: the bias moves by the change
 *     of ctx * CTX_GAIN / 512.
 * 10. az_classify measures what kind of creature it has been.
 *
 * No matter moves here, and the only energy is the price of a memory word.
 * A brain with no hidden block (n_hb == 0) has no lane activity: its traces
 * decay and everything else runs as for any other.
 */
#include "az_abi.h"
#include "tk_hop.h"
#include "tk_qgt.h"
#include "tk_qsv.h"
#include "tk_sh.h"

AZ_INLINE u32 wp(const AzWorld *w, u32 id) { return w->param[id]; }
AZ_INLINE i8 sat_i8(i32 v) { return (i8)az_clamp_i32(v, -127, 127); }

#define T1_SHIFT_H 2048u     /* the half step of the angle derivative: 8 units of context */

typedef struct T1 {          /* working room of one visit, in the worker's scratch */
    i32 off[AZ_NEIGH_MAX][3];
    u32 os[AZ_NEIGH_MAX];
    i64 acc[TK_SH_N];
    i32 g[64], f[64];
    i16 psi[TK_QSV_LEN], hi[TK_QSV_LEN], lo[TK_QSV_LEN];
    i16 dpsi[8 * TK_QSV_LEN];
    u16 ang[TK_QSV_SLOTS];
    u8 prog[48];
} T1;

/* the creature's program: a Hadamard on each qubit, then 1 + Q_DEPTH / 8
 * gates: rotations about y and z by the context angles, every fourth an
 * entangling CNOT. Returns the number of ops; *used = the angle slots read. */
static u32 q_program(u8 *prog, u32 depth, u32 *used)
{
    u32 n = 0, mask = 0;
    for (u32 q = 0; q < 3u; q++) prog[n++] = TK_QSV_OP_H(q);
    const u32 gates = 1u + (depth >> 3);
    for (u32 j = 0; j < gates; j++) {
        const u32 q = j % 3u, slot = j & 7u;
        if ((j & 3u) == 3u) prog[n++] = TK_QSV_OP_CNOT(q, (q + 1u) % 3u);
        else { prog[n++] = (j & 1u) ? TK_QSV_OP_RZ(q, slot) : TK_QSV_OP_RY(q, slot); mask |= 1u << slot; }
    }
    *used = mask;
    return n;
}

/* the state for the angles in x->ang, and its metric with respect to them (g) */
AZ_NOINLINE static bool q_state(T1 *x, u32 n_ops, u32 used)
{
    tk_qsv_reset(x->psi);
    tk_qsv_run(x->psi, x->prog, n_ops, x->ang);
    for (u32 l = 0; l < 8u; l++) {
        i16 *d = &x->dpsi[l * TK_QSV_LEN];
        if (!(used & (1u << l))) { for (u32 i = 0; i < TK_QSV_LEN; i++) d[i] = 0; continue; }
        const u16 a = x->ang[l];
        x->ang[l] = (u16)(a + T1_SHIFT_H);
        tk_qsv_reset(x->hi); tk_qsv_run(x->hi, x->prog, n_ops, x->ang);
        x->ang[l] = (u16)(a - T1_SHIFT_H);
        tk_qsv_reset(x->lo); tk_qsv_run(x->lo, x->prog, n_ops, x->ang);
        x->ang[l] = a;
        for (u32 i = 0; i < TK_QSV_LEN; i++) d[i] = (i16)az_clamp_i32((i32)x->hi[i] - (i32)x->lo[i], -32767, 32767);
    }
    return tk_qgt(x->psi, x->dpsi, TK_QSV_DIM, 8, x->g, x->f);
}

/* the neighbourhood as four numbers of 0..127: how many, how one-sided, how flat, how branched */
AZ_NOINLINE static bool neighbourhood(const AzWorld *w, u32 s, u32 worker, T1 *x, u32 d[4])
{
    const u32 n = az_scan_list(w, s, x->off, x->os, AZ_NEIGH_MAX, worker);
    if (!n) return false;
    u32 power[TK_SH_LMAX + 1];
    for (u32 i = 0; i < TK_SH_N; i++) x->acc[i] = 0;
    for (u32 i = 0; i < n; i++) tk_sh_accumulate(x->acc, x->off[i], 1);
    tk_sh_power(x->acc, n, power);
    d[0] = az_min_u32(127u, n * 4u);
    for (u32 j = 1; j < 4u; j++) d[j] = az_min_u32(127u, power[j] >> 9);
    return true;
}

static u32 agree64(u64 a, u64 b) { return 64u - (u32)__builtin_popcountll(a ^ b); }

/* memory: recall, storing, the long-term words. Returns the word the working set recalled (0: none). */
AZ_NOINLINE static u64 remember(AzWorld *w, u32 s, u32 worker, const u8 *t, const u8 *sens, u32 surprise, i32 *calm)
{
    const u64 uid = w->uid[s];
    u64 cue = 0;
    for (u32 i = 0; i < 64u; i++) cue |= (u64)(sens[i] >= 64u) << i;
    u64 *eng = &w->engram[(size_t)s * AZ_HOP_PATTERNS];
    u32 k = w->hop_n[s];
    u64 recalled = 0;
    if (k) {
        u32 r[8];
        az_rand4(w, uid, AZ_RS_MEMORY, 0, r);
        az_rand4(w, uid, AZ_RS_MEMORY, 4, r + 4);
        const u64 rnd[4] = { (u64)r[0] << 32 | r[1], (u64)r[2] << 32 | r[3], (u64)r[4] << 32 | r[5], (u64)r[6] << 32 | r[7] };
        const u32 temp = az_min_u32(TK_HOP_TMAX, ((255u - t[AZ_T_HOPFIELD]) * wp(w, AZ_WP_TIER1_HOP_TEMP)) >> 7);
        recalled = tk_hop_heatbath(eng, k, cue, temp, rnd);
        w->memory[s] = recalled;
    }
    const u32 roll = az_rand(w, uid, AZ_RS_MEMORY, 8);
    const bool store = cue != 0 && ((surprise * t[AZ_T_MEMORY]) >> 8) > (roll & 255u);
    if (store) {
        (void)tk_hop_store(eng, AZ_HOP_PATTERNS, &k, cue);
        w->hop_n[s] = (u8)k;
    }
    /* the long-term memory: any number of words, each paid for */
    u32 words;
    u64 *mem = az_memory(w, s, &words);
    const bool open = words && mem[words - 1u] == 0;         /* a word asked for and not yet written */
    const i32 price = (i32)az_min_u32(wp(w, AZ_WP_TIER1_WORD_EU), 0x7FFFFFFFu);
    if (open && cue != 0) {
        if (w->energy[s] >= price) {
            mem[words - 1u] = cue;
            if (price > 0) { w->energy[s] -= price; az_cell_post_heat(w, worker, w->cell[s], price); }
        } else az_memory_request(w, s, words - 1u);          /* it cannot pay: the word is given up */
    } else if (!open) {
        u32 best = 0, at = 0;
        for (u32 i = 0; i < words; i++) {
            const u32 a = agree64(mem[i], cue);
            if (a > best) { best = a; at = i; }
        }
        if (words && best >= wp(w, AZ_WP_TIER1_FAMILIAR)) {
            *calm = (i32)(t[AZ_T_HOPFIELD] >> 3);            /* it has been here before */
            (void)tk_hop_store(eng, AZ_HOP_PATTERNS, &k, mem[at]);
            w->hop_n[s] = (u8)k;
        } else if (store && ((roll >> 8) & 0xFFFFu) < (u32)t[AZ_T_MEMORY] * t[AZ_T_HOPFIELD]) {
            if (w->energy[s] >= price) az_memory_request(w, s, words + 1u);
            else if (words) az_memory_request(w, s, words - 1u);
        }
    }
    return recalled;
}

static void visit(AzWorld *w, u32 s, u32 worker)
{
    const u8 fl = w->flags[s];
    const i32 energy = w->energy[s];
    if (fl & (AZ_F_EGG | AZ_F_DORMANT)) { w->rprev[s] = energy; return; }
    u32 bbytes, mbytes;
    AzBrain *br = az_brain(w, s, &bbytes);
    const u8 *mind = (const u8 *)az_seg(w, s, AZ_SEG_MIND, &mbytes);
    if (bbytes < sizeof(AzBrain) || mbytes < AZ_SENS) { w->rprev[s] = energy; return; }
    const u8 *t = &w->trait[(size_t)s * AZ_TRAITS];
    const u64 uid = w->uid[s];
    const u32 lesion = w->cfg.lesion;
    T1 *x = (T1 *)az_scratch(w, worker);
    i8 *elig = &w->elig[(size_t)s * 8];
    i8 *ctx = &w->ctx[(size_t)s * 8];
    u32 rr[4];
    az_rand4(w, uid, AZ_RS_TIER1, 0, rr);

    /* 1. reward and surprise */
    const i32 reward = (i32)az_clamp_i64((i64)energy - (i64)w->rprev[s], -32767, 32767);
    w->rprev[s] = energy;
    const i32 delta = reward - (i32)w->rbase[s];
    w->rbase[s] = (i16)az_clamp_i32((i32)w->rbase[s] + delta / (i32)(1u << wp(w, AZ_WP_TIER1_BASE_SHIFT)), -32767, 32767);
    const i32 ds = (i32)az_clamp_i64(((i64)delta * 255) / (i64)wp(w, AZ_WP_TIER1_SURPRISE_EU), -255, 255);
    const u32 surprise = (u32)az_abs_i32(ds);

    /* 3. eligibility: how far each lane was above the creature's own mean activity */
    const u32 n_hb = az_min_u32(br->n_hb, (mbytes - AZ_SENS) / 8u);
    const u8 *hid = mind + AZ_SENS;
    i32 act[8], mean = 0;
    for (u32 l = 0; l < 8u; l++) {
        u32 a = 0;
        for (u32 b = 0; b < n_hb; b++) a += hid[b * 8u + l];
        act[l] = n_hb ? (i32)(a / n_hb) : 0;
        mean += act[l];
    }
    mean /= 8;
    for (u32 l = 0; l < 8u; l++)
        elig[l] = sat_i8(((i32)elig[l] * (i32)t[AZ_T_PLASTICITY]) / 256 + (n_hb ? act[l] - mean : 0));

    i32 dctx[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

    /* 4. the state vector and its sample */
    bool metric = false;
    if (!(lesion & AZ_LESION_TIER1_Q)) {
        u32 used;
        const u32 n_ops = q_program(x->prog, t[AZ_T_Q_DEPTH], &used);
        for (u32 l = 0; l < 8u; l++) x->ang[l] = (u16)((i32)ctx[l] * 256);
        metric = q_state(x, n_ops, used);
        i16 *qs = &w->qstate[(size_t)s * TK_QSV_LEN];
        for (u32 i = 0; i < TK_QSV_LEN; i++) qs[i] = x->psi[i];
        const u32 lane = tk_qsv_sample(x->psi, az_rand(w, uid, AZ_RS_QSV, 0)) & 7u;
        elig[lane] = sat_i8((i32)elig[lane] + (i32)(t[AZ_T_Q_COUPLE] >> 3));
        dctx[lane] += (i32)(t[AZ_T_Q_COUPLE] >> 4);
    }

    /* 5. memory */
    i32 calm = 0;
    if (!(lesion & AZ_LESION_MEMORY)) {
        const u64 m = remember(w, s, worker, t, mind, surprise, &calm);
        if (m)
            for (u32 l = 0; l < 8u; l++)
                dctx[l] += (((i32)__builtin_popcountll((m >> (8u * l)) & 255u) - 4) * (i32)t[AZ_T_HOPFIELD]) / 64;
    }

    /* 2. arousal */
    {
        i32 a = w->arousal[s];
        a -= (a * ((i32)t[AZ_T_AROUSAL_DECAY] + 1)) >> 8;
        a += (i32)((surprise * t[AZ_T_AROUSAL_GAIN]) >> 8);
        a += (((i32)(rr[0] & 255u) - 128) * (i32)t[AZ_T_MOODINESS]) / 1024;
        a -= calm;
        w->arousal[s] = az_sat_u8(a);
    }

    /* 6. the neighbourhood pulls the gains */
    if (!(lesion & AZ_LESION_TIER1_SH) && t[AZ_T_ATTENTION]) {
        u32 d[4];
        if (neighbourhood(w, s, worker, x, d))
            for (u32 l = 0; l < 8u; l++) {
                const i32 target = 32 + (i32)d[l & 3u];
                br->gain[l] = (u8)az_clamp_i32((i32)br->gain[l] + ((target - (i32)br->gain[l]) * (i32)t[AZ_T_ATTENTION]) / 1024, 1, 255);
            }
    }

    /* 7. the colony's culture pulls the context */
    const u32 col = w->colony[s];
    if (col && col < w->n_colonies && t[AZ_T_CONFORMITY]) {
        const u8 *cu = w->colonies[col].culture;
        for (u32 l = 0; l < 8u; l++)
            dctx[l] += (((i32)cu[l] - 128 - (i32)ctx[l]) * (i32)t[AZ_T_CONFORMITY]) / 1024;
    }

    /* 8. learning */
    if (!(lesion & AZ_LESION_PLASTICITY)) {
        i32 raw[8], step[8];
        for (u32 l = 0; l < 8u; l++) raw[l] = ds * (i32)elig[l];
        const i32 floor_q = (i32)az_min_u32(wp(w, AZ_WP_TIER1_METRIC_FLOOR), 0x7FFFFFFFu);
        if (metric && !(lesion & AZ_LESION_METRIC)) {
            i64 sum = 0;
            for (u32 l = 0; l < 8u; l++) sum += az_max_i32(x->g[l * 8u + l], floor_q);
            const i64 gbar = sum / 8;                       /* the mean lane keeps its size */
            for (u32 l = 0; l < 8u; l++) raw[l] = (i32)az_clamp_i64(((i64)raw[l] * gbar) >> 16, -0x3FFFFFFF, 0x3FFFFFFF);
            (void)tk_qgt_precondition(step, raw, x->g, 8, floor_q);
        } else {
            (void)tk_qgt_precondition(step, raw, NULL, 8, 1);
        }
        const u32 shift = wp(w, AZ_WP_TIER1_LEARN_SHIFT);
        const i32 drive = w->motor[(size_t)s * AZ_MOT + AZ_M_IMPROV];
        const u32 amp = az_min_u32(255u, (u32)(t[AZ_T_EXPLORE] >> 1) + (drive > 0 ? ((u32)t[AZ_T_IMPROV] * (u32)drive) >> 7 : 0u));
        for (u32 l = 0; l < 8u; l++) {
            const i32 move = (i32)az_clamp_i64(((i64)step[l] * (i64)t[AZ_T_LEARN_RATE]) / ((i64)1 << shift), -127, 127);
            const i32 noise = (((i32)((rr[1u + (l >> 2)] >> (8u * (l & 3u))) & 255u) - 128) * (i32)amp) / 4096;
            br->bias[l] = sat_i8((i32)br->bias[l] + move + noise);
            br->gain[l] = (u8)az_clamp_i32((i32)br->gain[l] + move / 2, 1, 255);
            elig[l] = sat_i8((i32)elig[l] + noise * 4);
            dctx[l] += move / 2;
        }
    }

    /* 9. the context, and its coupling to the bias */
    {
        const i32 leak = (i32)wp(w, AZ_WP_TIER1_CTX_LEAK);
        for (u32 l = 0; l < 8u; l++) {
            const i32 old = ctx[l];
            const i32 now = az_clamp_i32(old - old / leak + dctx[l], -127, 127);
            ctx[l] = (i8)now;
            br->bias[l] = sat_i8((i32)br->bias[l] + (now * (i32)t[AZ_T_CTX_GAIN]) / 512 - (old * (i32)t[AZ_T_CTX_GAIN]) / 512);
        }
    }
}

void az_tier1_range(AzWorld *w, u32 begin, u32 end, u32 worker)
{
    for (u32 s = begin; s < end; s++)
        if ((w->flags[s] & AZ_F_ALIVE) && w->blk[s] && az_tier1_due(w, w->uid[s])) visit(w, s, worker);
}
