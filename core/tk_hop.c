/* tk_hop.c - 64-spin Hopfield associative memory on pattern words.
 * See tk_hop.h for the model, the units and the known risks.
 * Freestanding: integers only, no libc, no state outside the arguments. */
#include "az_abi.h"
#include "tk_hop.h"

/* Largest |H_i| that TK_HOP_KMAX patterns can produce: 64*K + K. */
#define HOP_HMAX   (65u * TK_HOP_KMAX)
#define HOP_QLEN   384u

_Static_assert(TK_HOP_KMAX >= 1 && TK_HOP_KMAX <= 31, "the bit-sliced counter has 10 planes");
/* exact reciprocal division in hop_prob_mag needs (64*HMAX + TMAX) * 2*TMAX < 2^32 */
_Static_assert((u64)(64u * HOP_HMAX + TK_HOP_TMAX) * (2u * TK_HOP_TMAX) < 0x100000000ull,
               "reciprocal division range");

/* round(65536 * sigma(q/32)) for q = 0..383, capped at 65535. Build-time
 * constants so that no run-time libm can change the state hash; the test
 * recomputes every entry in double precision. */
static const u16 hop_sigma[HOP_QLEN] = {
    32768, 33280, 33792, 34303, 34813, 35323, 35831, 36338, 36843, 37346, 37847, 38345,
    38841, 39334, 39824, 40310, 40793, 41273, 41748, 42220, 42687, 43150, 43608, 44062,
    44511, 44954, 45393, 45826, 46254, 46677, 47094, 47505, 47911, 48310, 48704, 49092,
    49474, 49850, 50220, 50584, 50941, 51293, 51638, 51977, 52310, 52637, 52957, 53272,
    53581, 53883, 54179, 54470, 54754, 55033, 55306, 55572, 55834, 56089, 56339, 56583,
    56822, 57055, 57284, 57506, 57724, 57936, 58144, 58346, 58544, 58737, 58925, 59108,
    59287, 59462, 59632, 59797, 59959, 60116, 60270, 60419, 60565, 60706, 60844, 60979,
    61109, 61237, 61360, 61481, 61598, 61712, 61823, 61931, 62036, 62138, 62238, 62334,
    62428, 62519, 62608, 62694, 62778, 62859, 62938, 63015, 63090, 63162, 63233, 63301,
    63368, 63432, 63495, 63556, 63615, 63672, 63728, 63782, 63835, 63886, 63935, 63983,
    64030, 64075, 64119, 64162, 64203, 64244, 64283, 64321, 64357, 64393, 64427, 64461,
    64494, 64525, 64556, 64585, 64614, 64642, 64669, 64696, 64721, 64746, 64770, 64793,
    64816, 64838, 64859, 64880, 64900, 64919, 64938, 64956, 64974, 64991, 65008, 65024,
    65039, 65055, 65069, 65084, 65097, 65111, 65124, 65136, 65149, 65160, 65172, 65183,
    65194, 65204, 65215, 65224, 65234, 65243, 65252, 65261, 65269, 65277, 65285, 65293,
    65300, 65308, 65315, 65321, 65328, 65334, 65341, 65347, 65352, 65358, 65364, 65369,
    65374, 65379, 65384, 65388, 65393, 65397, 65402, 65406, 65410, 65414, 65417, 65421,
    65425, 65428, 65431, 65435, 65438, 65441, 65444, 65446, 65449, 65452, 65454, 65457,
    65459, 65462, 65464, 65466, 65468, 65470, 65472, 65474, 65476, 65478, 65480, 65482,
    65483, 65485, 65486, 65488, 65489, 65491, 65492, 65494, 65495, 65496, 65497, 65499,
    65500, 65501, 65502, 65503, 65504, 65505, 65506, 65507, 65508, 65509, 65509, 65510,
    65511, 65512, 65513, 65513, 65514, 65515, 65515, 65516, 65517, 65517, 65518, 65518,
    65519, 65519, 65520, 65520, 65521, 65521, 65522, 65522, 65523, 65523, 65523, 65524,
    65524, 65525, 65525, 65525, 65526, 65526, 65526, 65527, 65527, 65527, 65527, 65528,
    65528, 65528, 65528, 65529, 65529, 65529, 65529, 65530, 65530, 65530, 65530, 65530,
    65530, 65531, 65531, 65531, 65531, 65531, 65531, 65532, 65532, 65532, 65532, 65532,
    65532, 65532, 65532, 65533, 65533, 65533, 65533, 65533, 65533, 65533, 65533, 65533,
    65533, 65533, 65534, 65534, 65534, 65534, 65534, 65534, 65534, 65534, 65534, 65534,
    65534, 65534, 65534, 65534, 65534, 65534, 65535, 65535, 65535, 65535, 65535, 65535,
    65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535,
    65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535,
    65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535,
};

AZ_INLINE u32 hop_k(u32 k) { return k < TK_HOP_KMAX ? k : TK_HOP_KMAX; }

AZ_INLINE i32 hop_overlap(u64 p, u64 s)
{
    return 64 - 2 * (i32)__builtin_popcountll(p ^ s);
}

/* H_i of spin b from precomputed overlaps; si is the spin value (+1 / -1).
 * Branch-free: the pattern bits are data, a branch on them mispredicts. */
AZ_INLINE i32 hop_field_at(const u64 *patterns, const i32 *o, u32 kk, u32 b, i32 si)
{
    i32 h = -(i32)kk * si;
    for (u32 m = 0; m < kk; m++) {
        i32 neg = (i32)((patterns[m] >> b) & 1u) - 1;      /* 0 if the bit is set, -1 if not */
        h += (o[m] ^ neg) - neg;
    }
    return h;
}

i32 tk_hop_field(const u64 *patterns, u32 k, u64 state, u32 site)
{
    u32 kk = hop_k(k);
    u32 b = site & 63u;
    i32 o[TK_HOP_KMAX];
    for (u32 m = 0; m < kk; m++) o[m] = hop_overlap(patterns[m], state);
    return hop_field_at(patterns, o, kk, b, (i32)((state >> b) & 1u) * 2 - 1);
}

/* Bit-sliced synchronous update.
 *
 * A pattern with a negative overlap is replaced by its complement (the
 * product xi_i * o_mu is unchanged), so every weight is |o_mu| = 2*v_mu with
 * v_mu in 0..32. With T_i = sum of v_mu over the patterns whose bit i is
 * set and V = sum of v_mu:
 *     sum_mu xi_i^mu o_mu = 4*T_i - 2*V,   H_i = 4*T_i - 2*V - K*s_i.
 * T_i for all 64 spins is accumulated in vertical counters (one word per
 * binary digit), then compared with two scalar thresholds:
 *     a +1 spin stays +1   iff H_i >= 0  iff T_i >= ceil((2V + K) / 4)
 *     a -1 spin becomes +1 iff H_i >  0  iff T_i >  (2V - K) / 4
 * which is the sign rule with "a tie keeps the spin". */
u64 tk_hop_step(const u64 *patterns, u32 k, u64 state)
{
    u32 kk = hop_k(k);
    if (kk == 0) return state;

    /* digits needed for T_i <= 32*kk */
    u32 planes = 32u - (u32)__builtin_clz(32u * kk);
    u64 plane[10];
    for (u32 j = 0; j < planes; j++) plane[j] = 0;

    u32 vsum = 0;
    for (u32 m = 0; m < kk; m++) {
        u64 p = patterns[m];
        u32 d = (u32)__builtin_popcountll(p ^ state);
        u32 neg = d > 32u;
        u32 v = neg ? d - 32u : 32u - d;
        u64 q = neg ? ~p : p;
        u64 carry = 0;
        vsum += v;
        /* v <= 32 has six digits (planes >= 6 always): full adders there,
         * then only the carry ripples on */
        for (u32 j = 0; j < 6u; j++) {
            u64 a = (0ull - (u64)((v >> j) & 1u)) & q;
            u64 t = plane[j];
            plane[j] = t ^ a ^ carry;
            carry = (t & a) | (carry & (t ^ a));
        }
        for (u32 j = 6u; j < planes; j++) {
            u64 t = plane[j];
            plane[j] = t ^ carry;
            carry &= t;
        }
    }

    u32 th_stay = (2u * vsum + kk + 3u) >> 2;
    u32 th_rise = (2u * vsum >= kk) ? ((2u * vsum - kk) >> 2) + 1u : 0u;

    /* borrow of T_i - threshold, per spin: set means T_i < threshold */
    u64 lt_stay = 0, lt_rise = 0;
    for (u32 j = 0; j < planes; j++) {
        u64 nt = ~plane[j];
        u64 cs = 0ull - (u64)((th_stay >> j) & 1u);
        u64 cr = 0ull - (u64)((th_rise >> j) & 1u);
        lt_stay = (nt & (cs | lt_stay)) | (cs & lt_stay);
        lt_rise = (nt & (cr | lt_rise)) | (cr & lt_rise);
    }
    return (state & ~lt_stay) | (~state & ~lt_rise);
}

bool tk_hop_is_fixed(const u64 *patterns, u32 k, u64 state)
{
    return tk_hop_step(patterns, k, state) == state;
}

u64 tk_hop_recall(const u64 *patterns, u32 k, u64 cue, u32 max_iters)
{
    u32 n = max_iters < TK_HOP_ITERS ? max_iters : TK_HOP_ITERS;
    u64 prev = cue, s = cue;
    for (u32 i = 0; i < n; i++) {
        u64 nx = tk_hop_step(patterns, k, s);
        if (nx == s) return s;
        if (nx == prev) {
            /* 2-cycle {s, nx}: pick one member independent of the parity of n */
            u32 ds = (u32)__builtin_popcountll(s ^ cue);
            u32 dn = (u32)__builtin_popcountll(nx ^ cue);
            if (ds != dn) return ds < dn ? s : nx;
            return s < nx ? s : nx;
        }
        prev = s;
        s = nx;
    }
    return s;
}

i32 tk_hop_energy(const u64 *patterns, u32 k, u64 state)
{
    u32 kk = hop_k(k);
    i32 e = 64 * (i32)kk;
    for (u32 m = 0; m < kk; m++) {
        i32 o = hop_overlap(patterns[m], state);
        e -= o * o;
    }
    return e;
}

u64 tk_hop_step_one(const u64 *patterns, u32 k, u64 state, u32 site)
{
    u64 bit = 1ull << (site & 63u);
    i32 h = tk_hop_field(patterns, k, state, site);
    if (h > 0) return state | bit;
    if (h < 0) return state & ~bit;
    return state;
}

u64 tk_hop_sweep(const u64 *patterns, u32 k, u64 state)
{
    u32 kk = hop_k(k);
    i32 o[TK_HOP_KMAX];
    u64 s = state;
    for (u32 m = 0; m < kk; m++) o[m] = hop_overlap(patterns[m], s);
    for (u32 b = 0; b < 64u; b++) {
        i32 si = (i32)((s >> b) & 1u) * 2 - 1;
        i32 h = hop_field_at(patterns, o, kk, b, si);
        u32 flip = (u32)(h * si) >> 31;                    /* 1 iff the field opposes the spin */
        i32 d = 2 * si * (i32)flip;
        s ^= (u64)flip << b;
        /* a flipped spin now agrees with the patterns it disagreed with */
        for (u32 m = 0; m < kk; m++) {
            i32 xi = (i32)((patterns[m] >> b) & 1u) * 2 - 1;
            o[m] -= xi * d;
        }
    }
    return s;
}

u64 tk_hop_settle(const u64 *patterns, u32 k, u64 state, u32 max_sweeps)
{
    u32 n = max_sweeps < TK_HOP_ITERS ? max_sweeps : TK_HOP_ITERS;
    u64 s = state;
    for (u32 i = 0; i < n; i++) {
        u64 nx = tk_hop_sweep(patterns, k, s);
        if (nx == s) break;
        s = nx;
    }
    return s;
}

/* Probability of +1 for a field of magnitude a >= 0 at temperature t >= 1.
 * recip = floor(2^32 / 2t) + 1 turns the rounding division into a multiply
 * (exact in this range, see the static assert). */
AZ_INLINE u32 hop_prob_mag(u32 a, u32 t, u32 recip)
{
    u32 q = (u32)(((u64)(64u * a + t) * recip) >> 32);
    return q < HOP_QLEN ? hop_sigma[q] : TK_HOP_P_ONE;
}

AZ_INLINE u32 hop_recip(u32 t) { return (u32)(0x100000000ull / (2u * t)) + 1u; }

u32 tk_hop_prob_up(i32 h, u32 temperature)
{
    if (temperature == 0) return h > 0 ? TK_HOP_P_ONE : (h < 0 ? 0u : TK_HOP_P_ONE / 2u);
    u32 t = temperature < TK_HOP_TMAX ? temperature : TK_HOP_TMAX;
    u32 a = h < 0 ? 0u - (u32)h : (u32)h;
    if (a > HOP_HMAX) a = HOP_HMAX;
    u32 pm = hop_prob_mag(a, t, hop_recip(t));
    return h >= 0 ? pm : TK_HOP_P_ONE - pm;
}

/* next 16 bits of the stream at bit position pos <= 256; r is the 256-bit
 * stream followed by two zero words, so no bounds branch is needed */
AZ_INLINE u32 hop_peek16(const u64 r[6], u32 pos)
{
    u32 w = pos >> 6, off = pos & 63u;
    u64 v = (r[w] << off) | ((r[w + 1u] >> 1) >> (63u - off));
    return (u32)(v >> 48);
}

u64 tk_hop_heatbath(const u64 *patterns, u32 k, u64 state, u32 temperature,
                    const u64 rnd[4])
{
    if (temperature == 0) return tk_hop_step(patterns, k, state);

    u32 kk = hop_k(k);
    u32 t = temperature < TK_HOP_TMAX ? temperature : TK_HOP_TMAX;
    u32 recip = hop_recip(t);
    i32 o[TK_HOP_KMAX];
    for (u32 m = 0; m < kk; m++) o[m] = hop_overlap(patterns[m], state);

    const u64 r[6] = { rnd[0], rnd[1], rnd[2], rnd[3], 0, 0 };
    u64 out = 0;
    u32 pos = 0;
    for (u32 b = 0; b < 64u; b++) {
        i32 si = (i32)((state >> b) & 1u) * 2 - 1;
        i32 h = hop_field_at(patterns, o, kk, b, si);
        u32 pm = hop_prob_mag((u32)(h < 0 ? -h : h), t, recip);
        u32 p = h >= 0 ? pm : TK_HOP_P_ONE - pm;
        /* Lazy comparison "uniform < p": only the bits up to the first one
         * that differs from p are consumed. u < p is also right for the
         * certain cases p = 0 and p = 65536, which consume nothing.
         * Written without branches: the outcome is random by design. */
        u32 u = hop_peek16(r, pos);
        u32 x = u ^ (p & 0xFFFFu);
        u32 certain = (u32)(p == 0) | (u32)(p >= TK_HOP_P_ONE);
        u32 used = (x ? (u32)__builtin_clz(x) - 15u : 16u) & (certain - 1u);
        u32 dry = (u32)(pos + used > 256u);                /* the stream ran out */
        u32 det = (u32)(h > 0) | ((u32)(h == 0) & (u32)(si > 0));
        u32 up = dry ? det : (u32)(u < p);
        pos = dry ? 256u : pos + used;
        out |= (u64)up << b;
    }
    return out;
}

u32 tk_hop_store(u64 *patterns, u32 cap, u32 *k, u64 pattern)
{
    u32 c = hop_k(cap);
    if (c == 0) return TK_HOP_NONE;
    u32 n = *k < c ? *k : c;
    u32 best = 0, best_abs = 0;
    *k = n;
    for (u32 m = 0; m < n; m++) {
        u32 d = (u32)__builtin_popcountll(patterns[m] ^ pattern);
        u32 a = d > 32u ? 2u * d - 64u : 64u - 2u * d;
        if (a == 64u) return m;
        if (a > best_abs) { best_abs = a; best = m; }
    }
    if (n < c) {
        patterns[n] = pattern;
        *k = n + 1u;
        return n;
    }
    patterns[best] = pattern;
    return best;
}
