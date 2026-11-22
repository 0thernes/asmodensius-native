/* test_tools.c - unit test of tools/az_tools.h and az_event_less
 * (findings v34, v56, v53, s4).
 *
 * The option parsers print one line on stderr for every refused command
 * line; those lines are expected here. */
#include "az_test.h"
#include "../tools/az_tools.h"

/* ---- argv built from string literals (the parsers take char **) ---- */
#define AV_MAX 16
typedef struct Av {
    int argc;
    char *argv[AV_MAX + 1];
    char buf[AV_MAX][64];
} Av;

static Av *av_make(Av *a, int n, const char *const *src)
{
    memset(a, 0, sizeof *a);
    for (int i = 0; i < n && i < AV_MAX; i++) {
        size_t len = strlen(src[i]);
        if (len >= sizeof a->buf[i]) len = sizeof a->buf[i] - 1;
        memcpy(a->buf[i], src[i], len);
        a->buf[i][len] = 0;
        a->argv[i] = a->buf[i];
    }
    a->argc = n < AV_MAX ? n : AV_MAX;
    a->argv[a->argc] = NULL;
    return a;
}
#define AV(a, ...)                                                              \
    av_make(&(a), (int)(sizeof((const char *[]){__VA_ARGS__}) / sizeof(const char *)), \
            (const char *const[]){__VA_ARGS__})

/* ---- 1. azt_parse_u64 ---- */
static void test_parse_u64(void)
{
    u64 v = 0;
    v = 7; CHECK(azt_parse_u64("0", &v)); CHECK_EQ(v, 0);
    CHECK(azt_parse_u64("42", &v)); CHECK_EQ(v, 42);
    CHECK(azt_parse_u64("0x10", &v)); CHECK_EQ(v, 16);
    CHECK(azt_parse_u64("0XfF", &v)); CHECK_EQ(v, 255);
    v = 0;
    CHECK(azt_parse_u64("18446744073709551615", &v));
    CHECK(v == 18446744073709551615ull);
    CHECK(azt_parse_u64("0xFFFFFFFFFFFFFFFF", &v));
    CHECK(v == 18446744073709551615ull);

    static const char *const bad[] = {
        "", "-1", "+1", " 1", "1 ", "12abc", "0x", "0xg", "18446744073709551616",
        "1e3", "\xd9\xa1" /* U+0661 ARABIC-INDIC DIGIT ONE in UTF-8 */,
        "0x10000000000000000", "1\xd9\xa1", "0x 1", "--1",
        /* a decimal with a leading zero: the old parsers read 010 as octal 8 */
        "010", "00", "0007", "01"
    };
    for (u32 i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        v = 12345;
        bool ok = azt_parse_u64(bad[i], &v);
        CHECK(!ok);
        CHECK_EQ(v, 12345); /* a refused value leaves *out alone */
        if (ok) printf("  accepted '%s'\n", bad[i]);
    }
    CHECK(!azt_parse_u64(NULL, &v));
    CHECK(azt_parse_u64("0x0010", &v)); CHECK_EQ(v, 16);   /* leading zeros after 0x are unambiguous */
}

/* ---- 1b. azt_parse_dec / azt_f64 (plain decimals, for seconds) ---- */
static void test_parse_dec(void)
{
    static const struct { const char *s; double want; } good[] = {
        { "0", 0.0 }, { "30", 30.0 }, { "0.5", 0.5 }, { "86400", 86400.0 }, { "1.25", 1.25 }, { "10.0", 10.0 },
    };
    for (u32 i = 0; i < sizeof good / sizeof good[0]; i++) {
        double d = -1.0;
        CHECK(azt_parse_dec(good[i].s, &d));
        CHECK(d == good[i].want);
    }
    static const char *const bad[] = { "", ".5", "5.", "-1", "+1", "1e3", "0x10", " 1", "1 ", "00.5", "05", "1.2.3", "1,5" };
    for (u32 i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        double d = 7.0;
        bool ok = azt_parse_dec(bad[i], &d);
        CHECK(!ok);
        CHECK(d == 7.0);
        if (ok) printf("  accepted decimal '%s'\n", bad[i]);
    }
    char a0[] = "t", a1[] = "--max-s", a2[] = "0.5", a3[] = "86400.5";
    char *argv1[] = { a0, a1, a2 };
    double v = 30.0;
    CHECK(azt_f64(3, argv1, 1, "--max-s", 0.0, 86400.0, &v));
    CHECK(v == 0.5);
    char *argv2[] = { a0, a1, a3 };
    v = 30.0;
    CHECK(!azt_f64(3, argv2, 1, "--max-s", 0.0, 86400.0, &v));   /* above the range */
    CHECK(v == 30.0);
    v = 30.0;
    CHECK(azt_f64(1, argv1, 1, "--max-s", 0.0, 86400.0, &v));     /* absent: the default stays */
    CHECK(v == 30.0);
}

/* ---- 2. azt_u32 / azt_u64 / azt_str / azt_flag / azt_known ---- */
static void test_options(void)
{
    Av a;
    u32 x;
    u64 y;

    /* azt_u32: range [10, 20] */
    AV(a, "prog", "--n", "10");  x = 0; CHECK(azt_u32(a.argc, a.argv, 1, "--n", 10, 20, &x)); CHECK_EQ(x, 10);
    AV(a, "prog", "--n", "20");  x = 0; CHECK(azt_u32(a.argc, a.argv, 1, "--n", 10, 20, &x)); CHECK_EQ(x, 20);
    AV(a, "prog", "--n", "0x0f"); x = 0; CHECK(azt_u32(a.argc, a.argv, 1, "--n", 10, 20, &x)); CHECK_EQ(x, 15);
    AV(a, "prog", "--n", "9");   x = 3; CHECK(!azt_u32(a.argc, a.argv, 1, "--n", 10, 20, &x)); CHECK_EQ(x, 3);
    AV(a, "prog", "--n", "21");  x = 3; CHECK(!azt_u32(a.argc, a.argv, 1, "--n", 10, 20, &x)); CHECK_EQ(x, 3);
    AV(a, "prog", "--n", "12abc"); x = 3; CHECK(!azt_u32(a.argc, a.argv, 1, "--n", 10, 20, &x)); CHECK_EQ(x, 3);
    AV(a, "prog", "--t", "12");  x = 17; CHECK(azt_u32(a.argc, a.argv, 1, "--n", 10, 20, &x)); CHECK_EQ(x, 17);
    AV(a, "prog");               x = 17; CHECK(azt_u32(a.argc, a.argv, 1, "--n", 10, 20, &x)); CHECK_EQ(x, 17);
    /* full u32 range: hi + 1 does not wrap */
    AV(a, "prog", "--n", "4294967295"); x = 0;
    CHECK(azt_u32(a.argc, a.argv, 1, "--n", 0, 0xFFFFFFFFu, &x)); CHECK(x == 0xFFFFFFFFu);
    AV(a, "prog", "--n", "4294967296"); x = 5;
    CHECK(!azt_u32(a.argc, a.argv, 1, "--n", 0, 0xFFFFFFFFu, &x)); CHECK_EQ(x, 5);
    /* lo = 1: 0 is lo - 1 */
    AV(a, "prog", "--n", "0"); x = 5;
    CHECK(!azt_u32(a.argc, a.argv, 1, "--n", 1, 100, &x)); CHECK_EQ(x, 5);
    /* `first` skips a positional argument that looks like the option */
    AV(a, "prog", "--n", "--n", "11"); x = 0;
    CHECK(azt_u32(a.argc, a.argv, 2, "--n", 10, 20, &x)); CHECK_EQ(x, 11);

    /* azt_u64: range [1000, 2^40] */
    const u64 hi = 1ull << 40;
    AV(a, "prog", "--seed", "1000"); y = 0; CHECK(azt_u64(a.argc, a.argv, 1, "--seed", 1000, hi, &y)); CHECK_EQ(y, 1000);
    AV(a, "prog", "--seed", "1099511627776"); y = 0;
    CHECK(azt_u64(a.argc, a.argv, 1, "--seed", 1000, hi, &y)); CHECK(y == hi);
    AV(a, "prog", "--seed", "999"); y = 7; CHECK(!azt_u64(a.argc, a.argv, 1, "--seed", 1000, hi, &y)); CHECK_EQ(y, 7);
    AV(a, "prog", "--seed", "1099511627777"); y = 7;
    CHECK(!azt_u64(a.argc, a.argv, 1, "--seed", 1000, hi, &y)); CHECK_EQ(y, 7);
    AV(a, "prog", "--x", "5"); y = 4242; CHECK(azt_u64(a.argc, a.argv, 1, "--seed", 1000, hi, &y)); CHECK_EQ(y, 4242);
    AV(a, "prog", "--seed", "18446744073709551615"); y = 0;
    CHECK(azt_u64(a.argc, a.argv, 1, "--seed", 0, ~0ull, &y)); CHECK(y == ~0ull);
    AV(a, "prog", "--seed", "18446744073709551616"); y = 9;
    CHECK(!azt_u64(a.argc, a.argv, 1, "--seed", 0, ~0ull, &y)); CHECK_EQ(y, 9);

    /* azt_str */
    const char *s;
    AV(a, "prog", "--out", "file.bin"); s = NULL;
    CHECK(azt_str(a.argc, a.argv, 1, "--out", &s)); CHECK(s && !strcmp(s, "file.bin"));
    AV(a, "prog", "--other", "x"); s = "dflt";
    CHECK(azt_str(a.argc, a.argv, 1, "--out", &s)); CHECK(s && !strcmp(s, "dflt"));
    AV(a, "prog", "--out"); s = "dflt";
    CHECK(!azt_str(a.argc, a.argv, 1, "--out", &s)); CHECK(!strcmp(s, "dflt"));
    AV(a, "prog", "--out", "--ticks", "5"); s = "dflt";
    CHECK(!azt_str(a.argc, a.argv, 1, "--out", &s)); CHECK(!strcmp(s, "dflt"));
    AV(a, "prog", "--out", "a", "--out", "b"); s = "dflt";
    CHECK(!azt_str(a.argc, a.argv, 1, "--out", &s)); CHECK(!strcmp(s, "dflt"));
    AV(a, "prog", "--out", "-"); s = NULL; /* a single dash is a value (stdout) */
    CHECK(azt_str(a.argc, a.argv, 1, "--out", &s)); CHECK(s && !strcmp(s, "-"));
    /* the numeric readers inherit the refusals of azt_str */
    AV(a, "prog", "--n"); x = 3; CHECK(!azt_u32(a.argc, a.argv, 1, "--n", 0, 100, &x)); CHECK_EQ(x, 3);
    AV(a, "prog", "--n", "5", "--n", "5"); x = 3; CHECK(!azt_u32(a.argc, a.argv, 1, "--n", 0, 100, &x)); CHECK_EQ(x, 3);
    AV(a, "prog", "--n", "--quiet"); x = 3; CHECK(!azt_u32(a.argc, a.argv, 1, "--n", 0, 100, &x)); CHECK_EQ(x, 3);

    /* azt_flag */
    bool on;
    AV(a, "prog", "--quiet"); on = false; CHECK(azt_flag(a.argc, a.argv, 1, "--quiet", &on)); CHECK(on);
    AV(a, "prog", "--loud"); on = false; CHECK(azt_flag(a.argc, a.argv, 1, "--quiet", &on)); CHECK(!on);
    AV(a, "prog", "--quiet", "--quiet"); on = false; CHECK(!azt_flag(a.argc, a.argv, 1, "--quiet", &on));

    /* azt_known */
    static const char *const wv[] = { "--n", "--ticks", "--out", NULL };
    static const char *const fl[] = { "--quiet", "--no-lock", NULL };
    AV(a, "prog", "--n", "5", "--quiet", "--ticks", "9", "--out", "f", "--no-lock");
    CHECK(azt_known(a.argc, a.argv, 1, wv, fl));
    AV(a, "prog");
    CHECK(azt_known(a.argc, a.argv, 1, wv, fl));
    AV(a, "prog", "--n", "5", "--tick", "9");        /* misspelled option */
    CHECK(!azt_known(a.argc, a.argv, 1, wv, fl));
    AV(a, "prog", "--n", "5", "7");                  /* stray value */
    CHECK(!azt_known(a.argc, a.argv, 1, wv, fl));
    AV(a, "prog", "--quiet", "yes");                 /* a flag takes no value */
    CHECK(!azt_known(a.argc, a.argv, 1, wv, fl));
    AV(a, "prog", "--ticks");                        /* value missing at the end */
    CHECK(!azt_known(a.argc, a.argv, 1, wv, fl));
    AV(a, "prog", "--n", "5", "--ticks");
    CHECK(!azt_known(a.argc, a.argv, 1, wv, fl));
    AV(a, "prog", "pos", "--n", "5");                /* positional skipped by first = 2 */
    CHECK(azt_known(a.argc, a.argv, 2, wv, fl));
    CHECK(!azt_known(a.argc, a.argv, 1, wv, fl));
    AV(a, "prog", "--quiet");
    CHECK(azt_known(a.argc, a.argv, 1, NULL, fl));
    CHECK(!azt_known(a.argc, a.argv, 1, wv, NULL));
}

/* ---- 3. azt_grid ---- */
static void test_grid(void)
{
    Av a;
    u32 g[3];
    bool given;

    AV(a, "prog", "--grid", "6,6,5"); g[0] = g[1] = g[2] = 0; given = false;
    CHECK(azt_grid(a.argc, a.argv, 1, "--grid", 10, g, &given));
    CHECK(given); CHECK_EQ(g[0], 6); CHECK_EQ(g[1], 6); CHECK_EQ(g[2], 5);
    AV(a, "prog", "--grid", "10,1,0xa"); g[0] = g[1] = g[2] = 0;
    CHECK(azt_grid(a.argc, a.argv, 1, "--grid", 10, g, &given));
    CHECK(given); CHECK_EQ(g[0], 10); CHECK_EQ(g[1], 1); CHECK_EQ(g[2], 10);
    AV(a, "prog", "--n", "5"); g[0] = 4; g[1] = 4; g[2] = 3; given = true;
    CHECK(azt_grid(a.argc, a.argv, 1, "--grid", 10, g, &given));
    CHECK(!given); CHECK_EQ(g[0], 4); CHECK_EQ(g[1], 4); CHECK_EQ(g[2], 3);

    static const char *const bad[] = {
        "6,6", "6,6,5,", "6,,5", "0,6,5", "6,6,11", " 6,6,5", "6,6,5x",
        "", ",6,6,5", "6,6,5,5", "6;6;5", "-6,6,5", "6,6,99999999999999999999", "6, 6,5"
    };
    for (u32 i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        AV(a, "prog", "--grid", bad[i]);
        g[0] = 4; g[1] = 4; g[2] = 3; given = true;
        bool ok = azt_grid(a.argc, a.argv, 1, "--grid", 10, g, &given);
        CHECK(!ok);
        CHECK(!given);
        CHECK_EQ(g[0], 4); CHECK_EQ(g[1], 4); CHECK_EQ(g[2], 3);
        if (ok) printf("  grid accepted '%s'\n", bad[i]);
    }
    AV(a, "prog", "--grid");
    CHECK(!azt_grid(a.argc, a.argv, 1, "--grid", 10, g, &given));
    AV(a, "prog", "--grid", "6,6,5", "--grid", "6,6,5");
    CHECK(!azt_grid(a.argc, a.argv, 1, "--grid", 10, g, &given));
}

/* ---- 4. azt_grid_for ---- */
static u32 grid_fail_reported = 0;
static void grid_for_one(u32 n)
{
    u32 g[3] = { 99, 99, 99 };
    azt_grid_for(n, g);
    u32 b = g[0] + g[1] + g[2];
    /* the largest power of two with 2^b <= n, but at least 2^5 */
    u32 want = 5;
    while (want < 31 && (2ull << want) <= n) want++;
    bool ok_b = b == want;
    bool ok_order = g[2] <= g[1] && g[1] <= g[0] && g[0] <= g[2] + 1;
    CHECK(ok_b);
    CHECK(ok_order);
    if (want > 5) CHECK((1ull << b) <= n && n < (2ull << b));
    if ((!ok_b || !ok_order) && grid_fail_reported++ < 8)
        printf("  azt_grid_for(%u) = %u,%u,%u (want %u bits, z <= y <= x <= z + 1)\n",
               n, g[0], g[1], g[2], want);
}

static void test_grid_for(void)
{
    for (u32 n = 1; n <= 64; n++) grid_for_one(n);
    for (u32 p = 6; p <= 24; p++) {
        u32 c = 1u << p;
        grid_for_one(c - 1);
        grid_for_one(c);
        grid_for_one(c + 1);
        grid_for_one(c + c / 2);
        grid_for_one(2 * c - 1);
    }
    u32 g[3] = { 0, 0, 0 };
    azt_grid_for(100000, g);
    CHECK_EQ(g[0], 6); CHECK_EQ(g[1], 5); CHECK_EQ(g[2], 5);
    azt_grid_for(1, g);
    CHECK_EQ(g[0] + g[1] + g[2], 5);
}

/* ---- 5. stage and event names ---- */
static void test_names(void)
{
    for (u32 i = 0; i < AZ_PROF_COUNT; i++) {
        const char *ni = azt_prof_name(i);
        CHECK(ni && ni[0] && strcmp(ni, "?"));
        for (u32 j = 0; j < i; j++) CHECK(strcmp(ni, azt_prof_name(j)));
    }
    CHECK(!strcmp(azt_prof_name(AZ_PROF_COUNT), "?"));
    CHECK(!strcmp(azt_prof_name(AZ_PROF_COUNT + 1), "?"));
    CHECK(!strcmp(azt_prof_name(0xFFFFFFFFu), "?"));
    CHECK(!strcmp(azt_prof_name(AZ_PROF_REORDER), "reorder"));
    CHECK(!strcmp(azt_prof_name(AZ_PROF_LEDGER), "ledger"));

    CHECK(azt_ev_name(0) == NULL);
    CHECK(azt_ev_name(AZ_EV_COUNT) == NULL);
    CHECK(azt_ev_name(0xFFFFu) == NULL);
    u32 named = 0;
    for (u32 k = 1; k < AZ_EV_COUNT; k++) {
        const char *nk = azt_ev_name(k);
        CHECK(nk && nk[0]);
        if (!nk) continue;
        named++;
        for (u32 j = 1; j < k; j++) CHECK(azt_ev_name(j) && strcmp(nk, azt_ev_name(j)));
        CHECK_EQ(azt_parse_kind(nk), k);           /* name round trip */
        char num[16];
        snprintf(num, sizeof num, "%u", k);
        CHECK_EQ(azt_parse_kind(num), k);          /* number round trip */
        snprintf(num, sizeof num, "0x%x", k);
        CHECK_EQ(azt_parse_kind(num), k);
    }
    CHECK_EQ(named, AZ_EV_COUNT - 1);
    CHECK(!strcmp(azt_ev_name(AZ_EV_BIRTH), "birth"));
    CHECK(!strcmp(azt_ev_name(AZ_EV_TICK), "tick"));
    CHECK(!strcmp(azt_ev_name(AZ_EV_REFUSED), "refused"));
    CHECK_EQ(azt_parse_kind("birth"), AZ_EV_BIRTH);
    CHECK_EQ(azt_parse_kind("tick"), AZ_EV_TICK);
    CHECK_EQ(azt_parse_kind("refused"), AZ_EV_REFUSED);
    CHECK_EQ(azt_parse_kind("x"), -1);
    CHECK_EQ(azt_parse_kind("0"), -1);
    CHECK_EQ(azt_parse_kind("(0)"), -1);
    CHECK_EQ(azt_parse_kind(""), -1);
    CHECK_EQ(azt_parse_kind("Birth"), -1);
    CHECK_EQ(azt_parse_kind("65535"), 65535);
    CHECK_EQ(azt_parse_kind("65536"), -1);
    CHECK_EQ(azt_parse_kind("-1"), -1);
}

/* ---- 6. az_event_less against a reference written from its documentation:
 * (kind, a, b, aux, v as a signed number), then tick. ---- */
static u64 sm_state;
static u64 splitmix64(void)
{
    u64 z = (sm_state += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

static int ref_cmp_u(u64 x, u64 y) { return x < y ? -1 : x > y ? 1 : 0; }
static int ref_cmp(const AzEvent *x, const AzEvent *y)
{
    int c;
    if ((c = ref_cmp_u(x->kind, y->kind))) return c;
    if ((c = ref_cmp_u(x->a, y->a))) return c;
    if ((c = ref_cmp_u(x->b, y->b))) return c;
    if ((c = ref_cmp_u(x->aux, y->aux))) return c;
    if (x->v != y->v) return (long long)x->v < (long long)y->v ? -1 : 1;
    return ref_cmp_u(x->tick, y->tick);
}

/* a field value: often from a tiny set (so ties reach the later keys),
 * sometimes an extreme, otherwise anything */
static u32 rnd_field(u32 bits)
{
    u64 r = splitmix64();
    u32 mask = bits >= 32 ? 0xFFFFFFFFu : ((1u << bits) - 1u);
    switch (r & 3) {
    case 0: case 1: return (u32)((r >> 8) & 1);
    case 2: return (r >> 8) & 1 ? mask : 0;
    default: return (u32)(r >> 32) & mask;
    }
}

static void rnd_event(AzEvent *e)
{
    memset(e, 0, sizeof *e);
    e->kind = (u16)rnd_field(16);
    e->a = rnd_field(32);
    e->b = rnd_field(32);
    e->aux = (u16)rnd_field(16);
    u64 r = splitmix64();
    switch (r & 3) {
    case 0: e->v = (r >> 8) & 1 ? -1 : 1; break;
    case 1: e->v = (r >> 8) & 1 ? INT32_MIN : INT32_MAX; break;
    case 2: e->v = 0; break;
    default: e->v = (i32)(u32)(r >> 32); break;
    }
    e->tick = rnd_field(32);
}

static u32 ev_fail_reported = 0;
static void check_pair(const AzEvent *x, const AzEvent *y)
{
    int c = ref_cmp(x, y);
    bool lxy = az_event_less(x, y), lyx = az_event_less(y, x);
    bool ok = lxy == (c < 0) && lyx == (c > 0);
    CHECK(ok);
    CHECK(!(lxy && lyx));                 /* antisymmetry */
    if (!ok && ev_fail_reported++ < 5)
        printf("  pair (%u,%llu,%llu,%u,%lld,t%llu) vs (%u,%llu,%llu,%u,%lld,t%llu): less %d/%d, ref %d\n",
               x->kind, (unsigned long long)x->a, (unsigned long long)x->b, x->aux, (long long)x->v,
               (unsigned long long)x->tick, y->kind, (unsigned long long)y->a, (unsigned long long)y->b, y->aux,
               (long long)y->v, (unsigned long long)y->tick, lxy, lyx, c);
}

static void test_event_less(void)
{
    sm_state = 17; /* tuning seed */
    AzEvent x, y;
    u32 pairs = 0, ties = 0, equal = 0;
    for (u32 i = 0; i < 200000; i++) {
        rnd_event(&x);
        if (splitmix64() & 1) {
            /* y is x with the keys from position k on redrawn: exercises each key */
            y = x;
            AzEvent r;
            rnd_event(&r);
            switch (splitmix64() % 7) {
            case 0: y.kind = r.kind; /* fall through */
            case 1: y.a = r.a;       /* fall through */
            case 2: y.b = r.b;       /* fall through */
            case 3: y.aux = r.aux;   /* fall through */
            case 4: y.v = r.v;       /* fall through */
            case 5: y.tick = r.tick; break;
            default: break;          /* y == x */
            }
            ties++;
        } else rnd_event(&y);
        check_pair(&x, &y);
        CHECK(!az_event_less(&x, &x));    /* irreflexivity */
        if (!ref_cmp(&x, &y)) { equal++; CHECK(!az_event_less(&x, &y)); }
        pairs++;
    }
    CHECK(pairs >= 100000);
    CHECK(ties > 50000);
    CHECK(equal > 1000);

    /* v is compared as a signed number */
    AzEvent lo, hi;
    memset(&lo, 0, sizeof lo);
    lo.kind = AZ_EV_ATTACK; lo.a = 5; lo.b = 6; lo.aux = 7;
    hi = lo;
    lo.v = INT32_MIN; hi.v = INT32_MAX;
    lo.tick = 9; hi.tick = 1;
    CHECK(az_event_less(&lo, &hi));
    CHECK(!az_event_less(&hi, &lo));
    lo.v = -1; hi.v = 0;
    CHECK(az_event_less(&lo, &hi));
    CHECK(!az_event_less(&hi, &lo));
    lo.v = INT32_MIN; hi.v = -1;
    CHECK(az_event_less(&lo, &hi));
    /* tick decides only when everything else ties */
    hi = lo;
    lo.tick = 1; hi.tick = 2;
    CHECK(az_event_less(&lo, &hi));
    CHECK(!az_event_less(&hi, &lo));
    /* kind outranks a, a outranks b, b outranks aux, aux outranks v */
    lo = hi; lo.kind = 1; hi.kind = 2; lo.a = 9; hi.a = 0; CHECK(az_event_less(&lo, &hi));
    lo = hi; lo.a = 1; hi.a = 2; lo.b = 9; hi.b = 0; CHECK(az_event_less(&lo, &hi));
    lo = hi; lo.b = 1; hi.b = 2; lo.aux = 9; hi.aux = 0; CHECK(az_event_less(&lo, &hi));
    lo = hi; lo.aux = 1; hi.aux = 2; lo.v = INT32_MAX; hi.v = INT32_MIN; CHECK(az_event_less(&lo, &hi));
    lo = hi; lo.v = 1; hi.v = 2; lo.tick = 0xFFFFFFFFu; hi.tick = 0; CHECK(az_event_less(&lo, &hi));
}

int main(void)
{
    test_parse_u64();
    test_parse_dec();
    test_options();
    test_grid();
    test_grid_for();
    test_names();
    test_event_less();
    return az_test_done();
}
