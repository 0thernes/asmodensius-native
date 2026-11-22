/* az_tools.h - helpers shared by the command-line tools (hosted C).
 *
 * One copy of what five tools carried each (findings v34, v56, v53, s4):
 *  - strict options: a value must be a whole number (decimal, or hexadecimal
 *    after 0x) inside the stated range; garbage, partial values ("12abc"),
 *    missing values, repeated options and unknown options are refused;
 *  - the names of the AZ_PROF_* stages, the AZ_EV_* event kinds and the
 *    AZ_ACT_* pair intents;
 *  - the grid rule of the benchmark tools.
 * Every parser prints one line on stderr and returns false on a bad option;
 * the tool then exits with code 2, so it never runs a configuration other
 * than the one typed. All functions are static inline: a tool pays only for
 * what it uses.
 *
 * Use: walk the options once with azt_known (rejects unknown ones), then read
 * each with azt_u32 / azt_u64 / azt_str / azt_flag / azt_grid. `first` is the
 * index of the first option in argv (1, or 2 after a positional argument).
 */
#ifndef AZ_TOOLS_H
#define AZ_TOOLS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "../core/az_abi.h"

_Static_assert(AZ_PROF_COUNT == 10, "azt_prof_name lists every AZ_PROF_* stage");
_Static_assert(AZ_EV_COUNT == 19, "azt_ev_name lists every AZ_EV_* kind");

static inline const char *azt_prof_name(u32 stage)
{
    static const char *const name[AZ_PROF_COUNT] = {
        "reorder", "grid", "perceive", "match", "act", "field", "life", "tier1", "tier2", "ledger"
    };
    return stage < AZ_PROF_COUNT ? name[stage] : "?";
}

/* NULL for a kind the ABI does not define */
static inline const char *azt_ev_name(u32 kind)
{
    static const char *const name[AZ_EV_COUNT] = {
        "(0)", "birth", "death", "clade", "colony", "mate", "attack",
        "trade", "storm", "speciate", "teach", "bond", "tick", "refused",
        "share", "steal", "recruit", "hatch", "stage"
    };
    return (kind > 0 && kind < AZ_EV_COUNT) ? name[kind] : NULL;
}

_Static_assert(AZ_ACT_COUNT == 9, "azt_act_name lists every AZ_ACT_* intent");

static inline const char *azt_act_name(u32 act)
{
    static const char *const name[AZ_ACT_COUNT] = {
        "none", "share", "groom", "teach", "recruit", "attack", "steal", "trade", "mate"
    };
    return act < AZ_ACT_COUNT ? name[act] : "?";
}

/* A whole unsigned number: decimal digits, or 0x and hexadecimal digits.
 * No sign, no spaces, nothing after the digits, no overflow. A decimal number
 * may not start with 0 unless it is 0: the old parsers read "010" as octal 8,
 * so such a value is refused rather than silently read as 10. */
static inline bool azt_parse_u64(const char *s, u64 *out)
{
    if (!s) return false;
    int base = 10;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) { base = 16; s += 2; }
    if (!*s) return false;
    if (base == 10 && s[0] == '0' && s[1] != 0) return false;
    for (const char *p = s; *p; p++) {
        char c = *p;
        bool dig = (c >= '0' && c <= '9') ||
                   (base == 16 && ((c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')));
        if (!dig) return false;
    }
    errno = 0;
    char *end = NULL;
    unsigned long long v = strtoull(s, &end, base);
    if (errno == ERANGE || !end || *end) return false;
    *out = (u64)v;
    return true;
}

/* A plain decimal number: an integer part (0, or digits not starting with 0)
 * and an optional fraction ".digits". No sign, exponent or spaces. */
static inline bool azt_parse_dec(const char *s, double *out)
{
    if (!s || !*s) return false;
    const char *p = s;
    if (*p == '0') p++;
    else if (*p >= '1' && *p <= '9') { while (*p >= '0' && *p <= '9') p++; }
    else return false;
    if (*p == '.') {
        p++;
        if (!(*p >= '0' && *p <= '9')) return false;
        while (*p >= '0' && *p <= '9') p++;
    }
    if (*p) return false;
    *out = strtod(s, NULL);
    return true;
}

/* a kind by name ("birth") or number (1..65535); -1 if neither */
static inline int azt_parse_kind(const char *s)
{
    for (u32 k = 1; k < AZ_EV_COUNT; k++)
        if (!strcmp(s, azt_ev_name(k))) return (int)k;
    u64 v;
    return (azt_parse_u64(s, &v) && v >= 1 && v <= 65535) ? (int)v : -1;
}

/* index of `name` in argv[first..]; -1 if absent, -2 if given twice */
static inline int azt_find(int argc, char **argv, int first, const char *name)
{
    int at = -1;
    for (int i = first; i < argc; i++)
        if (!strcmp(argv[i], name)) {
            if (at >= 0) return -2;
            at = i;
        }
    return at;
}

/* Every token from argv[first] on must be a known option: one of with_value
 * followed by a value, or one of flags (NULL-terminated lists; either may be
 * NULL). A token that is neither, such as a misspelled option, is refused. */
static inline bool azt_known(int argc, char **argv, int first,
                             const char *const *with_value, const char *const *flags)
{
    for (int i = first; i < argc; i++) {
        bool hit = false;
        for (const char *const *p = with_value; p && *p && !hit; p++)
            if (!strcmp(argv[i], *p)) {
                if (i + 1 >= argc) {
                    fprintf(stderr, "option %s needs a value\n", argv[i]);
                    return false;
                }
                hit = true;
                i++;
            }
        for (const char *const *p = flags; p && *p && !hit; p++)
            if (!strcmp(argv[i], *p)) hit = true;
        if (!hit) {
            fprintf(stderr, "unknown option or stray value '%s'\n", argv[i]);
            return false;
        }
    }
    return true;
}

/* The value of `name`, or *out unchanged when the option is absent. */
static inline bool azt_str(int argc, char **argv, int first, const char *name, const char **out)
{
    int i = azt_find(argc, argv, first, name);
    if (i == -1) return true;
    if (i == -2) { fprintf(stderr, "option %s is given twice\n", name); return false; }
    if (i + 1 >= argc || (argv[i + 1][0] == '-' && argv[i + 1][1] == '-')) {
        fprintf(stderr, "option %s needs a value\n", name);
        return false;
    }
    *out = argv[i + 1];
    return true;
}

/* World parameters from "name=value,name=value" (names as in
 * core/az_params.def, any letter case). Call after az_world_create and before
 * az_worldgen. False, with a message, for an unknown name or a value outside
 * the parameter's range. */
static inline bool azt_params(AzWorld *w, const char *list)
{
    const char *p = list;
    while (*p) {
        char name[64];
        u32 n = 0, id = 0;
        while (*p && *p != '=' && *p != ',' && n + 1 < sizeof name) {
            char ch = *p++;
            name[n++] = (ch >= 'a' && ch <= 'z') ? (char)(ch - 'a' + 'A') : ch;
        }
        name[n] = 0;
        if (*p != '=') { fprintf(stderr, "--param: '%s' needs name=value\n", name); return false; }
        p++;
        char *end = NULL;
        unsigned long long v = strtoull(p, &end, 0);
        if (end == p || (*end && *end != ',')) { fprintf(stderr, "--param %s: not a number\n", name); return false; }
        const AzParamDesc *d = NULL;
        for (id = 0; (d = az_param_desc(id)) != NULL; id++)
            if (!strcmp(d->name, name)) break;
        if (!d) { fprintf(stderr, "--param: no world parameter is called '%s'\n", name); return false; }
        if (v < d->lo || v > d->hi || !az_param_set(w, id, (u32)v)) {
            fprintf(stderr, "--param %s: %llu is outside %u..%u (default %u)\n", name, v, d->lo, d->hi, d->def);
            return false;
        }
        p = *end ? end + 1 : end;
    }
    return true;
}

/* A number in [lo, hi]; *v keeps its default when the option is absent. */
static inline bool azt_u64(int argc, char **argv, int first, const char *name, u64 lo, u64 hi, u64 *v)
{
    const char *s = NULL;
    if (!azt_str(argc, argv, first, name, &s)) return false;
    if (!s) return true;
    u64 x = 0;
    if (!azt_parse_u64(s, &x) || x < lo || x > hi) {
        fprintf(stderr, "option %s: '%s' is not a whole number from %llu to %llu\n", name, s,
                (unsigned long long)lo, (unsigned long long)hi);
        return false;
    }
    *v = x;
    return true;
}

/* A plain decimal number in [lo, hi]; *v keeps its default when absent. */
static inline bool azt_f64(int argc, char **argv, int first, const char *name, double lo, double hi, double *v)
{
    const char *s = NULL;
    if (!azt_str(argc, argv, first, name, &s)) return false;
    if (!s) return true;
    double x = 0;
    if (!azt_parse_dec(s, &x) || x < lo || x > hi) {
        fprintf(stderr, "option %s: '%s' is not a decimal number from %g to %g\n", name, s, lo, hi);
        return false;
    }
    *v = x;
    return true;
}

static inline bool azt_u32(int argc, char **argv, int first, const char *name, u32 lo, u32 hi, u32 *v)
{
    u64 x = *v;
    if (!azt_u64(argc, argv, first, name, lo, hi, &x)) return false;
    *v = (u32)x;
    return true;
}

/* *on becomes true when the flag is present; a repeated flag is refused. */
static inline bool azt_flag(int argc, char **argv, int first, const char *name, bool *on)
{
    int i = azt_find(argc, argv, first, name);
    if (i == -2) { fprintf(stderr, "option %s is given twice\n", name); return false; }
    if (i >= 0) *on = true;
    return true;
}

/* "x,y,z" grid bits, each a whole number from 1 to max_bits. *given tells
 * whether the option was present; bits keeps its default when it was not. */
static inline bool azt_grid(int argc, char **argv, int first, const char *name, u32 max_bits,
                            u32 bits[3], bool *given)
{
    const char *s = NULL;
    *given = false;
    if (!azt_str(argc, argv, first, name, &s)) return false;
    if (!s) return true;
    u32 out[3];
    const char *p = s;
    for (u32 k = 0; k < 3; k++) {
        char part[16];
        u32 len = 0;
        while (p[len] && p[len] != ',') len++;
        u64 v = 0;
        bool ok = len > 0 && len < sizeof part;
        if (ok) { memcpy(part, p, len); part[len] = 0; ok = azt_parse_u64(part, &v) && v >= 1 && v <= max_bits; }
        if (!ok || (k < 2 && p[len] != ',') || (k == 2 && p[len] != 0)) {
            fprintf(stderr, "option %s: '%s' is not three whole numbers x,y,z from 1 to %u\n", name, s, max_bits);
            return false;
        }
        out[k] = (u32)v;
        p += len + (k < 2 ? 1 : 0);
    }
    bits[0] = out[0]; bits[1] = out[1]; bits[2] = out[2];
    *given = true;
    return true;
}

/* The grid rule of az_bench, az_budget and az_run's default (finding s4):
 * the largest power-of-two cell count that leaves at least one creature per
 * cell (1 <= n / cells < 2), at least 2^5 cells; z gets the fewest bits and x
 * the most, as evenly as possible (z <= y <= x <= z + 1). The copies this
 * replaced used (b - 1) / 3 for z, which gave x = z + 2 whenever b was a
 * multiple of 3 (for example 3,2,1 at n = 64); found by test_tools. */
static inline void azt_grid_for(u32 n, u32 bits[3])
{
    u32 b = 0;
    while (b < 26 && (2ull << b) <= n) b++;
    if (b < 5) b = 5;
    u32 gz = b / 3, rest = b - gz;
    u32 gx = (rest + 1) / 2, gy = rest - gx;
    bits[0] = gx; bits[1] = gy; bits[2] = gz;
}

#endif /* AZ_TOOLS_H */
