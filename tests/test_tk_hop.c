/* test_tk_hop.c - tests of core/tk_hop.c (64-spin Hopfield memory).
 *
 * Oracles, all independent of the code under test:
 *  - dbl_*      : a double-precision 64 x 64 weight-matrix Hopfield written
 *                 here (tie keeps the spin, like tk_hop);
 *  - upstream   : tsotchke's hopfield.c, unmodified, in tests/upstream/spin_nn
 *                 (MIT; tie resolves to +1, so it is compared bit for bit for
 *                 odd K and on the untied spins for even K);
 *  - ref_*      : a per-site integer form that never uses popcount.
 *
 * Seeds: thresholds were chosen on seeds 1..99, and every check runs on
 * seeds 1..99. The preregistered blocks 5001..5024 and 5101..5124 are
 * reserved for the milestone-8 reporting runs and are not used here.
 */
#include "az_test.h"
#include "../core/tk_hop.h"
#include <math.h>
#include <time.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-prototypes"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wunused-function"
#include "upstream/spin_nn/hopfield.c"
#pragma GCC diagnostic pop

#define N 64

/* ---- test RNG (splitmix64 sequence; test-only, not the simulation RNG) ---- */
static u64 rng_state;
static void rnd_seed(u64 seed) { rng_state = az_mix64(seed ^ 0x5851F42D4C957F2Dull); }
static u64 rnd64(void) { return az_test_rand64(&rng_state); }
static u32 rnd_below(u32 n) { return (u32)(((rnd64() >> 32) * (u64)n) >> 32); }

/* flip exactly nflip distinct bits */
static u64 flip_bits(u64 s, u32 nflip)
{
    u64 mask = 0;
    while ((u32)__builtin_popcountll(mask) < nflip) mask |= 1ull << rnd_below(64);
    return s ^ mask;
}

static void rnd_patterns(u64 *pat, u32 k)
{
    for (u32 m = 0; m < k; m++) pat[m] = rnd64();
}

static int spin_of(u64 s, int i) { return ((s >> i) & 1u) ? 1 : -1; }

/* ---- oracle 1: double-precision weight matrix ---- */
typedef struct { double J[N][N]; } DblNet;

static void dbl_build(DblNet *net, const u64 *pat, u32 k)
{
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++) {
            double a = 0.0;
            if (i != j)
                for (u32 m = 0; m < k; m++) a += (double)(spin_of(pat[m], i) * spin_of(pat[m], j));
            net->J[i][j] = a / (double)N;
        }
}

static double dbl_field(const DblNet *net, u64 s, int i)
{
    double f = 0.0;
    for (int j = 0; j < N; j++) f += net->J[i][j] * (double)spin_of(s, j);
    return f;
}

static u64 dbl_step(const DblNet *net, u64 s, u64 *tie_mask)
{
    u64 out = 0, ties = 0;
    for (int i = 0; i < N; i++) {
        double f = dbl_field(net, s, i);
        int v = f > 0.0 ? 1 : (f < 0.0 ? -1 : spin_of(s, i));
        if (f == 0.0) ties |= 1ull << i;
        if (v > 0) out |= 1ull << i;
    }
    if (tie_mask) *tie_mask = ties;
    return out;
}

static double dbl_energy(const DblNet *net, u64 s)
{
    double e = 0.0;
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++) e -= 0.5 * net->J[i][j] * (double)(spin_of(s, i) * spin_of(s, j));
    return e;
}

/* ---- oracle 2: upstream hopfield.c ---- */
static void to_spins(u64 s, int *spins) { for (int i = 0; i < N; i++) spins[i] = spin_of(s, i); }
static u64 from_spins(const int *spins)
{
    u64 s = 0;
    for (int i = 0; i < N; i++) if (spins[i] > 0) s |= 1ull << i;
    return s;
}
static void up_store(hopfield_t *net, const u64 *pat, u32 k)
{
    static int flat[TK_HOP_KMAX * N];
    if (k == 0) { memset(net->J, 0, sizeof(double) * N * N); return; }
    for (u32 m = 0; m < k; m++) to_spins(pat[m], flat + m * N);
    (void)hopfield_store_patterns(net, flat, (int)k);
}
static u64 up_step(hopfield_t *net, u64 s)
{
    int spins[N];
    to_spins(s, spins);
    (void)hopfield_sync_update(net, spins);
    return from_spins(spins);
}

/* ---- oracle 3: per-site integer form without popcount ---- */
static i32 ref_field(const u64 *pat, u32 k, u64 s, int i)
{
    i32 h = 0;
    for (u32 m = 0; m < k; m++) {
        i32 o = 0;
        for (int j = 0; j < N; j++) o += spin_of(pat[m], j) * spin_of(s, j);
        h += spin_of(pat[m], i) * o;
    }
    return h - (i32)k * spin_of(s, i);
}
static u64 ref_step(const u64 *pat, u32 k, u64 s)
{
    u64 out = 0;
    for (int i = 0; i < N; i++) {
        i32 h = ref_field(pat, k, s, i);
        if (h > 0 || (h == 0 && spin_of(s, i) > 0)) out |= 1ull << i;
    }
    return out;
}

/* per-site integer baseline for the timing table: popcount overlaps, then
 * one sign per spin (what tk_hop_step would be without bit-slicing) */
static u64 persite_step(const u64 *pat, u32 k, u64 s)
{
    i32 o[TK_HOP_KMAX];
    u64 out = 0;
    for (u32 m = 0; m < k; m++) o[m] = 64 - 2 * (i32)__builtin_popcountll(pat[m] ^ s);
    for (int i = 0; i < N; i++) {
        i32 si = (i32)((s >> i) & 1u) * 2 - 1;
        i32 h = -(i32)k * si;
        for (u32 m = 0; m < k; m++) {
            i32 neg = (i32)((pat[m] >> i) & 1u) - 1;
            h += (o[m] ^ neg) - neg;
        }
        out |= (u64)((u32)(h > 0) | ((u32)(h == 0) & (u32)(si > 0))) << i;
    }
    return out;
}

/* ======================================================================= */
/* A. synchronous step, field and energy against the three oracles          */
static void test_step_equivalence(hopfield_t *up)
{
    static DblNet net;
    u64 pat[TK_HOP_KMAX + 4];
    long n_states = 0, bad_dbl = 0, bad_ref = 0, bad_up = 0, bad_field = 0, bad_energy = 0;
    long bad_up_energy = 0, ties_odd = 0, ties_even = 0, bad_tie_keep = 0;
    rnd_seed(11);
    for (u32 k = 0; k <= TK_HOP_KMAX; k++) {
        for (int set = 0; set < 60; set++) {
            rnd_patterns(pat, k);
            dbl_build(&net, pat, k);
            up_store(up, pat, k);
            for (int v = 0; v < 12; v++) {
                u64 s;
                if (v < 4 || k == 0) s = rnd64();
                else if (v < 10) s = flip_bits(pat[rnd_below(k)], (u32)(v - 4) * 6u);
                else s = ~flip_bits(pat[rnd_below(k)], 5u);
                u64 ties;
                u64 want = dbl_step(&net, s, &ties);
                u64 got = tk_hop_step(pat, k, s);
                n_states++;
                if (got != want) bad_dbl++;
                if (got != ref_step(pat, k, s)) bad_ref++;
                if (got != persite_step(pat, k, s)) bad_ref++;
                /* upstream: equal wherever the field is not exactly zero */
                if ((got ^ up_step(up, s)) & ~ties) bad_up++;
                if ((got ^ s) & ties) bad_tie_keep++;
                if (k & 1u) ties_odd += __builtin_popcountll(ties);
                else ties_even += __builtin_popcountll(ties);
                for (int q = 0; q < 4; q++) {
                    int i = (int)rnd_below(64);
                    i32 h = tk_hop_field(pat, k, s, (u32)i);
                    if ((double)h != 64.0 * dbl_field(&net, s, i) || h != ref_field(pat, k, s, i)) bad_field++;
                }
                i32 e = tk_hop_energy(pat, k, s);
                if ((double)e != 128.0 * dbl_energy(&net, s)) bad_energy++;
                int spins[N];
                to_spins(s, spins);
                if ((double)e != 128.0 * hopfield_energy(up, spins)) bad_up_energy++;
            }
        }
    }
    printf("A. step/field/energy: %ld states, k = 0..%u; ties seen: odd k %ld, even k %ld\n",
           n_states, TK_HOP_KMAX, ties_odd, ties_even);
    CHECK_EQ(bad_dbl, 0);
    CHECK_EQ(bad_ref, 0);
    CHECK_EQ(bad_up, 0);
    CHECK_EQ(bad_field, 0);
    CHECK_EQ(bad_energy, 0);
    CHECK_EQ(bad_up_energy, 0);
    CHECK_EQ(bad_tie_keep, 0);
    CHECK_EQ(ties_odd, 0);      /* parity argument: no tie for odd K          */
    CHECK(ties_even > 100);     /* the tie rule was actually exercised        */

    /* k above the maximum is clamped, k = 0 is the identity */
    rnd_patterns(pat, TK_HOP_KMAX + 4);
    u64 s = rnd64();
    CHECK(tk_hop_step(pat, TK_HOP_KMAX + 4, s) == tk_hop_step(pat, TK_HOP_KMAX, s));
    CHECK(tk_hop_step(pat, TK_HOP_KMAX, s) != tk_hop_step(pat, TK_HOP_KMAX - 1, s));
    CHECK(tk_hop_energy(pat, 1000, s) == tk_hop_energy(pat, TK_HOP_KMAX, s));
    CHECK(tk_hop_step(pat, 0, s) == s);
    CHECK_EQ(tk_hop_energy(pat, 0, s), 0);
    /* a stored pattern alone: fixed point with the minimum energy 64 - 64^2 */
    CHECK(tk_hop_step(pat, 1, pat[0]) == pat[0]);
    CHECK(tk_hop_step(pat, 1, ~pat[0]) == ~pat[0]);
    CHECK_EQ(tk_hop_energy(pat, 1, pat[0]), 64 - 4096);
    CHECK_EQ(tk_hop_energy(pat, 1, ~pat[0]), 64 - 4096);
    /* the example of the audit note: three patterns, energy -31.28125 * 128 */
    u64 ex[3] = { 0x09f1fd9d03f0a9b4ull, 0x553274161bbf8475ull, 0x5d5bca4696b343b3ull };
    CHECK_EQ(tk_hop_energy(ex, 3, ex[1]), -4004);
    CHECK(tk_hop_recall(ex, 3, 0xd7317452599ba477ull, 20) == ex[1]);
}

/* ======================================================================= */
/* B. one-spin (asynchronous) updates never raise the energy                */
static void test_async_energy(void)
{
    u64 pat[TK_HOP_KMAX];
    long updates = 0, flips = 0, raised = 0, bad_delta = 0, bad_sweep = 0, not_fixed = 0, bad_local = 0;
    long sweep_raised = 0, settle_above_start = 0;
    u32 max_sweeps = 0;
    rnd_seed(12);
    for (u32 k = 1; k <= TK_HOP_KMAX; k++) {
        for (int set = 0; set < 40; set++) {
            rnd_patterns(pat, k);
            u64 s = (set & 1) ? rnd64() : flip_bits(pat[rnd_below(k)], 20);
            u64 start = s;
            i32 e = tk_hop_energy(pat, k, s);
            for (int u = 0; u < 300; u++) {
                u32 site = rnd_below(64);
                i32 h = tk_hop_field(pat, k, s, site);
                u64 nx = tk_hop_step_one(pat, k, s, site);
                i32 e2 = tk_hop_energy(pat, k, nx);
                updates++;
                if (e2 > e) raised++;
                if (nx != s) {
                    flips++;
                    if (e2 != e - 4 * (h < 0 ? -h : h) || e2 >= e) bad_delta++;
                    if (__builtin_popcountll(nx ^ s) != 1) bad_delta++;
                } else if (e2 != e) bad_delta++;
                s = nx;
                e = e2;
            }
            /* a sweep is 64 one-spin updates in site order */
            u64 a = start, b = start;
            for (u32 i = 0; i < 64; i++) a = tk_hop_step_one(pat, k, a, i);
            b = tk_hop_sweep(pat, k, b);
            if (a != b) bad_sweep++;
            if (tk_hop_energy(pat, k, b) > tk_hop_energy(pat, k, start)) sweep_raised++;
            /* settle: ends in a fixed point of both dynamics */
            u64 f = start;
            u32 sweeps = 0;
            while (sweeps < TK_HOP_ITERS) {
                u64 nx = tk_hop_sweep(pat, k, f);
                sweeps++;
                if (nx == f) break;
                f = nx;
            }
            if (sweeps > max_sweeps) max_sweeps = sweeps;
            if (tk_hop_settle(pat, k, start, TK_HOP_ITERS) != f) bad_sweep++;
            if (!tk_hop_is_fixed(pat, k, f)) not_fixed++;
            for (int i = 0; i < N; i++)
                if (tk_hop_field(pat, k, f, (u32)i) * spin_of(f, i) < 0) bad_local++;
            if (tk_hop_energy(pat, k, f) > tk_hop_energy(pat, k, start)) settle_above_start++;
        }
    }
    printf("B. async: %ld one-spin updates, %ld flips, energy raised %ld times; settle needs at most %u sweeps (incl. the confirming one)\n",
           updates, flips, raised, max_sweeps);
    CHECK(flips > 1000);
    CHECK_EQ(raised, 0);
    CHECK_EQ(bad_delta, 0);
    CHECK_EQ(bad_sweep, 0);
    CHECK_EQ(sweep_raised, 0);
    CHECK_EQ(not_fixed, 0);
    CHECK_EQ(bad_local, 0);
    CHECK_EQ(settle_above_start, 0);
    CHECK(max_sweeps < 32);
}

/* ======================================================================= */
/* C. synchronous updates: the 2-cycle risk                                 */
static void test_sync_cycles(void)
{
    /* analytic example: K = 1 and a state orthogonal to the pattern. Then
     * o = 0, H_i = -s_i, every spin flips: s <-> ~s for ever, at the
     * maximum energy. */
    u64 p1[1] = { 0x0123456789ABCDEFull };
    u64 a = p1[0] ^ 0x00000000FFFFFFFFull;
    CHECK(tk_hop_step(p1, 1, a) == ~a);
    CHECK(tk_hop_step(p1, 1, ~a) == a);
    CHECK(!tk_hop_is_fixed(p1, 1, a));
    CHECK_EQ(tk_hop_energy(p1, 1, a), 64);
    CHECK_EQ(tk_hop_energy(p1, 1, ~a), 64);
    /* recall reports the cycle member nearest the cue, for either parity */
    u64 r20 = tk_hop_recall(p1, 1, a, 20), r21 = tk_hop_recall(p1, 1, a, 21);
    CHECK(r20 == a);
    CHECK(r21 == a);
    CHECK(tk_hop_recall(p1, 1, ~a, 20) == ~a);
    CHECK(!tk_hop_is_fixed(p1, 1, r20));
    /* the asynchronous dynamics leaves the cycle and finds the memory */
    u64 f = tk_hop_settle(p1, 1, a, 16);
    CHECK(f == p1[0] || f == ~p1[0]);
    CHECK(tk_hop_is_fixed(p1, 1, f));

    /* how often it happens: random starts and 8-flip cues, 100 sync steps */
    u64 pat[TK_HOP_KMAX];
    long raised_total = 0, steps_total = 0, unresolved_odd = 0, unresolved_even = 0, bad_recall = 0;
    long cyc_rand_total = 0;
    rnd_seed(13);
    printf("C. synchronous dynamics, 4000 starts per row: share ending in a 2-cycle\n");
    printf("    k   random start   8-flip cue   longer/unresolved\n");
    for (u32 k = 1; k <= TK_HOP_KMAX; k++) {
        long cyc[2] = { 0, 0 }, unresolved = 0;
        for (int trial = 0; trial < 4000; trial++) {
            int kind = trial & 1;
            if ((trial % 20) < 2) rnd_patterns(pat, k);
            u64 cue = kind ? flip_bits(pat[rnd_below(k)], 8) : rnd64();
            u64 prev = cue, s = cue;
            int outcome = 2;                     /* 0 fixed, 1 two-cycle, 2 neither */
            for (int it = 0; it < 100; it++) {
                u64 nx = tk_hop_step(pat, k, s);
                steps_total++;
                if (tk_hop_energy(pat, k, nx) > tk_hop_energy(pat, k, s)) raised_total++;
                if (nx == s) { outcome = 0; break; }
                if (nx == prev) { outcome = 1; break; }
                prev = s;
                s = nx;
            }
            if (outcome == 1) cyc[kind]++;
            if (outcome == 2) unresolved++;
            /* tk_hop_recall agrees with this classification */
            u64 r = tk_hop_recall(pat, k, cue, 100);
            if (outcome == 0 && (r != s || !tk_hop_is_fixed(pat, k, r))) bad_recall++;
            if (outcome == 1) {
                u64 other = tk_hop_step(pat, k, r);
                if (tk_hop_is_fixed(pat, k, r) || tk_hop_step(pat, k, other) != r) bad_recall++;
                if (__builtin_popcountll(r ^ cue) > __builtin_popcountll(other ^ cue)) bad_recall++;
                if (tk_hop_recall(pat, k, cue, 101) != r) bad_recall++;
            }
        }
        if (k & 1u) unresolved_odd += unresolved; else unresolved_even += unresolved;
        cyc_rand_total += cyc[0];
        printf("   %2u      %6.4f        %6.4f        %ld\n", k, cyc[0] / 2000.0, cyc[1] / 2000.0, unresolved);
    }
    printf("   synchronous steps that raised the energy: %ld of %ld\n", raised_total, steps_total);
    CHECK_EQ(bad_recall, 0);
    /* symmetric weights and no ties (odd K): period is 1 or 2, a theorem */
    CHECK_EQ(unresolved_odd, 0);
    /* even K has ties; no longer cycle was seen, but that is not a theorem */
    CHECK_EQ(unresolved_even, 0);
    CHECK(cyc_rand_total > 0);    /* the risk is real, not only the K = 1 example */
    CHECK(raised_total > 0);      /* and a synchronous step can raise the energy  */
}

/* ======================================================================= */
/* D. recall curve, shuffled control, memory-off control                    */
typedef struct {
    long trials, exact_sync, exact_async, exact_shuf, exact_off, cycles;
    long ov_sync, ov_async, ov_shuf, ov_off;
} Curve;

static i32 overlap(u64 a, u64 b) { return 64 - 2 * (i32)__builtin_popcountll(a ^ b); }

static u64 permute_bits(u64 x, const u8 *perm)
{
    u64 y = 0;
    for (int i = 0; i < N; i++) y |= ((x >> i) & 1ull) << perm[i];
    return y;
}

/* 25 pattern sets x 20 cues with 8 flipped bits, 20 iterations */
static void recall_trials(u64 seed, u32 k, Curve *c)
{
    u64 pat[TK_HOP_KMAX], shuf[TK_HOP_KMAX];
    u8 perm[N];
    rnd_seed(seed * 1000003ull + k);
    for (int set = 0; set < 25; set++) {
        rnd_patterns(pat, k);
        for (int i = 0; i < N; i++) perm[i] = (u8)i;
        for (int i = N - 1; i > 0; i--) {
            u32 j = rnd_below((u32)i + 1u);
            u8 t = perm[i]; perm[i] = perm[j]; perm[j] = t;
        }
        /* the control keeps every statistic of the store (same overlaps
         * between patterns) and only destroys its relation to the cue */
        for (u32 m = 0; m < k; m++) shuf[m] = permute_bits(pat[m], perm);
        for (int q = 0; q < 20; q++) {
            u64 target = pat[rnd_below(k)];
            u64 cue = flip_bits(target, 8);
            u64 r = tk_hop_recall(pat, k, cue, 20);
            u64 ra = tk_hop_settle(pat, k, cue, 20);
            u64 rs = tk_hop_recall(shuf, k, cue, 20);
            u64 ro = tk_hop_recall(pat, 0, cue, 20);     /* memory off: k = 0 */
            i32 os = overlap(rs, target);
            c->trials++;
            c->exact_sync += r == target;
            c->exact_async += ra == target;
            c->exact_shuf += rs == target;
            c->exact_off += ro == target;
            c->cycles += !tk_hop_is_fixed(pat, k, r);
            c->ov_sync += overlap(r, target);
            c->ov_async += overlap(ra, target);
            c->ov_shuf += os < 0 ? -os : os;
            c->ov_off += overlap(ro, target);
        }
    }
}

static void test_recall_curve(void)
{
    printf("D. recall of one of k random patterns from a cue with 8 of 64 bits flipped\n");
    for (int pass = 0; pass < 2; pass++) {
        /* pass 0: tuning seeds 1..48 (printed only). pass 1: tuning seeds
         * 51..98, checked. The reporting blocks 5001..5024 and 5101..5124
         * stay reserved for the milestone-8 runs (no unit test uses them). */
        printf("   %s\n", pass ? "checked seeds 51..98 (24000 cues per row)"
                               : "tuning seeds 1..48 (24000 cues per row, no checks)");
        printf("    k  exact(sync)  exact(async)  2-cycle  mean ov   | shuffled: exact  mean|ov| | memory off: exact  mean ov | worst seed\n");
        for (u32 k = 1; k <= 12; k++) {
            Curve c;
            memset(&c, 0, sizeof c);
            double worst = 1.0;
            for (int i = 0; i < 48; i++) {
                u64 seed = pass ? (u64)(51 + i) : (u64)(1 + i);
                Curve one;
                memset(&one, 0, sizeof one);
                recall_trials(seed, k, &one);
                double rate = (double)one.exact_sync / (double)one.trials;
                if (rate < worst) worst = rate;
                c.trials += one.trials; c.exact_sync += one.exact_sync; c.exact_async += one.exact_async;
                c.exact_shuf += one.exact_shuf; c.exact_off += one.exact_off; c.cycles += one.cycles;
                c.ov_sync += one.ov_sync; c.ov_async += one.ov_async; c.ov_shuf += one.ov_shuf; c.ov_off += one.ov_off;
            }
            double n = (double)c.trials;
            printf("   %2u    %6.4f       %6.4f     %6.4f   %6.2f   |          %6.4f   %6.2f   |             %6.4f   %6.2f  |  %5.3f\n",
                   k, c.exact_sync / n, c.exact_async / n, c.cycles / n, c.ov_sync / n,
                   c.exact_shuf / n, c.ov_shuf / n, c.exact_off / n, c.ov_off / n, worst);
            if (!pass) continue;
            CHECK_EQ(c.trials, 24000);
            if (k <= 6) {
                CHECK(c.exact_sync * 100 >= c.trials * 95);      /* the required 95% */
                CHECK(c.exact_async * 100 >= c.trials * 95);
                CHECK(c.ov_sync >= 62 * c.trials);
            }
            /* shuffled store: recall collapses, for every load. Two unrelated
             * words have a mean |overlap| of 6.4; the rows sit at 9 to 15
             * because a store that has nothing to say about the cue leaves
             * part of it standing (for k = 1 a bit permutation keeps the
             * popcount, so the cue is orthogonal to the store twice as
             * often and comes back from the 2-cycle unchanged). The bound
             * of 20 was read off the tuning-seed table; an intact memory
             * scores 55 to 64, a missing one exactly 48. */
            CHECK(c.exact_shuf * 200 <= c.trials);               /* <= 0.5%          */
            CHECK(c.ov_shuf <= 20 * c.trials);
            CHECK(c.ov_shuf * 2 < c.ov_sync);
            /* memory off: the cue comes back unchanged, 8 bits wrong */
            CHECK_EQ(c.exact_off, 0);
            CHECK_EQ(c.ov_off, 48 * c.trials);
        }
    }
}

/* ======================================================================= */
/* E. recall against the upstream recall loop                               */
static void test_recall_upstream(hopfield_t *up)
{
    u64 pat[TK_HOP_KMAX];
    long n = 0, converged = 0, bad_state = 0, bad_flag = 0;
    rnd_seed(15);
    for (u32 k = 1; k <= 13; k += 2) {            /* odd K: same function as upstream */
        for (int set = 0; set < 40; set++) {
            rnd_patterns(pat, k);
            up_store(up, pat, k);
            for (int q = 0; q < 25; q++) {
                u64 cue = (q % 5 == 4) ? rnd64() : flip_bits(pat[rnd_below(k)], (u32)(q % 8) * 4u);
                int spins[N];
                to_spins(cue, spins);
                int conv = hopfield_recall(up, spins, 20);
                u64 mine = tk_hop_recall(pat, k, cue, 20);
                n++;
                if (conv) {
                    converged++;
                    if (mine != from_spins(spins)) bad_state++;
                    if (!tk_hop_is_fixed(pat, k, mine)) bad_flag++;
                } else if (tk_hop_is_fixed(pat, k, mine)) {
                    /* upstream needs one extra sweep to confirm a fixed point
                     * that was reached on its last sweep */
                    if (mine != from_spins(spins)) bad_flag++;
                }
            }
        }
    }
    printf("E. recall vs upstream hopfield_recall (odd k 1..13, 20 sweeps): %ld cues, %ld converged upstream, state mismatches %ld\n",
           n, converged, bad_state);
    CHECK(converged > n / 2);
    CHECK(converged < n);
    CHECK_EQ(bad_state, 0);
    CHECK_EQ(bad_flag, 0);
}

/* ======================================================================= */
/* F. heat bath                                                             */
static double sigma(double x) { return 1.0 / (1.0 + exp(-x)); }

/* The stream rule of tk_hop.h written bit by bit. Reports bits used and
 * whether the stream ran out. */
static u64 ref_heatbath_h(const i32 *hs, u64 s, u32 t, const u64 rnd[4], u32 *used, int *ran_out)
{
    u64 out = 0;
    u32 pos = 0;
    int dead = 0;
    for (int i = 0; i < N; i++) {
        i32 h = hs[i];
        u32 p = tk_hop_prob_up(h, t);
        int up = -1;
        if (p == 0) up = 0;
        else if (p >= 65536u) up = 1;
        else if (!dead) {
            u32 at = pos;
            for (int b = 15; b >= 0 && up < 0; b--) {
                if (at >= 256u) break;
                u32 ubit = (u32)((rnd[at >> 6] >> (63u - (at & 63u))) & 1u);
                u32 pbit = (p >> b) & 1u;
                at++;
                if (ubit != pbit) up = (int)pbit;      /* u < p iff u has the 0 */
                else if (b == 0) up = 0;               /* u == p: not below     */
            }
            if (up < 0) dead = 1; else pos = at;
        }
        if (up < 0) up = h > 0 ? 1 : (h < 0 ? 0 : spin_of(s, i) > 0);
        if (up) out |= 1ull << i;
    }
    if (used) *used = pos;
    if (ran_out) *ran_out = dead;
    return out;
}

static u64 ref_heatbath(const u64 *pat, u32 k, u64 s, u32 t, const u64 rnd[4], u32 *used, int *ran_out)
{
    i32 hs[N];
    for (int i = 0; i < N; i++) hs[i] = ref_field(pat, k, s, i);
    return ref_heatbath_h(hs, s, t, rnd, used, ran_out);
}

static void test_heatbath(void)
{
    /* F1. the table against libm, entry by entry (t = 32 makes q = h) */
    long bad_table = 0, bad_mirror = 0;
    for (i32 h = 0; h <= 400; h++) {
        double want = floor(65536.0 * sigma((double)h / 32.0) + 0.5);
        if (want > 65535.0) want = 65535.0;
        if (h >= 384) want = 65536.0;
        if ((double)tk_hop_prob_up(h, 32) != want) bad_table++;
        if (tk_hop_prob_up(-h, 32) != 65536u - tk_hop_prob_up(h, 32)) bad_mirror++;
    }
    CHECK_EQ(bad_table, 0);
    CHECK_EQ(bad_mirror, 0);
    CHECK_EQ(tk_hop_prob_up(0, 7), 32768);

    /* F2. every (|h|, t): exact rounding division, distance to the true
     * logistic, monotone in h */
    long bad_q = 0, bad_mono = 0;
    double worst = 0.0;
    for (u32 t = 1; t <= TK_HOP_TMAX; t++) {
        u32 last = 0;
        for (u32 a = 0; a <= 65u * TK_HOP_KMAX; a++) {
            u32 q = (64u * a + t) / (2u * t);                    /* round(32a/t) */
            u32 want = q < 384u ? tk_hop_prob_up((i32)q, 32) : 65536u;
            u32 got = tk_hop_prob_up((i32)a, t);
            if (got != want) bad_q++;
            if (got < last) bad_mono++;
            last = got;
            double err = fabs((double)got / 65536.0 - sigma((double)a / (double)t));
            if (err > worst) worst = err;
        }
    }
    printf("F. heat bath: max |table probability - logistic| over all (|h| <= %u, t <= %u) = %.5f\n",
           65u * TK_HOP_KMAX, TK_HOP_TMAX, worst);
    CHECK_EQ(bad_q, 0);
    CHECK_EQ(bad_mono, 0);
    CHECK(worst < 0.0040);
    CHECK(tk_hop_prob_up(1000000, 9) == 65536u && tk_hop_prob_up(-1000000, 9) == 0u);
    CHECK_EQ(tk_hop_prob_up(5, 0), 65536);
    CHECK_EQ(tk_hop_prob_up(-5, 0), 0);
    CHECK_EQ(tk_hop_prob_up(0, 0), 32768);
    CHECK(tk_hop_prob_up(900, TK_HOP_TMAX + 1000u) == tk_hop_prob_up(900, TK_HOP_TMAX));
    CHECK(tk_hop_prob_up(900, TK_HOP_TMAX) != tk_hop_prob_up(900, TK_HOP_TMAX - 1000u));

    /* F3. temperature 0 is the deterministic step, whatever the stream */
    u64 pat[TK_HOP_KMAX], rnd[4];
    long bad_zero = 0, bad_ref = 0, n_ref = 0, differ = 0, ran_out_seen = 0;
    u32 max_used = 0;
    unsigned long long used_sum = 0;
    rnd_seed(16);
    for (u32 k = 0; k <= TK_HOP_KMAX; k++) {
        for (int trial = 0; trial < 300; trial++) {
            rnd_patterns(pat, k);
            u64 s = (trial & 1) || k == 0 ? rnd64() : flip_bits(pat[rnd_below(k)], rnd_below(30));
            for (int i = 0; i < 4; i++) rnd[i] = rnd64();
            if (tk_hop_heatbath(pat, k, s, 0, rnd) != tk_hop_step(pat, k, s)) bad_zero++;
            /* F4. any temperature: the stream rule, bit for bit */
            u32 t = 1u + rnd_below(trial % 3 == 0 ? 4200u : 64u);
            u32 used; int ran_out;
            i32 hs[N];
            for (int i = 0; i < N; i++) hs[i] = ref_field(pat, k, s, i);
            u64 want = ref_heatbath_h(hs, s, t, rnd, &used, &ran_out);
            u64 got = tk_hop_heatbath(pat, k, s, t, rnd);
            n_ref++;
            if (got != want) bad_ref++;
            if (got != tk_hop_step(pat, k, s)) differ++;
            if (used > max_used) max_used = used;
            used_sum += used;
            ran_out_seen += ran_out;
            /* same stream, same answer; extreme streams */
            if (tk_hop_heatbath(pat, k, s, t, rnd) != got) bad_ref++;
            u64 z[4] = { 0, 0, 0, 0 }, o[4] = { ~0ull, ~0ull, ~0ull, ~0ull };
            if (tk_hop_heatbath(pat, k, s, t, z) != ref_heatbath_h(hs, s, t, z, NULL, NULL)) bad_ref++;
            if (tk_hop_heatbath(pat, k, s, t, o) != ref_heatbath_h(hs, s, t, o, NULL, NULL)) bad_ref++;
        }
    }
    printf("   stream rule: %ld random cases, mean %.1f bits used of 256 (max %u), stream ran out %ld times\n",
           n_ref, (double)used_sum / (double)n_ref, max_used, ran_out_seen);
    CHECK_EQ(bad_zero, 0);
    CHECK_EQ(bad_ref, 0);
    CHECK(differ > n_ref / 2);         /* noise does change the update        */
    CHECK(max_used <= 256u);
    CHECK((double)used_sum / (double)n_ref < 140.0);   /* about 2 bits a spin */

    /* F5. forced exhaustion: a stream that equals the first sixteen
     * probabilities uses 16 bits each; spin 16 onwards must fall back to the
     * deterministic rule. */
    {
        u64 p3[3] = { 0x09f1fd9d03f0a9b4ull, 0x553274161bbf8475ull, 0x5d5bca4696b343b3ull };
        u64 s = p3[1] ^ 0x0000F0F000000F0Full;
        u32 t = 40;
        u64 adv[4] = { 0, 0, 0, 0 };
        int nontrivial = 0;
        for (int i = 0; i < 16; i++) {
            u32 p = tk_hop_prob_up(ref_field(p3, 3, s, i), t);
            if (p > 0 && p < 65536u) nontrivial++;
            adv[i >> 2] |= (u64)(p & 0xFFFFu) << (48 - 16 * (i & 3));
        }
        CHECK_EQ(nontrivial, 16);
        u32 used; int ran_out;
        u64 want = ref_heatbath(p3, 3, s, t, adv, &used, &ran_out);
        u64 got = tk_hop_heatbath(p3, 3, s, t, adv);
        CHECK_EQ(used, 256);
        CHECK_EQ(ran_out, 1);
        CHECK(got == want);
        CHECK((got & 0xFFFFull) == 0);                               /* u == p: not below */
        CHECK((got >> 16) == (tk_hop_step(p3, 3, s) >> 16));         /* deterministic rest */
    }

    /* F6. statistics of the draws: every spin is +1 with its table
     * probability, neighbours are uncorrelated */
    {
        u64 p3[3];
        rnd_seed(17);
        rnd_patterns(p3, 3);
        u64 s = flip_bits(p3[0], 6);
        const u32 t = 24;
        const int draws = 40000;
        static long cnt[N], both[N];
        memset(cnt, 0, sizeof cnt);
        memset(both, 0, sizeof both);
        for (int d = 0; d < draws; d++) {
            for (int i = 0; i < 4; i++) rnd[i] = rnd64();
            u64 r = tk_hop_heatbath(p3, 3, s, t, rnd);
            for (int i = 0; i < N; i++) {
                cnt[i] += (long)((r >> i) & 1u);
                both[i] += (long)((r >> i) & (r >> ((i + 1) & 63)) & 1u);
            }
        }
        long bad_freq = 0, bad_pair = 0;
        double max_z = 0.0;
        int distinct_p = 0;
        u32 seen_p[N];
        for (int i = 0; i < N; i++) {
            u32 pi = tk_hop_prob_up(ref_field(p3, 3, s, i), t);
            double p = pi / 65536.0;
            double pj = tk_hop_prob_up(ref_field(p3, 3, s, (i + 1) & 63), t) / 65536.0;
            double z = fabs((double)cnt[i] - draws * p) / sqrt(draws * p * (1.0 - p) + 1e-9);
            if (z > max_z) max_z = z;
            if (z > 5.0) bad_freq++;
            double pp = p * pj;
            double zp = fabs((double)both[i] - draws * pp) / sqrt(draws * pp * (1.0 - pp) + 1e-9);
            if (zp > 5.0) bad_pair++;
            int k2 = 0;
            while (k2 < distinct_p && seen_p[k2] != pi) k2++;
            if (k2 == distinct_p) seen_p[distinct_p++] = pi;
        }
        printf("   statistics: 64 spins x %d draws at t = %u, %d distinct probabilities, largest deviation %.2f sigma\n",
               draws, t, distinct_p, max_z);
        CHECK_EQ(bad_freq, 0);
        CHECK_EQ(bad_pair, 0);
        CHECK(distinct_p >= 4);
    }

    /* F7. order parameter against temperature: 3 patterns, start in a
     * pattern, 40 heat-bath steps. t = 32 is T = 1, the mean-field
     * transition of the Hopfield model at low load. */
    {
        static const u32 temps[] = { 0, 2, 4, 8, 12, 16, 20, 24, 32, 48, 64, 4095 };
        double ov[AZ_LEN(temps)];
        printf("   order vs temperature (k = 3, 2000 runs x 40 steps, seed 51): t / mean |overlap| with the start pattern\n   ");
        for (u32 ti = 0; ti < AZ_LEN(temps); ti++) {
            long sum = 0;
            rnd_seed(51);
            for (int run = 0; run < 2000; run++) {
                u64 p3[3];
                rnd_patterns(p3, 3);
                u64 s = p3[0];
                for (int step = 0; step < 40; step++) {
                    for (int i = 0; i < 4; i++) rnd[i] = rnd64();
                    s = tk_hop_heatbath(p3, 3, s, temps[ti], rnd);
                }
                i32 o = overlap(s, p3[0]);
                sum += o < 0 ? -o : o;
            }
            ov[ti] = sum / 2000.0;
            printf(" %u/%.1f", temps[ti], ov[ti]);
        }
        printf("\n");
        CHECK(ov[0] == 64.0);          /* t = 0: a stored pattern never moves */
        CHECK(ov[2] > 62.0);           /* t = 4: ordered                      */
        CHECK(ov[5] > 40.0);           /* t = 16 (T = 0.5): still ordered     */
        CHECK(ov[10] < 16.0);          /* t = 64 (T = 2): disordered          */
        CHECK(ov[11] < 10.0);          /* t max: coin flips, |ov| about 6.4   */
        for (u32 ti = 1; ti < AZ_LEN(temps); ti++) CHECK(ov[ti] <= ov[ti - 1] + 1.0);
    }
}

/* ======================================================================= */
/* G. store                                                                 */
static void test_store(void)
{
    u64 pat[TK_HOP_KMAX + 2];
    u32 k = 0;
    rnd_seed(18);
    memset(pat, 0, sizeof pat);
    CHECK(tk_hop_store(pat, 0, &k, 123) == TK_HOP_NONE);
    CHECK_EQ(k, 0);
    u64 a = rnd64(), b = rnd64(), c = rnd64();
    CHECK_EQ(tk_hop_store(pat, 3, &k, a), 0);
    CHECK_EQ(tk_hop_store(pat, 3, &k, b), 1);
    CHECK_EQ(k, 2);
    /* the word or its complement is already there: nothing changes */
    CHECK_EQ(tk_hop_store(pat, 3, &k, a), 0);
    CHECK_EQ(tk_hop_store(pat, 3, &k, ~b), 1);
    CHECK_EQ(k, 2);
    CHECK(pat[0] == a && pat[1] == b);
    CHECK_EQ(tk_hop_store(pat, 3, &k, c), 2);
    CHECK_EQ(k, 3);
    /* storing is Hebbian learning: each word is now an attractor */
    CHECK(tk_hop_recall(pat, k, flip_bits(a, 8), 20) == a);
    CHECK(tk_hop_recall(pat, k, flip_bits(b, 8), 20) == b);
    CHECK(tk_hop_recall(pat, k, flip_bits(c, 8), 20) == c);
    /* full: the most similar stored word is replaced (here a variant of b,
     * then a variant of the complement of a) */
    u64 b2 = flip_bits(b, 10), a2 = ~flip_bits(a, 12);
    CHECK_EQ(tk_hop_store(pat, 3, &k, b2), 1);
    CHECK(pat[0] == a && pat[1] == b2 && pat[2] == c);
    CHECK_EQ(tk_hop_store(pat, 3, &k, a2), 0);
    CHECK(pat[0] == a2 && pat[1] == b2 && pat[2] == c);
    CHECK_EQ(k, 3);
    CHECK(tk_hop_recall(pat, k, flip_bits(b2, 8), 20) == b2);
    CHECK(tk_hop_recall(pat, k, flip_bits(b, 4), 20) == b2);   /* b is gone, its neighbour answers */
    /* a count above the capacity is clamped; capacity is clamped to KMAX */
    k = 99;
    CHECK(tk_hop_store(pat, 3, &k, a2) == 0);
    CHECK_EQ(k, 3);
    k = 0;
    u32 last = 0;
    for (int i = 0; i < 40; i++) last = tk_hop_store(pat, 1000, &k, rnd64());
    CHECK_EQ(k, TK_HOP_KMAX);
    CHECK(last < TK_HOP_KMAX);
    CHECK_EQ(pat[TK_HOP_KMAX], 0);          /* nothing written past KMAX slots */
    CHECK_EQ(pat[TK_HOP_KMAX + 1], 0);
    /* property: after any sequence of stores no two slots are equal or complementary */
    long clash = 0;
    for (u32 i = 0; i < k; i++)
        for (u32 j = i + 1; j < k; j++)
            if (pat[i] == pat[j] || pat[i] == ~pat[j]) clash++;
    CHECK_EQ(clash, 0);
}

/* ======================================================================= */
/* I. known-answer hash: every entry point on a fixed input sequence. Any
 * change of behaviour (compiler, flags, a later edit) shows up here, as it
 * would in the world state hash.                                           */
static u64 fnv(u64 h, u64 v)
{
    for (int i = 0; i < 8; i++) { h ^= (v >> (8 * i)) & 0xFFu; h *= 0x100000001B3ull; }
    return h;
}

static void test_known_answer(void)
{
    u64 pat[TK_HOP_KMAX], rnd[4];
    u64 h = 0xCBF29CE484222325ull;
    rnd_seed(20);
    for (int trial = 0; trial < 20000; trial++) {
        u32 k = (u32)trial % (TK_HOP_KMAX + 1u);
        u32 n = 0;
        for (u32 m = 0; m < k; m++) h = fnv(h, tk_hop_store(pat, k, &n, rnd64()));
        /* The two draws are separate statements: C leaves the order of a
         * call's argument evaluation unspecified (finding v29). The flip
         * count is drawn first, the order gcc 16 and clang's msvc target used,
         * so the pinned value below is unchanged. */
        u64 s;
        if (n && (trial & 1)) {
            const u32 flips = rnd_below(24);
            const u32 pick = rnd_below(n);
            s = flip_bits(pat[pick], flips);
        } else {
            s = rnd64();
        }
        for (int i = 0; i < 4; i++) rnd[i] = rnd64();
        u32 t = rnd_below(80);
        h = fnv(h, tk_hop_step(pat, n, s));
        h = fnv(h, tk_hop_recall(pat, n, s, 20));
        h = fnv(h, (u64)(i64)tk_hop_energy(pat, n, s));
        h = fnv(h, (u64)(i64)tk_hop_field(pat, n, s, (u32)trial));
        h = fnv(h, tk_hop_step_one(pat, n, s, (u32)trial));
        h = fnv(h, tk_hop_sweep(pat, n, s));
        h = fnv(h, tk_hop_settle(pat, n, s, 20));
        h = fnv(h, tk_hop_heatbath(pat, n, s, t, rnd));
        h = fnv(h, tk_hop_prob_up((i32)(s % 2081u) - 1040, t));
        h = fnv(h, tk_hop_is_fixed(pat, n, s));
    }
    printf("I. known-answer hash over 20000 mixed calls = %016llx\n", (unsigned long long)h);
    /* pinned 2026-11-17; identical for gcc 16.1 -O0 / -O2 / -O3 -march=native,
     * a -fsanitize=undefined trap build and clang 21 -O2 (msvc target). With the
     * draws split into statements (finding v29), clang 21 -O2 for
     * x86_64-w64-windows-gnu, which evaluates arguments left to right, prints
     * it too; before the split it printed 2336b0065d4a316f. */
    CHECK(h == 0xe819181860fbf61dull);
}

/* ======================================================================= */
/* H. timing (noisy, parallel load: other agents share the machine)         */
static double now_ns(void)
{
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}

static void test_timing(hopfield_t *up)
{
    enum { M = 4096 };
    static u64 noise[M];
    u64 pat[TK_HOP_KMAX], rnd[4];
    rnd_seed(19);
    rnd_patterns(pat, TK_HOP_KMAX);
    for (int i = 0; i < M; i++) noise[i] = rnd64();
    for (int i = 0; i < 4; i++) rnd[i] = rnd64();
    u64 sink = 0;
    printf("H. timing, ns per call, best of 5 (noisy, parallel load)\n");
    printf("    k   step(bit-sliced)  per-site int  sweep(async)  heatbath t=16  recall 8-flip cue\n");
    static const u32 ks[] = { 1, 3, 6, 12, 16 };
    for (u32 ki = 0; ki < AZ_LEN(ks); ki++) {
        u32 k = ks[ki];
        double best[5] = { 1e30, 1e30, 1e30, 1e30, 1e30 };
        for (int rep = 0; rep < 5; rep++) {
            const int it = 400000, it_slow = 20000;
            u64 s = noise[0];
            double t0 = now_ns();
            for (int i = 0; i < it; i++) s = tk_hop_step(pat, k, s ^ noise[i & (M - 1)]);
            double t1 = now_ns();
            sink ^= s;
            for (int i = 0; i < it_slow; i++) s = persite_step(pat, k, s ^ noise[i & (M - 1)]);
            double t2 = now_ns();
            sink ^= s;
            for (int i = 0; i < it_slow; i++) s = tk_hop_sweep(pat, k, s ^ noise[i & (M - 1)]);
            double t3 = now_ns();
            sink ^= s;
            for (int i = 0; i < it_slow; i++) { rnd[i & 3] += s; s = tk_hop_heatbath(pat, k, s ^ noise[i & (M - 1)], 16, rnd); }
            double t4 = now_ns();
            sink ^= s;
            for (int i = 0; i < it_slow; i++)
                s = tk_hop_recall(pat, k, pat[i % k] ^ (0xFFull << ((s ^ noise[i & (M - 1)]) & 56u)), 20);
            double t5 = now_ns();
            sink ^= s;
            double v[5] = { (t1 - t0) / it, (t2 - t1) / it_slow, (t3 - t2) / it_slow, (t4 - t3) / it_slow, (t5 - t4) / it_slow };
            for (int j = 0; j < 5; j++) if (v[j] < best[j]) best[j] = v[j];
        }
        printf("   %2u       %7.1f          %7.1f       %7.1f       %7.1f          %7.1f\n",
               k, best[0], best[1], best[2], best[3], best[4]);
    }
    /* upstream double kernel for scale (k = 3) */
    up_store(up, pat, 3);
    int spins[N];
    to_spins(noise[1], spins);
    double bu = 1e30;
    for (int rep = 0; rep < 5; rep++) {
        double t0 = now_ns();
        for (int i = 0; i < 20000; i++) { spins[i & 63] = -spins[i & 63]; sink += (u64)hopfield_sync_update(up, spins); }
        double t1 = now_ns();
        if ((t1 - t0) / 20000 < bu) bu = (t1 - t0) / 20000;
    }
    printf("   upstream hopfield_sync_update (double J, malloc per call), k = 3: %.1f ns per call\n", bu);
    /* printing the checksum keeps the timed results alive; timing itself is
     * reported, not checked (the machine is shared) */
    printf("   (checksum %016llx)\n", (unsigned long long)sink);
}

int main(void)
{
    hopfield_t *up = hopfield_create(N);
    if (!up) { printf("upstream hopfield_create failed\n"); return 2; }
    test_step_equivalence(up);
    test_async_energy();
    test_sync_cycles();
    test_recall_curve();
    test_recall_upstream(up);
    test_heatbath();
    test_store();
    test_known_answer();
    test_timing(up);
    hopfield_free(up);
    return az_test_done();
}
