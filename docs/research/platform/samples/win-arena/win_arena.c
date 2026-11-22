// win_arena.c : proof that the Windows host layer works on this machine.
// Build: gcc -O2 -std=c2x -Wall -Wextra win_arena.c -o win_arena.exe
// Parts: (1) locked arena + large-page probe, (2) topology, (4) TSC calibration,
//        (3) pinned vs unpinned RDTSC jitter.  Run with no arguments.
#define _WIN32_WINNT 0x0A00
#define PSAPI_VERSION 2
#include <windows.h>
#include <psapi.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <x86intrin.h>
#include <cpuid.h>

#define ARENA_BYTES ((SIZE_T)256 << 20)

static const char *errtext(DWORD e) {
    static char buf[256];
    DWORD n = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, e,
                             MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), buf, sizeof buf, NULL);
    if (!n) n = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, e, 0, buf, sizeof buf, NULL);
    while (n && (buf[n - 1] == '\r' || buf[n - 1] == '\n' || buf[n - 1] == ' ')) buf[--n] = 0;
    if (!n) snprintf(buf, sizeof buf, "(no message)");
    return buf;
}

static double qpc_seconds(void) {
    static LARGE_INTEGER f;
    LARGE_INTEGER c;
    if (!f.QuadPart) QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart / (double)f.QuadPart;
}

// ---------------------------------------------------------------- part 1: privileges + arena

// Returns 0 when the privilege is now enabled, otherwise the Win32 error
// (1300 ERROR_NOT_ALL_ASSIGNED means the token does not hold the privilege).
static DWORD host_enable_privilege(const char *name) {
    HANDLE tok;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &tok)) return GetLastError();
    TOKEN_PRIVILEGES tp;
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    if (!LookupPrivilegeValueA(NULL, name, &tp.Privileges[0].Luid)) {
        DWORD e = GetLastError();
        CloseHandle(tok);
        return e;
    }
    SetLastError(0);
    BOOL ok = AdjustTokenPrivileges(tok, FALSE, &tp, 0, NULL, NULL);
    DWORD e = GetLastError(); // success can still carry ERROR_NOT_ALL_ASSIGNED
    CloseHandle(tok);
    return ok ? e : (e ? e : ERROR_GEN_FAILURE);
}

static void print_token(void) {
    HANDLE tok;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) {
        printf("  OpenProcessToken failed: %lu\n", GetLastError());
        return;
    }
    TOKEN_ELEVATION el = {0};
    TOKEN_ELEVATION_TYPE et = 0;
    DWORD n = 0;
    GetTokenInformation(tok, TokenElevation, &el, sizeof el, &n);
    GetTokenInformation(tok, TokenElevationType, &et, sizeof et, &n);
    printf("  token: elevated=%lu elevation_type=%d (1=default 2=full 3=limited)\n", el.TokenIsElevated, (int)et);
    static BYTE buf[4096];
    if (GetTokenInformation(tok, TokenPrivileges, buf, sizeof buf, &n)) {
        TOKEN_PRIVILEGES *tp = (TOKEN_PRIVILEGES *)buf;
        printf("  token holds %lu privileges:\n", tp->PrivilegeCount);
        for (DWORD i = 0; i < tp->PrivilegeCount; i++) {
            char nm[128];
            DWORD ln = sizeof nm;
            if (!LookupPrivilegeNameA(NULL, &tp->Privileges[i].Luid, nm, &ln)) strcpy(nm, "?");
            printf("    %-34s %s\n", nm, (tp->Privileges[i].Attributes & SE_PRIVILEGE_ENABLED) ? "enabled" : "disabled");
        }
    }
    CloseHandle(tok);
}

static void print_ws(const char *when) {
    SIZE_T mn = 0, mx = 0;
    DWORD fl = 0;
    if (GetProcessWorkingSetSizeEx(GetCurrentProcess(), &mn, &mx, &fl))
        printf("  working set %s: min=%zu KB max=%zu KB flags=0x%lx%s%s\n", when, mn >> 10, mx >> 10, fl,
               (fl & QUOTA_LIMITS_HARDWS_MIN_ENABLE) ? " HARD_MIN" : "", (fl & QUOTA_LIMITS_HARDWS_MAX_ENABLE) ? " HARD_MAX" : "");
    else
        printf("  GetProcessWorkingSetSizeEx failed: %lu\n", GetLastError());
}

static void print_page_state(void *base, SIZE_T bytes) {
    PSAPI_WORKING_SET_EX_INFORMATION q[3];
    q[0].VirtualAddress = base;
    q[1].VirtualAddress = (char *)base + bytes / 2;
    q[2].VirtualAddress = (char *)base + bytes - 4096;
    if (!QueryWorkingSetEx(GetCurrentProcess(), q, sizeof q)) {
        printf("  QueryWorkingSetEx failed: %lu\n", GetLastError());
        return;
    }
    const char *nm[3] = {"first", "middle", "last"};
    for (int i = 0; i < 3; i++)
        printf("  page %-6s %p: valid=%u locked=%u largepage=%u node=%u\n", nm[i], q[i].VirtualAddress,
               (unsigned)q[i].VirtualAttributes.Valid, (unsigned)q[i].VirtualAttributes.Locked,
               (unsigned)q[i].VirtualAttributes.LargePage, (unsigned)q[i].VirtualAttributes.Node);
}

static void *part1_arena(void) {
    printf("== PART 1: page-locked arena ==\n");
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    printf("  page size=%lu allocation granularity=%lu\n", si.dwPageSize, si.dwAllocationGranularity);
    print_token();
    print_ws("at start");

    double t0 = qpc_seconds();
    void *arena = VirtualAlloc(NULL, ARENA_BYTES, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!arena) {
        DWORD e = GetLastError();
        printf("  VirtualAlloc(%zu MB) FAILED: %lu %s\n", ARENA_BYTES >> 20, e, errtext(e));
        return NULL;
    }
    printf("  VirtualAlloc(%zu MB, RESERVE|COMMIT) ok at %p\n", ARENA_BYTES >> 20, arena);

    // Control experiment: lock before raising the working set minimum.
    if (VirtualLock(arena, ARENA_BYTES)) {
        printf("  control: VirtualLock BEFORE raising working set: succeeded (unexpected); unlocking\n");
        VirtualUnlock(arena, ARENA_BYTES);
    } else {
        DWORD e = GetLastError();
        printf("  control: VirtualLock BEFORE raising working set: FAILED %lu %s\n", e, errtext(e));
    }

    DWORD pe = host_enable_privilege("SeIncreaseWorkingSetPrivilege");
    printf("  enable SeIncreaseWorkingSetPrivilege: %s (code %lu %s)\n", pe ? "FAILED" : "ok", pe, pe ? errtext(pe) : "");

    SIZE_T want_min = ARENA_BYTES + ((SIZE_T)32 << 20);
    SIZE_T want_max = ARENA_BYTES + ((SIZE_T)512 << 20);
    if (SetProcessWorkingSetSizeEx(GetCurrentProcess(), want_min, want_max,
                                   QUOTA_LIMITS_HARDWS_MIN_ENABLE | QUOTA_LIMITS_HARDWS_MAX_DISABLE))
        printf("  SetProcessWorkingSetSizeEx(min=%zu MB, max=%zu MB, HARDWS_MIN_ENABLE|HARDWS_MAX_DISABLE) ok\n",
               want_min >> 20, want_max >> 20);
    else {
        DWORD e = GetLastError();
        printf("  SetProcessWorkingSetSizeEx FAILED: %lu %s\n", e, errtext(e));
    }
    print_ws("after raise");

    double t1 = qpc_seconds();
    if (VirtualLock(arena, ARENA_BYTES)) {
        double t2 = qpc_seconds();
        printf("  VirtualLock(%zu MB) ok in %.1f ms (noisy, parallel load)\n", ARENA_BYTES >> 20, (t2 - t1) * 1e3);
    } else {
        DWORD e = GetLastError();
        printf("  VirtualLock(%zu MB) FAILED: %lu %s\n", ARENA_BYTES >> 20, e, errtext(e));
    }
    print_page_state(arena, ARENA_BYTES);

    // Touch every page; a locked arena must not take a page fault here.
    PROCESS_MEMORY_COUNTERS pmc0 = {0}, pmc1 = {0};
    GetProcessMemoryInfo(GetCurrentProcess(), &pmc0, sizeof pmc0);
    double t3 = qpc_seconds();
    uint64_t sum = 0;
    for (SIZE_T off = 0; off < ARENA_BYTES; off += 4096) {
        ((volatile uint8_t *)arena)[off] = (uint8_t)(off >> 12);
        sum += ((volatile uint8_t *)arena)[off];
    }
    double t4 = qpc_seconds();
    GetProcessMemoryInfo(GetCurrentProcess(), &pmc1, sizeof pmc1);
    printf("  touched %zu pages in %.2f ms, page faults during touch=%lu (checksum %llu), working set now %zu KB\n",
           ARENA_BYTES / 4096, (t4 - t3) * 1e3, pmc1.PageFaultCount - pmc0.PageFaultCount, (unsigned long long)sum,
           pmc1.WorkingSetSize >> 10);
    printf("  total alloc+lock path %.1f ms (noisy, parallel load)\n", (t4 - t0) * 1e3);

    // Large pages.
    printf("-- large pages --\n");
    SIZE_T lp = GetLargePageMinimum();
    printf("  GetLargePageMinimum() = %zu bytes\n", lp);
    DWORD le = host_enable_privilege("SeLockMemoryPrivilege");
    printf("  enable SeLockMemoryPrivilege: %s (code %lu %s)\n", le ? "FAILED" : "ok", le, le ? errtext(le) : "");
    SetLastError(0);
    void *big = VirtualAlloc(NULL, ARENA_BYTES, MEM_RESERVE | MEM_COMMIT | MEM_LARGE_PAGES, PAGE_READWRITE);
    if (big) {
        printf("  VirtualAlloc(%zu MB, MEM_LARGE_PAGES) ok at %p\n", ARENA_BYTES >> 20, big);
        print_page_state(big, ARENA_BYTES);
        VirtualFree(big, 0, MEM_RELEASE);
    } else {
        DWORD e = GetLastError();
        printf("  VirtualAlloc(%zu MB, MEM_LARGE_PAGES) FAILED: %lu %s\n", ARENA_BYTES >> 20, e, errtext(e));
    }
    return arena;
}

// ---------------------------------------------------------------- part 2: topology

#define MAX_LP 64
typedef struct {
    int n_lp;                 // logical processors (group 0 only; this machine has one group)
    int n_groups;
    int max_eff;              // highest EfficiencyClass seen = performance cores
    int eff[MAX_LP];          // EfficiencyClass per LP
    int core[MAX_LP];         // core index per LP (order of RelationProcessorCore records)
    uint32_t l1d_kb[MAX_LP], l2_kb[MAX_LP], l3_kb[MAX_LP];
    uint64_t l1d_share[MAX_LP], l2_share[MAX_LP], l3_share[MAX_LP]; // LP masks sharing that cache
    uint32_t cpuset_id[MAX_LP];
    uint64_t p_mask, e_mask;
} host_topology_t;

static char *mask_str(uint64_t m, char *out, size_t cap) {
    size_t n = 0;
    out[0] = 0;
    for (int i = 0; i < 64; i++) {
        if (!(m >> i & 1)) continue;
        int j = i;
        while (j + 1 < 64 && (m >> (j + 1) & 1)) j++;
        n += (size_t)snprintf(out + n, cap - n, j > i ? "%s%d-%d" : "%s%d", n ? "," : "", i, j);
        i = j;
    }
    return out;
}

static int host_topology(host_topology_t *t) {
    memset(t, 0, sizeof *t);
    DWORD len = 0;
    GetLogicalProcessorInformationEx(RelationAll, NULL, &len);
    BYTE *buf = malloc(len);
    if (!buf || !GetLogicalProcessorInformationEx(RelationAll, (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)buf, &len)) {
        printf("  GetLogicalProcessorInformationEx failed: %lu\n", GetLastError());
        return -1;
    }
    int ncore = 0, npkg = 0, nnuma = 0, ncache = 0;
    char s[128];
    printf("  cache records (GetLogicalProcessorInformationEx RelationCache):\n");
    for (DWORD off = 0; off < len;) {
        SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *p = (void *)(buf + off);
        switch (p->Relationship) {
        case RelationProcessorCore: {
            uint64_t m = p->Processor.GroupMask[0].Mask;
            for (int i = 0; i < MAX_LP; i++)
                if (m >> i & 1) {
                    t->eff[i] = p->Processor.EfficiencyClass;
                    t->core[i] = ncore;
                    if (i + 1 > t->n_lp) t->n_lp = i + 1;
                    if (p->Processor.EfficiencyClass > t->max_eff) t->max_eff = p->Processor.EfficiencyClass;
                }
            ncore++;
            break;
        }
        case RelationCache: {
            uint64_t m = p->Cache.GroupMask.Mask;
            const char *ty = p->Cache.Type == CacheData ? "D" : p->Cache.Type == CacheInstruction ? "I" : p->Cache.Type == CacheUnified ? "U" : "T";
            printf("    L%u%s %6lu KB  line=%u assoc=%3u  shared by LP {%s}\n", p->Cache.Level, ty, p->Cache.CacheSize >> 10,
                   p->Cache.LineSize, p->Cache.Associativity, mask_str(m, s, sizeof s));
            for (int i = 0; i < MAX_LP; i++)
                if (m >> i & 1) {
                    if (p->Cache.Level == 1 && p->Cache.Type == CacheData) t->l1d_kb[i] = p->Cache.CacheSize >> 10, t->l1d_share[i] = m;
                    if (p->Cache.Level == 2) t->l2_kb[i] = p->Cache.CacheSize >> 10, t->l2_share[i] = m;
                    if (p->Cache.Level == 3) t->l3_kb[i] = p->Cache.CacheSize >> 10, t->l3_share[i] = m;
                }
            ncache++;
            break;
        }
        case RelationProcessorPackage: npkg++; break;
        case RelationNumaNode: nnuma++; break;
        case RelationGroup: t->n_groups = p->Group.ActiveGroupCount; break;
        default: break;
        }
        off += p->Size;
    }
    free(buf);
    printf("  records: cores=%d packages=%d numa nodes=%d caches=%d processor groups=%d logical processors=%d\n", ncore, npkg,
           nnuma, ncache, t->n_groups, t->n_lp);

    // CPU sets.
    ULONG clen = 0;
    GetSystemCpuSetInformation(NULL, 0, &clen, GetCurrentProcess(), 0);
    BYTE *cb = malloc(clen);
    if (!cb || !GetSystemCpuSetInformation((PSYSTEM_CPU_SET_INFORMATION)cb, clen, &clen, GetCurrentProcess(), 0)) {
        printf("  GetSystemCpuSetInformation failed: %lu\n", GetLastError());
        return -1;
    }
    printf("  GetSystemCpuSetInformation:\n");
    printf("    cpuset_id grp lp core llc numa eff sched parked alloc alloc_me realtime\n");
    for (ULONG off = 0; off < clen;) {
        SYSTEM_CPU_SET_INFORMATION *c = (void *)(cb + off);
        if (c->Type == CpuSetInformation) {
            int lpi = c->CpuSet.LogicalProcessorIndex;
            if (c->CpuSet.Group == 0 && lpi < MAX_LP) t->cpuset_id[lpi] = c->CpuSet.Id;
            printf("    %9lu %3u %2u %4u %3u %4u %3u %5u %6u %5u %8u %8u\n", c->CpuSet.Id, c->CpuSet.Group, lpi, c->CpuSet.CoreIndex,
                   c->CpuSet.LastLevelCacheIndex, c->CpuSet.NumaNodeIndex, c->CpuSet.EfficiencyClass, c->CpuSet.SchedulingClass,
                   c->CpuSet.Parked, c->CpuSet.Allocated, c->CpuSet.AllocatedToTargetProcess, c->CpuSet.RealTime);
        }
        off += c->Size;
    }
    free(cb);

    for (int i = 0; i < t->n_lp; i++) {
        if (t->eff[i] == t->max_eff) t->p_mask |= 1ull << i;
        else t->e_mask |= 1ull << i;
    }
    return 0;
}

static void part2_topology(host_topology_t *t) {
    printf("== PART 2: topology ==\n");
    if (host_topology(t)) return;
    char a[128], b[128];
    printf("  per logical processor:\n");
    printf("    lp core eff type cpuset  L1D_KB  L2_KB L2_shared_by      L3_KB L3_shared_by\n");
    for (int i = 0; i < t->n_lp; i++)
        printf("    %2d %4d %3d   %c  %6u  %6u %6u {%-12s}  %6u {%s}\n", i, t->core[i], t->eff[i], t->eff[i] == t->max_eff ? 'P' : 'E',
               t->cpuset_id[i], t->l1d_kb[i], t->l2_kb[i], mask_str(t->l2_share[i], a, sizeof a), t->l3_kb[i],
               mask_str(t->l3_share[i], b, sizeof b));
    printf("  P cores (EfficiencyClass %d): LP {%s}  count=%d\n", t->max_eff, mask_str(t->p_mask, a, sizeof a), __builtin_popcountll(t->p_mask));
    printf("  E cores (lower class):       LP {%s}  count=%d\n", mask_str(t->e_mask, a, sizeof a), __builtin_popcountll(t->e_mask));
    if (!t->e_mask) printf("  note: only one EfficiencyClass reported; not a hybrid view\n");
}

// ---------------------------------------------------------------- part 4: TSC calibration

// One (tsc, qpc) sample: the QPC read with the tightest RDTSC bracket out of 32 tries.
static uint64_t tsc_qpc_pair(uint64_t *tsc, int64_t *qpc) {
    uint64_t best = ~0ull;
    for (int i = 0; i < 32; i++) {
        LARGE_INTEGER q;
        uint64_t a = __rdtsc();
        QueryPerformanceCounter(&q);
        uint64_t b = __rdtsc();
        if (b - a < best) best = b - a, *tsc = a + (b - a) / 2, *qpc = q.QuadPart;
    }
    return best;
}

// Returns TSC ticks per second measured against QueryPerformanceCounter.
static double host_tsc_calibrate(int windows, DWORD window_ms, int verbose) {
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    uint64_t t0 = 0, t1 = 0, tfirst;
    int64_t q0 = 0, q1 = 1, qfirst;
    double lo = 1e300, hi = 0, sum = 0;
    uint64_t br = tsc_qpc_pair(&t0, &q0);
    tfirst = t0, qfirst = q0;
    for (int i = 0; i < windows; i++) {
        Sleep(window_ms);
        uint64_t br1 = tsc_qpc_pair(&t1, &q1);
        double hz = (double)(t1 - t0) * (double)f.QuadPart / (double)(q1 - q0);
        if (verbose) printf("    window %d: %.3f ms  tsc=%.1f Hz  (qpc read bracket %llu / %llu ticks)\n", i, (double)(q1 - q0) * 1e3 / (double)f.QuadPart, hz, (unsigned long long)br, (unsigned long long)br1);
        if (hz < lo) lo = hz;
        if (hz > hi) hi = hz;
        sum += hz;
        t0 = t1, q0 = q1, br = br1;
    }
    double whole = (double)(t1 - tfirst) * (double)f.QuadPart / (double)(q1 - qfirst);
    if (verbose) {
        printf("    per-window mean %.1f Hz, min %.1f, max %.1f, spread %.2f ppm\n", sum / windows, lo, hi, (hi - lo) / whole * 1e6);
        printf("    whole span (%d x %lu ms): %.1f Hz = %.6f ns per tick\n", windows, window_ms, whole, 1e9 / whole);
    }
    return whole;
}

static double part4_calibrate(void) {
    printf("== PART 4: RDTSC vs QueryPerformanceCounter ==\n");
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    printf("  QueryPerformanceFrequency = %lld Hz\n", (long long)f.QuadPart);
    unsigned a, b, c, d;
    if (__get_cpuid(0x80000007, &a, &b, &c, &d)) printf("  CPUID 80000007h EDX bit 8 (invariant TSC) = %u\n", (d >> 8) & 1);
    if (__get_cpuid_max(0, NULL) >= 0x15) {
        __cpuid_count(0x15, 0, a, b, c, d);
        printf("  CPUID 15h: denominator=%u numerator=%u crystal_hz=%u", a, b, c);
        if (a && b && c) printf("  => nominal TSC %.1f Hz", (double)c * b / a);
        printf("\n");
    }
    if (__get_cpuid_max(0, NULL) >= 0x16) {
        __cpuid_count(0x16, 0, a, b, c, d);
        printf("  CPUID 16h: base=%u MHz max=%u MHz bus=%u MHz\n", a & 0xffff, b & 0xffff, c & 0xffff);
    }
    double hz = host_tsc_calibrate(5, 200, 1);
    // cost of the two clocks
    enum { N = 2000000 };
    LARGE_INTEGER q;
    uint64_t s0 = __rdtsc();
    for (int i = 0; i < N; i++) QueryPerformanceCounter(&q);
    uint64_t s1 = __rdtsc();
    uint64_t acc = 0;
    for (int i = 0; i < N; i++) acc += __rdtsc();
    uint64_t s2 = __rdtsc();
    printf("  cost per call (noisy, parallel load): QueryPerformanceCounter %.1f ns, RDTSC %.1f ns (sink %llu)\n",
           (double)(s1 - s0) / N * 1e9 / hz, (double)(s2 - s1) / N * 1e9 / hz, (unsigned long long)(acc & 1));
    return hz;
}

// ---------------------------------------------------------------- part 3: jitter

enum { PIN_NONE = 0, PIN_CPUSET = 1, PIN_AFFINITY = 2 };
#define TOPN 8
typedef struct {
    const char *label;
    int pin_mode;
    int lp;            // target logical processor (pinned modes)
    ULONG cpuset_id;
    int prio;          // SetThreadPriority value
    double tsc_hz, seconds;
    // results
    DWORD pin_err, prio_err;
    int prio_readback;
    uint64_t iters, min_gap, top[TOPN], top_at[TOPN];
    uint64_t bins[5]; // [1,10)us [10,100)us [100us,1ms) [1,10)ms >=10ms
    uint64_t stolen;  // ticks spent in gaps >= 1 us
    uint64_t seen;    // LP mask observed
    uint32_t migrations;
    uint64_t elapsed;
} trial_t;

// Pin the calling thread. PIN_CPUSET uses SetThreadSelectedCpuSets, PIN_AFFINITY uses
// SetThreadAffinityMask. Returns 0 or the Win32 error.
static DWORD host_pin_thread(int mode, int lp, ULONG cpuset_id) {
    if (mode == PIN_CPUSET) {
        if (!SetThreadSelectedCpuSets(GetCurrentThread(), &cpuset_id, 1)) return GetLastError();
    } else if (mode == PIN_AFFINITY) {
        if (!SetThreadAffinityMask(GetCurrentThread(), (DWORD_PTR)1 << lp)) return GetLastError();
    }
    return 0;
}

static DWORD WINAPI jitter_thread(void *arg) {
    trial_t *t = arg;
    t->pin_err = host_pin_thread(t->pin_mode, t->lp, t->cpuset_id);
    t->prio_err = SetThreadPriority(GetCurrentThread(), t->prio) ? 0 : GetLastError();
    t->prio_readback = GetThreadPriority(GetCurrentThread());
    Sleep(1); // let the scheduler move us onto the selected processor
    const uint64_t us = (uint64_t)(t->tsc_hz * 1e-6);
    uint64_t min_gap = ~0ull, iters = 0;
    DWORD cpu = GetCurrentProcessorNumber();
    t->seen = 1ull << cpu;
    uint64_t start = __rdtsc(), prev = start, end = start + (uint64_t)(t->seconds * t->tsc_hz);
    while (prev < end) {
        uint64_t now = __rdtsc();
        uint64_t d = now - prev;
        if (d < min_gap) min_gap = d;
        if (__builtin_expect(d >= us, 0)) {
            int b = d < 10 * us ? 0 : d < 100 * us ? 1 : d < 1000 * us ? 2 : d < 10000 * us ? 3 : 4;
            t->bins[b]++;
            t->stolen += d;
            if (d > t->top[TOPN - 1]) {
                int k = TOPN - 1;
                while (k > 0 && t->top[k - 1] < d) t->top[k] = t->top[k - 1], t->top_at[k] = t->top_at[k - 1], k--;
                t->top[k] = d, t->top_at[k] = now - start;
            }
            DWORD c2 = GetCurrentProcessorNumber();
            if (c2 != cpu) t->migrations++, cpu = c2, t->seen |= 1ull << c2;
        }
        prev = now;
        iters++;
    }
    t->elapsed = prev - start;
    t->iters = iters;
    t->min_gap = min_gap;
    return 0;
}

static void run_trial(trial_t *t) {
    HANDLE h = CreateThread(NULL, 0, jitter_thread, t, 0, NULL);
    if (!h) {
        printf("  CreateThread failed: %lu\n", GetLastError());
        return;
    }
    WaitForSingleObject(h, INFINITE);
    CloseHandle(h);
    double ns = 1e9 / t->tsc_hz;
    char s[128];
    printf("  [%s]\n", t->label);
    printf("    pin: %s (err %lu)  priority set err %lu, readback %d  ran on LP {%s}  migrations=%u\n",
           t->pin_mode == PIN_CPUSET ? "SetThreadSelectedCpuSets" : t->pin_mode == PIN_AFFINITY ? "SetThreadAffinityMask" : "none",
           t->pin_err, t->prio_err, t->prio_readback, mask_str(t->seen, s, sizeof s), t->migrations);
    printf("    iterations=%llu in %.3f s  mean gap=%.2f ns  min gap=%.2f ns (%llu ticks)\n", (unsigned long long)t->iters,
           (double)t->elapsed / t->tsc_hz, (double)t->elapsed / (double)t->iters * ns, (double)t->min_gap * ns, (unsigned long long)t->min_gap);
    printf("    gaps: [1,10)us=%llu [10,100)us=%llu [100us,1ms)=%llu [1,10)ms=%llu >=10ms=%llu  time in gaps>=1us: %.3f ms (%.4f %%)\n",
           (unsigned long long)t->bins[0], (unsigned long long)t->bins[1], (unsigned long long)t->bins[2], (unsigned long long)t->bins[3],
           (unsigned long long)t->bins[4], (double)t->stolen * ns / 1e6, 100.0 * (double)t->stolen / (double)t->elapsed);
    printf("    largest gaps (us @ s):");
    for (int i = 0; i < TOPN && t->top[i]; i++) printf(" %.1f@%.2f", (double)t->top[i] * ns / 1e3, (double)t->top_at[i] / t->tsc_hz);
    printf("\n");
}

static void part3_jitter(const host_topology_t *topo, double tsc_hz) {
    printf("== PART 3: RDTSC spin-loop jitter, 3 s per trial (noisy, parallel load) ==\n");
    int p_lp = -1, e_lp = -1;
    for (int i = 0; i < topo->n_lp; i++) {
        if (topo->p_mask >> i & 1) p_lp = i; // last P core; LP 0 is avoided on purpose
        if (topo->e_mask >> i & 1) e_lp = i;
    }
    if (p_lp < 0) {
        printf("  no topology, skipping\n");
        return;
    }
    printf("  process priority class 0x%lx; chosen P core LP %d (cpuset id %u), E core LP %d (cpuset id %u)\n",
           GetPriorityClass(GetCurrentProcess()), p_lp, topo->cpuset_id[p_lp], e_lp, e_lp >= 0 ? topo->cpuset_id[e_lp] : 0);
    ULONG e_id = e_lp >= 0 ? topo->cpuset_id[e_lp] : 0;
    trial_t tr[7] = {
        {.label = "A unpinned, THREAD_PRIORITY_NORMAL", .pin_mode = PIN_NONE, .prio = THREAD_PRIORITY_NORMAL},
        {.label = "B unpinned, THREAD_PRIORITY_TIME_CRITICAL", .pin_mode = PIN_NONE, .prio = THREAD_PRIORITY_TIME_CRITICAL},
        {.label = "C P core via SetThreadSelectedCpuSets, TIME_CRITICAL", .pin_mode = PIN_CPUSET, .lp = p_lp, .cpuset_id = topo->cpuset_id[p_lp], .prio = THREAD_PRIORITY_TIME_CRITICAL},
        {.label = "D P core via SetThreadAffinityMask, TIME_CRITICAL", .pin_mode = PIN_AFFINITY, .lp = p_lp, .cpuset_id = topo->cpuset_id[p_lp], .prio = THREAD_PRIORITY_TIME_CRITICAL},
        {.label = "E E core via SetThreadSelectedCpuSets, TIME_CRITICAL", .pin_mode = PIN_CPUSET, .lp = e_lp, .cpuset_id = e_id, .prio = THREAD_PRIORITY_TIME_CRITICAL},
        {.label = "F E core via SetThreadSelectedCpuSets, THREAD_PRIORITY_NORMAL", .pin_mode = PIN_CPUSET, .lp = e_lp, .cpuset_id = e_id, .prio = THREAD_PRIORITY_NORMAL},
        {.label = "G E core via SetThreadAffinityMask, TIME_CRITICAL", .pin_mode = PIN_AFFINITY, .lp = e_lp, .cpuset_id = e_id, .prio = THREAD_PRIORITY_TIME_CRITICAL},
    };
    for (int i = 0; i < 7; i++) {
        if (i >= 4 && e_lp < 0) break;
        tr[i].tsc_hz = tsc_hz;
        tr[i].seconds = 3.0;
        run_trial(&tr[i]);
    }

    // Smallest back-to-back RDTSC delta, plain and with LFENCE, hard-pinned to each core type.
    printf("  back-to-back RDTSC delta over 4,000,000 reads (hard affinity):\n");
    DWORD_PTR old = SetThreadAffinityMask(GetCurrentThread(), (DWORD_PTR)1 << p_lp);
    for (int pass = 0; pass < 2; pass++) {
        int lp = pass ? e_lp : p_lp;
        if (lp < 0) break;
        SetThreadAffinityMask(GetCurrentThread(), (DWORD_PTR)1 << lp);
        Sleep(1);
        uint64_t mn = ~0ull, mnf = ~0ull, zeros = 0, prev = __rdtsc();
        for (int i = 0; i < 4000000; i++) {
            uint64_t now = __rdtsc(), d = now - prev;
            if (d < mn) mn = d;
            zeros += d == 0;
            prev = now;
        }
        _mm_lfence();
        prev = __rdtsc();
        for (int i = 0; i < 4000000; i++) {
            _mm_lfence();
            uint64_t now = __rdtsc(), d = now - prev;
            if (d < mnf) mnf = d;
            prev = now;
        }
        printf("    %c core LP %2d (on LP %lu): plain min=%llu ticks (zero deltas=%llu), with LFENCE min=%llu ticks (%.2f ns)\n",
               pass ? 'E' : 'P', lp, GetCurrentProcessorNumber(), (unsigned long long)mn, (unsigned long long)zeros,
               (unsigned long long)mnf, (double)mnf * 1e9 / tsc_hz);
    }
    if (old) SetThreadAffinityMask(GetCurrentThread(), old);
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    void *arena = part1_arena();
    static host_topology_t topo;
    part2_topology(&topo);
    double hz = part4_calibrate();
    part3_jitter(&topo, hz);
    if (arena) {
        VirtualUnlock(arena, ARENA_BYTES);
        VirtualFree(arena, 0, MEM_RELEASE);
    }
    return 0;
}
