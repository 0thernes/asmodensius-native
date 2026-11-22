/* az_host_win.c - Windows host layer. See az_host_win.h. */
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <immintrin.h>
#include <bcrypt.h>
#include "az_host_win.h"

/* ---------------------------------------------------------------------- */
/* topology: which logical processors are performance cores                */
/* ---------------------------------------------------------------------- */
void az_win_topology_classify(AzWinInfo *info, const AzWinCpuSet *sets, u32 n)
{
    info->n_logical = info->n_pcores = info->n_ecores = info->n_smt = 0;
    u8 max_class = 0;
    for (u32 i = 0; i < n; i++)
        if (sets[i].group == 0 && sets[i].eff_class > max_class) max_class = sets[i].eff_class;
    /* Group 0 holds at most 64 logical processors (indices 0..63). Walking them
     * in index order makes the lowest thread of each core the one that counts,
     * so the result does not depend on the order of the list. */
    u64 core_seen = 0;
    for (u32 lp = 0; lp < 64; lp++) {
        for (u32 i = 0; i < n; i++) {
            const AzWinCpuSet *c = &sets[i];
            if (c->group != 0 || c->lp != lp) continue;
            info->n_logical++;
            const u32 core = c->core < 64 ? c->core : lp;     /* out of range: its own core */
            if ((core_seen >> core) & 1u) { info->n_smt++; continue; }
            core_seen |= 1ull << core;
            if (c->eff_class == max_class) info->pcore_lp[info->n_pcores++] = lp;
            else info->ecore_lp[info->n_ecores++] = lp;
        }
    }
}

void az_win_topology(AzWinInfo *info)
{
    info->n_logical = info->n_pcores = info->n_ecores = info->n_smt = 0;
    ULONG len = 0;
    GetSystemCpuSetInformation(NULL, 0, &len, GetCurrentProcess(), 0);
    if (!len) return;
    BYTE *buf = (BYTE *)malloc(len);
    if (!buf) return;
    if (!GetSystemCpuSetInformation((PSYSTEM_CPU_SET_INFORMATION)buf, len, &len, GetCurrentProcess(), 0)) {
        free(buf);
        return;
    }
    u32 cap = 0;                                  /* entries have the Size the OS gives them */
    for (ULONG off = 0; off < len;) {
        PSYSTEM_CPU_SET_INFORMATION si = (PSYSTEM_CPU_SET_INFORMATION)(buf + off);
        if (si->Size == 0) break;
        cap++;
        off += si->Size;
    }
    AzWinCpuSet *sets = (AzWinCpuSet *)malloc((size_t)(cap ? cap : 1) * sizeof(AzWinCpuSet));
    if (!sets) { free(buf); return; }
    u32 n = 0;
    for (ULONG off = 0; off < len && n < cap;) {
        PSYSTEM_CPU_SET_INFORMATION si = (PSYSTEM_CPU_SET_INFORMATION)(buf + off);
        if (si->Size == 0) break;
        if (si->Type == CpuSetInformation) {
            sets[n].lp = si->CpuSet.LogicalProcessorIndex;
            sets[n].core = si->CpuSet.CoreIndex;
            sets[n].group = si->CpuSet.Group;
            sets[n].eff_class = si->CpuSet.EfficiencyClass;
            n++;
        }
        off += si->Size;
    }
    az_win_topology_classify(info, sets, n);
    free(buf);
    free(sets);
}

/* ---------------------------------------------------------------------- */
/* arena                                                                   */
/* ---------------------------------------------------------------------- */
static bool enable_privilege(const char *name)
{
    HANDLE tok;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &tok)) return false;
    TOKEN_PRIVILEGES tp;
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    bool ok = LookupPrivilegeValueA(NULL, name, &tp.Privileges[0].Luid) &&
              AdjustTokenPrivileges(tok, FALSE, &tp, 0, NULL, NULL) &&
              GetLastError() == ERROR_SUCCESS;
    CloseHandle(tok);
    return ok;
}

/* append "; <text>" to info->note (or the text alone if the note is empty) */
static void note_add(AzWinInfo *info, const char *fmt, ...)
{
    size_t n = strlen(info->note);
    if (n + 3 >= sizeof info->note) return;
    if (n) { info->note[n++] = ';'; info->note[n++] = ' '; info->note[n] = 0; }
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(info->note + n, sizeof info->note - n, fmt, ap);
    va_end(ap);
}

bool az_win_arena(AzArena *a, u64 bytes, u32 flags, AzWinInfo *info)
{
    a->base = NULL; a->size = 0; a->used = 0;
    info->arena_locked = 0; info->arena_large = 0; info->arena_refused = 0; info->last_error = 0;
    info->note[0] = 0;
    void *p = NULL;

    if (flags & AZ_WIN_LARGE_PAGES) {
        SIZE_T lp = GetLargePageMinimum();
        if (lp && enable_privilege("SeLockMemoryPrivilege")) {
            SIZE_T sz = (SIZE_T)((bytes + lp - 1) / lp * lp);
            p = VirtualAlloc(NULL, sz, MEM_RESERVE | MEM_COMMIT | MEM_LARGE_PAGES, PAGE_READWRITE);
            if (p) { info->arena_large = 1; info->arena_locked = 1; bytes = sz; }   /* large pages are never paged */
            else info->last_error = GetLastError();
        } else {
            info->last_error = GetLastError();
        }
        if (!p) note_add(info, "large pages unavailable (error %lu)", (unsigned long)info->last_error);
    }
    if (!p) {
        SIZE_T sz = (SIZE_T)((bytes + 4095) & ~(u64)4095);
        p = VirtualAlloc(NULL, sz, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        if (!p) {
            info->last_error = GetLastError();
            note_add(info, "VirtualAlloc of %llu bytes failed (error %lu)", (unsigned long long)sz,
                     (unsigned long)info->last_error);
            return false;
        }
        bytes = sz;
        if (flags & AZ_WIN_LOCK) {
            DWORD ws_error = 0;
            enable_privilege("SeIncreaseWorkingSetPrivilege");
            SIZE_T mn = sz + ((SIZE_T)64 << 20), mx = sz + ((SIZE_T)512 << 20);
            if (!SetProcessWorkingSetSizeEx(GetCurrentProcess(), mn, mx,
                                            QUOTA_LIMITS_HARDWS_MIN_ENABLE | QUOTA_LIMITS_HARDWS_MAX_DISABLE))
                ws_error = GetLastError();
            DWORD lock_error = VirtualLock(p, sz) ? ERROR_SUCCESS : GetLastError();
            /* The working-set quota is per process. A grid comparison or a
             * snapshot restore can keep several other arenas resident. Windows
             * can reject the single-arena size itself when it is too small. */
            if (lock_error == ERROR_WORKING_SET_QUOTA && (!ws_error || ws_error == ERROR_INVALID_PARAMETER)) {
                PROCESS_MEMORY_COUNTERS counters;
                if (GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof counters) &&
                    counters.WorkingSetSize <= SIZE_MAX - mx) {
                    if (SetProcessWorkingSetSizeEx(GetCurrentProcess(), mn + counters.WorkingSetSize,
                                                   mx + counters.WorkingSetSize,
                                                   QUOTA_LIMITS_HARDWS_MIN_ENABLE | QUOTA_LIMITS_HARDWS_MAX_DISABLE)) {
                        ws_error = ERROR_SUCCESS;
                        lock_error = VirtualLock(p, sz) ? ERROR_SUCCESS : GetLastError();
                    } else ws_error = GetLastError();
                }
            }
            if (!lock_error) {
                info->arena_locked = 1;
            } else {
                info->last_error = lock_error;
                note_add(info, "VirtualLock failed (error %lu)", (unsigned long)info->last_error);
                if (ws_error) note_add(info, "raising the working set failed (error %lu)", (unsigned long)ws_error);
            }
        }
    }
    /* The owner's law: no reliance on the pagefile. A lock that was asked for
     * and failed is an error, never a silent fallback (finding v48). */
    if ((flags & AZ_WIN_LOCK) && !info->arena_locked && !(flags & AZ_WIN_ALLOW_PAGEABLE)) {
        VirtualFree(p, 0, MEM_RELEASE);
        info->arena_refused = 1;
        note_add(info, "refused: the arena would be pageable");
        return false;
    }
    if (!info->arena_locked)
        note_add(info, (flags & AZ_WIN_LOCK) ? "pageable: allowed by the caller" : "pageable: no lock requested");
    a->base = (u8 *)p;
    a->size = bytes;
    a->used = 0;
    return true;
}

void az_win_arena_free(AzArena *a)
{
    if (a->base) VirtualFree(a->base, 0, MEM_RELEASE);
    a->base = NULL; a->size = 0; a->used = 0;
}

/* ---------------------------------------------------------------------- */
/* worker pool                                                             */
/* ---------------------------------------------------------------------- */
/* Design (findings v65, v66, v12, v19, v17 and s3 of the 2026-11-17 review):
 *  - Soft placement. Each worker, and the calling thread as worker 0, gets an
 *    ideal processor (P cores first) but keeps the process affinity, so the
 *    scheduler runs it elsewhere when that processor is busy. AZ_WIN_NO_PIN
 *    leaves placement to the OS. Priorities are not raised.
 *  - Dynamic parts. max_parts is AZ_MAX_PARTS, so the core cuts a region into
 *    about eight parts per worker. Every thread, the caller included, takes
 *    parts from one counter, so a slow, preempted or sleeping worker only
 *    runs fewer parts. It holds the region up only while it is inside a part.
 *  - A region ends when all of its parts are done. Workers that have no part
 *    are never waited for; a worker that wakes after the region ended finds
 *    nothing to take and waits again.
 *  - Generation check. The counter word holds the region's generation in its
 *    upper bits, and a part is claimed only by a compare-exchange of the whole
 *    word, so a late worker cannot claim a part of a newer region. The two
 *    job descriptors alternate by generation parity; the one of generation g
 *    is rewritten only for g + 2, after the counter has left g, so a claim
 *    that succeeds always reads its own region's descriptor.
 *  - Cheaper wake-up. An idle worker spins for AZ_WIN_SPIN_US, then sleeps in
 *    WaitOnAddress on the generation word. If a worker sleeps, the caller
 *    wakes one (WakeByAddressSingle, about 3 us measured) and starts on the
 *    parts at once; it never waits for a woken worker to arrive. The first
 *    worker that takes a part while more are left wakes all the others
 *    (WakeByAddressAll, 11 to 30 us measured for 7 to 15 sleepers), so that
 *    cost is off the caller's path, and every sleeper is woken while parts
 *    remain even if the caller is held up inside a long part. */
#define AZ_WIN_SPIN_US 200u   /* an idle worker spins this long, then sleeps */
#define POOL_IDX_BITS  16u
#define POOL_IDX_MASK  ((1u << POOL_IDX_BITS) - 1u)
_Static_assert(AZ_MAX_PARTS < (1u << POOL_IDX_BITS), "a part index must fit the claim word");

typedef struct PoolJob {
    AzJobFn         fn;
    struct AzWorld *world;
    const u32      *splits;
    u32             n_parts;
} PoolJob;

typedef struct WinPool {
    AzHost  host;                       /* the host's ctx is the pool itself       */
    HANDLE  threads[AZ_MAX_WORKERS];
    u32     n_workers;
    u32     lp[AZ_MAX_WORKERS];         /* ideal logical processor of each worker  */
    u32     soft;                       /* 1: ideal processors are set             */
    u64     spin_ticks;                 /* QPC ticks an idle worker spins          */
    u64     next_gen;                   /* caller only: generation of the last region */
    LARGE_INTEGER qpf;
    HANDLE  caller;                     /* the creating thread, worker 0           */
    PROCESSOR_NUMBER caller_ideal;      /* its ideal processor before the pool     */
    u32     caller_saved;
    PoolJob job[2];                     /* generation g uses job[g & 1]            */

    /* hot words, one cache line each, so that spinning on one does not slow
     * the others (finding v19) */
    u64 claim AZ_ALIGNED(64);           /* (generation << POOL_IDX_BITS) | next part */
    u32 done  AZ_ALIGNED(64);           /* parts finished in the current region    */
    u64 gen   AZ_ALIGNED(64);           /* generation posted last: the wait word   */
    u32 sleepers;                       /* workers inside WaitOnAddress            */
    u32 woke_all;                       /* 1 once a worker woke the others for this region */
    u32 quit;
} WinPool;

typedef struct WorkerArg { WinPool *pool; u32 index; } WorkerArg;

static u64 qpc_now(void)
{
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return (u64)t.QuadPart;
}

static BOOL place_thread(const WinPool *p, HANDLE th, u32 t, PROCESSOR_NUMBER *prev)
{
    PROCESSOR_NUMBER pn;
    memset(&pn, 0, sizeof pn);
    pn.Group = 0;
    pn.Number = (BYTE)p->lp[t];
    return SetThreadIdealProcessorEx(th, &pn, prev);
}

/* Claim and run parts of region g until none is left, or until the counter
 * shows a newer region (this thread was late). A worker (t > 0) that claims a
 * part while more are left wakes every sleeping worker, once per region. */
static void pool_drain(WinPool *p, u64 g, u32 t)
{
    PoolJob *j = &p->job[g & 1];
    u64 c = __atomic_load_n(&p->claim, __ATOMIC_ACQUIRE);
    for (;;) {
        if ((c >> POOL_IDX_BITS) != g) return;
        u32 k = (u32)(c & POOL_IDX_MASK);
        const u32 n = __atomic_load_n(&j->n_parts, __ATOMIC_RELAXED);
        if (k >= n) return;
        if (!__atomic_compare_exchange_n(&p->claim, &c, c + 1, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))
            continue;                    /* c now holds the current word */
        /* the claim succeeded, so n is this region's part count */
        if (t != 0 && k + 1 < n && __atomic_load_n(&p->sleepers, __ATOMIC_SEQ_CST) &&
            !__atomic_exchange_n(&p->woke_all, 1u, __ATOMIC_ACQ_REL))
            WakeByAddressAll((PVOID)&p->gen);
        j->fn(j->world, j->splits[k], j->splits[k + 1], t);
        __atomic_fetch_add(&p->done, 1u, __ATOMIC_RELEASE);
        c = __atomic_load_n(&p->claim, __ATOMIC_ACQUIRE);
    }
}

/* Wait until a generation other than `seen` is posted; 0 means quit. */
static u64 pool_wait(WinPool *p, u64 seen)
{
    u64 start = qpc_now();
    for (u32 spins = 1;; spins++) {
        if (__atomic_load_n(&p->quit, __ATOMIC_ACQUIRE)) return 0;
        u64 g = __atomic_load_n(&p->gen, __ATOMIC_ACQUIRE);
        if (g != seen) return g;
        _mm_pause();
        if ((spins & 63u) || qpc_now() - start < p->spin_ticks) continue;
        /* Announce the sleep, then look again: a region posted after the look
         * sees sleepers > 0 and wakes us, and one posted before it is seen. */
        __atomic_fetch_add(&p->sleepers, 1u, __ATOMIC_SEQ_CST);
        if (__atomic_load_n(&p->gen, __ATOMIC_SEQ_CST) == seen && !__atomic_load_n(&p->quit, __ATOMIC_SEQ_CST))
            WaitOnAddress(&p->gen, &seen, sizeof seen, INFINITE);
        __atomic_fetch_sub(&p->sleepers, 1u, __ATOMIC_SEQ_CST);
        start = qpc_now();
    }
}

static DWORD WINAPI worker_main(LPVOID arg)
{
    WorkerArg *wa = (WorkerArg *)arg;
    WinPool *p = wa->pool;
    const u32 t = wa->index;
    free(wa);
    if (p->soft) place_thread(p, GetCurrentThread(), t, NULL);
    /* No wait-completion boost: a woken worker with a boost was put on the
     * caller's processor and preempted the caller inside WakeByAddressAll,
     * then spun there for AZ_WIN_SPIN_US. Measured 2026-11-17 (8 workers, an
     * empty region after a 3 ms gap, noisy): post and wake 215 us median with
     * the boost, 13 us without it. Base priorities are equal either way. */
    SetThreadPriorityBoost(GetCurrentThread(), TRUE);
    u64 seen = 0;
    for (;;) {
        u64 g = pool_wait(p, seen);
        if (!g) return 0;
        seen = g;
        pool_drain(p, g, t);
    }
}

/* Fresh entropy for a wild world: the operating system's generator (which
 * Windows keeps reseeding from the processor's hardware source and from
 * interrupt timings), then stirred with the time-stamp counter and the
 * performance counter, so that two calls differ even if the generator were
 * ever to repeat itself. No seed of ours goes in. */
bool az_win_entropy(void *ctx, void *buf, u32 bytes)
{
    (void)ctx;
    if (BCryptGenRandom(NULL, (PUCHAR)buf, bytes, BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) return false;
    LARGE_INTEGER q;
    QueryPerformanceCounter(&q);
    u64 t = az_mix64(__rdtsc() ^ ((u64)q.QuadPart * 0x9E3779B97F4A7C15ull));
    u8 *b = (u8 *)buf;
    for (u32 i = 0; i < bytes; i++) {
        b[i] ^= (u8)(t >> (8u * (i & 7u)));
        if ((i & 7u) == 7u) t = az_mix64(t + i);
    }
    return true;
}

static void pool_parallel_for(void *ctx, struct AzWorld *w, AzJobFn fn, const u32 *splits, u32 n_parts)
{
    WinPool *p = (WinPool *)ctx;
    if (n_parts <= 1 || p->n_workers <= 1 || n_parts > POOL_IDX_MASK) {
        for (u32 k = 0; k < n_parts; k++) fn(w, splits[k], splits[k + 1], 0);
        return;
    }
    const u64 g = ++p->next_gen;
    PoolJob *j = &p->job[g & 1];
    j->fn = fn;
    j->world = w;
    j->splits = splits;
    __atomic_store_n(&j->n_parts, n_parts, __ATOMIC_RELAXED);
    __atomic_store_n(&p->done, 0u, __ATOMIC_RELAXED);        /* every part of g - 1 is done */
    __atomic_store_n(&p->woke_all, 0u, __ATOMIC_RELAXED);
    __atomic_store_n(&p->claim, g << POOL_IDX_BITS, __ATOMIC_RELEASE);
    __atomic_store_n(&p->gen, g, __ATOMIC_SEQ_CST);
    /* Read after the generation is posted: a worker that goes to sleep later
     * sees the new region instead. One wake here; that worker wakes the rest. */
    if (__atomic_load_n(&p->sleepers, __ATOMIC_SEQ_CST)) WakeByAddressSingle((PVOID)&p->gen);
    pool_drain(p, g, 0);
    /* parts still running on other threads */
    for (u32 spins = 1; __atomic_load_n(&p->done, __ATOMIC_ACQUIRE) < n_parts; spins++) {
        if (spins & 1023u) _mm_pause();
        else SwitchToThread();
    }
}

static u64 pool_now_ns(void *ctx)
{
    WinPool *p = (WinPool *)ctx;
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return (u64)((unsigned __int128)t.QuadPart * 1000000000ull / (u64)p->qpf.QuadPart);
}

AzHost *az_win_host_create(u32 n_workers, u32 flags, AzWinInfo *info)
{
    az_win_topology(info);
    info->workers_asked = n_workers;
    info->workers = 0;
    info->pool_note[0] = 0;
    WinPool *p = (WinPool *)_aligned_malloc(sizeof(WinPool), 64);
    if (!p) return NULL;
    memset(p, 0, sizeof(*p));
    QueryPerformanceFrequency(&p->qpf);
    p->spin_ticks = (u64)p->qpf.QuadPart * AZ_WIN_SPIN_US / 1000000u;

    const bool ecores = (flags & AZ_WIN_USE_ECORES) != 0;
    u32 avail = info->n_pcores + (ecores ? info->n_ecores : 0);
    if (avail == 0) { avail = 1; info->pcore_lp[0] = 0; info->n_pcores = 1; }
    if (n_workers == 0) n_workers = info->n_pcores ? info->n_pcores : 1;
    if (avail > AZ_MAX_WORKERS) avail = AZ_MAX_WORKERS;
    if (n_workers > avail) {             /* say so (finding v14) */
        snprintf(info->pool_note, sizeof info->pool_note, "lowered from %u to %u: %s", n_workers, avail,
                 ecores ? "no more cores" : "P cores only; E cores need --ecores");
        n_workers = avail;
    }
    p->soft = (flags & AZ_WIN_NO_PIN) ? 0 : 1;
    for (u32 k = 0; k < n_workers; k++)
        p->lp[k] = k < info->n_pcores ? info->pcore_lp[k] : info->ecore_lp[k - info->n_pcores];

    p->host.ctx = p;
    p->host.parallel_for = pool_parallel_for;
    p->host.now_ns = pool_now_ns;
    p->host.sink = NULL;        /* a ledger sink may be attached later: sink + sink_ctx */
    p->host.sink_ctx = NULL;
    p->host.entropy = az_win_entropy;
    p->host.entropy_log = NULL; /* an entropy journal may be attached later */

    /* the calling thread is worker 0; destroy gives back its ideal processor */
    if (p->soft) {
        p->caller = OpenThread(THREAD_SET_INFORMATION | THREAD_QUERY_INFORMATION, FALSE, GetCurrentThreadId());
        if (p->caller && place_thread(p, p->caller, 0, &p->caller_ideal)) p->caller_saved = 1;
    }
    u32 made = 1;
    DWORD err = 0;
    for (u32 k = 1; k < n_workers; k++) {
        WorkerArg *wa = (WorkerArg *)malloc(sizeof(WorkerArg));
        if (!wa) { err = ERROR_NOT_ENOUGH_MEMORY; break; }
        wa->pool = p;
        wa->index = k;
        HANDLE th = CreateThread(NULL, 1 << 20, worker_main, wa, 0, NULL);
        if (!th) { err = GetLastError(); free(wa); break; }
        p->threads[k] = th;
        made = k + 1;
    }
    if (made < n_workers) {              /* finding v17: run with what started */
        snprintf(info->pool_note, sizeof info->pool_note, "only %u of %u worker threads started (error %lu)",
                 made, n_workers, (unsigned long)err);
        n_workers = made;
    }
    p->n_workers = n_workers;
    p->host.n_workers = n_workers;
    p->host.max_parts = n_workers > 1 ? AZ_MAX_PARTS : 0;
    info->workers = n_workers;
    return &p->host;
}

void az_win_host_destroy(AzHost *h)
{
    if (!h) return;
    WinPool *p = (WinPool *)h->ctx;
    __atomic_store_n(&p->quit, 1u, __ATOMIC_SEQ_CST);
    __atomic_fetch_add(&p->gen, 1u, __ATOMIC_SEQ_CST);      /* so no WaitOnAddress keeps sleeping */
    WakeByAddressAll((PVOID)&p->gen);
    for (u32 k = 1; k < p->n_workers; k++) {
        if (p->threads[k]) { WaitForSingleObject(p->threads[k], INFINITE); CloseHandle(p->threads[k]); }
    }
    if (p->caller) {
        if (p->caller_saved) SetThreadIdealProcessorEx(p->caller, &p->caller_ideal, NULL);
        CloseHandle(p->caller);
    }
    _aligned_free(p);
}
