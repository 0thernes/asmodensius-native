/* test_host_win.c - checks of the Windows host layer (platform/win).
 *
 * Arena (review finding v48)
 *   A1. With AZ_WIN_LOCK, az_win_arena never hands out pageable memory: it
 *       returns true only with arena_locked = 1.
 *   A2. In a child process that has removed every privilege from its token
 *       (so it cannot raise its working set and VirtualLock fails for real,
 *       error 1453), az_win_arena(AZ_WIN_LOCK) returns false, frees the
 *       memory and says why; with AZ_WIN_LOCK | AZ_WIN_ALLOW_PAGEABLE it
 *       returns true with arena_locked = 0; without AZ_WIN_LOCK it returns
 *       true.
 *   A3. In the same child, az_run refuses to run on pageable memory (exit
 *       code 3, a message on stderr even with --quiet). Standalone diagnostics
 *       also check both explicit pageable override flags and their warnings.
 * Pool (findings v65, v66, v12; also v19, v17 and s3)
 *   P1. max_parts is above n_workers, so the core cuts about eight parts
 *       per worker; a one-worker host asks for no split.
 *   P2. Every part of a region runs exactly once, with a worker index below
 *       n_workers that no other running part holds at the same moment.
 *   P3. Workers and the calling thread keep the process affinity (no hard
 *       pin to one logical processor) and each has its ideal processor;
 *       destroy gives the calling thread back its ideal processor, and its
 *       affinity and priority are as they were before the pool. The threads
 *       are found by a region whose parts wait for each other, posted while
 *       every worker sleeps: all sleepers must be woken although the caller
 *       is held up inside its first part.
 *   P4. The calling thread drains parts itself: with every worker thread
 *       suspended (a descheduled worker), regions of 2 and of many parts
 *       still finish, all of them on the calling thread.
 *   P5. 20,000 back-to-back regions of random size, two job functions and
 *       four split tables in turn, while another thread suspends and
 *       resumes workers at random (late workers): every part runs once,
 *       with its own region's function and splits.
 *   P6. Hosts are created and destroyed repeatedly.
 *   P7. Information only: the cost of an empty region after a serial gap.
 *
 * Usage: test_host_win               (suite; starts a child for A2 and A3)
 *        test_host_win --diagnostics (A2/A3 including explicit pageable overrides)
 */
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#include <immintrin.h>
#include "az_test.h"

#define MIB (1024ull * 1024ull)

static double now_us(void)
{
    static LARGE_INTEGER f;
    if (!f.QuadPart) QueryPerformanceFrequency(&f);
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return (double)t.QuadPart * 1e6 / (double)f.QuadPart;
}

static void busy_us(double us)
{
    double t0 = now_us();
    while (now_us() - t0 < us) _mm_pause();
}

/* ====================================================================== */
/* running a command with its output captured                              */
/* ====================================================================== */
typedef struct RunOut {
    DWORD code;
    bool started, timed_out;
    char out[16384], err[16384];
    u32 n_out, n_err;
} RunOut;

static void pipe_take(HANDLE h, char *buf, u32 *n, u32 cap, bool *open)
{
    DWORD avail = 0;
    if (!*open) return;
    if (!PeekNamedPipe(h, NULL, 0, NULL, &avail, NULL)) { *open = false; return; }
    while (avail) {
        char tmp[4096];
        DWORD want = avail < sizeof tmp ? avail : (DWORD)sizeof tmp, got = 0;
        if (!ReadFile(h, tmp, want, &got, NULL) || got == 0) { *open = false; return; }
        for (DWORD i = 0; i < got && *n + 1 < cap; i++) buf[(*n)++] = tmp[i];
        buf[*n] = 0;
        avail -= got;
    }
}

static bool run_capture(const char *cmdline, DWORD limit_ms, RunOut *r)
{
    memset(r, 0, sizeof *r);
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    HANDLE out_r, out_w, err_r, err_w;
    if (!CreatePipe(&out_r, &out_w, &sa, 1 << 16)) return false;
    if (!CreatePipe(&err_r, &err_w, &sa, 1 << 16)) { CloseHandle(out_r); CloseHandle(out_w); return false; }
    SetHandleInformation(out_r, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(err_r, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOA si;
    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = out_w;
    si.hStdError = err_w;
    PROCESS_INFORMATION pi;
    char cmd[2048];
    snprintf(cmd, sizeof cmd, "%s", cmdline);
    BOOL ok = CreateProcessA(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi);
    CloseHandle(out_w);
    CloseHandle(err_w);
    if (!ok) { CloseHandle(out_r); CloseHandle(err_r); return false; }
    r->started = true;
    bool o_open = true, e_open = true, exited = false;
    double t0 = now_us();
    while (o_open || e_open || !exited) {
        pipe_take(out_r, r->out, &r->n_out, sizeof r->out, &o_open);
        pipe_take(err_r, r->err, &r->n_err, sizeof r->err, &e_open);
        if (!exited && WaitForSingleObject(pi.hProcess, 5) == WAIT_OBJECT_0) exited = true;
        if (now_us() - t0 > (double)limit_ms * 1e3) {
            if (!exited) TerminateProcess(pi.hProcess, 99);
            r->timed_out = true;
            WaitForSingleObject(pi.hProcess, 5000);
            break;
        }
    }
    GetExitCodeProcess(pi.hProcess, &r->code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    CloseHandle(out_r);
    CloseHandle(err_r);
    return true;
}

static void exe_dir(char *dir, size_t n)
{
    DWORD len = GetModuleFileNameA(NULL, dir, (DWORD)n);
    if (len == 0 || len >= n) { dir[0] = 0; return; }
    char *cut = dir;
    for (char *p = dir; *p; p++) if (*p == '\\' || *p == '/') cut = p + 1;
    *cut = 0;
}

/* ====================================================================== */
/* A. arena                                                                */
/* ====================================================================== */

/* Remove every privilege except SeChangeNotifyPrivilege from this process's
 * token for good (what CreateRestrictedToken's DISABLE_MAX_PRIVILEGE does to
 * a new token). Children made with CreateProcess inherit the reduced token. */
static bool drop_privileges(void)
{
    HANDLE tok;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &tok)) return false;
    DWORD len = 0;
    GetTokenInformation(tok, TokenPrivileges, NULL, 0, &len);
    TOKEN_PRIVILEGES *have = (TOKEN_PRIVILEGES *)malloc(len ? len : 1);
    TOKEN_PRIVILEGES *drop = (TOKEN_PRIVILEGES *)malloc(len ? len : 1);
    bool ok = have && drop && GetTokenInformation(tok, TokenPrivileges, have, len, &len);
    if (ok) {
        LUID keep;
        LookupPrivilegeValueA(NULL, "SeChangeNotifyPrivilege", &keep);
        drop->PrivilegeCount = 0;
        for (DWORD i = 0; i < have->PrivilegeCount; i++) {
            LUID l = have->Privileges[i].Luid;
            if (l.LowPart == keep.LowPart && l.HighPart == keep.HighPart) continue;
            drop->Privileges[drop->PrivilegeCount].Luid = l;
            drop->Privileges[drop->PrivilegeCount].Attributes = SE_PRIVILEGE_REMOVED;
            drop->PrivilegeCount++;
        }
        ok = drop->PrivilegeCount == 0 ||
             (AdjustTokenPrivileges(tok, FALSE, drop, 0, NULL, NULL) && GetLastError() == ERROR_SUCCESS);
        printf("  removed %lu of %lu privileges from the child's token\n",
               (unsigned long)drop->PrivilegeCount, (unsigned long)have->PrivilegeCount);
    }
    free(have);
    free(drop);
    CloseHandle(tok);
    return ok;
}

static void show_arena(const char *what, bool ok, const AzArena *a, const AzWinInfo *info)
{
    printf("  %-44s -> %s, base %s, locked=%u, last_error=%u, note '%s'\n", what, ok ? "true" : "false",
           a->base ? "set" : "NULL", info->arena_locked, info->last_error, info->note);
}

/* A1: the calling process keeps its privileges; whatever happens, a locked
 * request never comes back pageable. */
static void test_arena_unrestricted(void)
{
    printf("A1. arena, unrestricted process\n");
    const u32 flags[2] = { AZ_WIN_LOCK, AZ_WIN_LOCK | AZ_WIN_LARGE_PAGES };
    const char *name[2] = { "az_win_arena(256 MB, LOCK)", "az_win_arena(256 MB, LOCK | LARGE_PAGES)" };
    for (int i = 0; i < 2; i++) {
        AzArena a;
        AzWinInfo info;
        memset(&info, 0, sizeof info);
        bool ok = az_win_arena(&a, 256 * MIB, flags[i], &info);
        show_arena(name[i], ok, &a, &info);
        CHECK(!ok || info.arena_locked == 1);
        CHECK(ok || a.base == NULL);
        if (ok) az_win_arena_free(&a);
    }
}

/* Read the OS lock bit of every arena page, including unused tail pages. */
static void check_world_pages(const AzArena *a)
{
    SYSTEM_INFO sys;
    GetSystemInfo(&sys);
    u64 pages = (a->size + sys.dwPageSize - 1) / sys.dwPageSize;
    u64 locked = 0;
    PSAPI_WORKING_SET_EX_INFORMATION rows[128];
    for (u64 first = 0; first < pages; first += AZ_LEN(rows)) {
        u32 n = (u32)((pages - first < AZ_LEN(rows)) ? pages - first : AZ_LEN(rows));
        for (u32 j = 0; j < n; j++) rows[j].VirtualAddress = a->base + (first + j) * sys.dwPageSize;
        BOOL ok = QueryWorkingSetEx(GetCurrentProcess(), rows, (DWORD)(n * sizeof rows[0]));
        CHECK(ok);
        if (!ok) return;
        for (u32 j = 0; j < n; j++)
            locked += rows[j].VirtualAttributes.Valid && rows[j].VirtualAttributes.Locked;
    }
    printf("  AzTestWorld: %" PRIu64 "/%" PRIu64 " pages valid and locked\n", locked, pages);
    CHECK(pages && locked == pages);
}

static void test_world_residency(void)
{
    AzConfig cfg = az_test_config(7, 2);
    /* The snapshot continuation test owns a source and five destinations.
     * Each exceeds the host's 64 MiB of slack for an ordinary single world. */
    cfg.cap = 24000;
    AzTestWorld worlds[6];
    void *bases[AZ_LEN(worlds)];
    MEMORY_BASIC_INFORMATION region;
    for (u32 i = 0; i < AZ_LEN(worlds); i++) {
        bool ok = i == 0 ? az_test_world(&worlds[i], &cfg, 0) :
            az_test_world_ex(&worlds[i], &cfg, 2, AZ_TEST_NO_WORLDGEN | (i == 1 ? AZ_TEST_UNPINNED : 0));
        CHECK(ok);
        bases[i] = worlds[i].arena.base;
        if (ok) {
            CHECK(worlds[i].info.arena_locked && (worlds[i].host != NULL) == (i != 0));
            CHECK_EQ(worlds[i].w->n_slots, i == 0 ? cfg.n_initial : 0);
        }
    }
    /* Check every mapping with all successful fixtures still live. */
    for (u32 i = 0; i < AZ_LEN(worlds); i++)
        if (bases[i]) check_world_pages(&worlds[i].arena);
    for (u32 i = AZ_LEN(worlds); i-- > 0;) {
        az_test_world_free(&worlds[i]);
        if (bases[i]) CHECK(VirtualQuery(bases[i], &region, sizeof region) && region.State == MEM_FREE);
    }

    /* Valid size and residency, then world_create refuses a wild world without entropy. */
    cfg.mode = AZ_MODE_WILD;
    AzTestWorld a;
    CHECK(!az_test_world(&a, &cfg, 0));
    CHECK(!a.w && !a.host && !a.arena.base && !a.arena.size);
    az_test_world_free(&a);
}

static bool has(const char *s, const char *what) { return strstr(s, what) != NULL; }

/* A3: one az_run invocation under the reduced token */
static void check_az_run(const char *az_run, const char *args, DWORD want_code, bool want_hash)
{
    char cmd[1200];
    snprintf(cmd, sizeof cmd, "\"%s\" --lab --n 2000 --ticks 2 --threads 2 %s", az_run, args);
    RunOut r;
    bool started = run_capture(cmd, 60000, &r);
    CHECK(started);
    if (!started) { printf("  could not start %s\n", az_run); return; }
    const char *e = r.err;
    printf("  az_run %-28s -> exit %lu%s; stderr: %s", args, (unsigned long)r.code,
           r.timed_out ? " (TIMED OUT)" : "", r.n_err ? e : "(empty)\n");
    if (r.n_err && e[r.n_err - 1] != '\n') printf("\n");
    CHECK(!r.timed_out);
    CHECK_EQ(r.code, want_code);
    CHECK(has(r.out, "final hash") == want_hash);
    if (want_code == 3) CHECK(has(e, "refusing") && has(e, "pageable"));
    else CHECK(has(e, "pageable"));     /* the warning is printed even with --quiet */
}

/* A2 and A3, run in a child process that gave up its privileges */
static int arena_child(bool diagnostics)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("A2. arena, child process without privileges\n");
    CHECK(drop_privileges());

    /* precondition: the lock must now really fail, or nothing below is shown */
    const SIZE_T raw_sz = (SIZE_T)(64 * MIB);
    void *raw = VirtualAlloc(NULL, raw_sz, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    BOOL raw_ok = raw ? VirtualLock(raw, raw_sz) : FALSE;
    DWORD raw_err = GetLastError();
    printf("  precondition: VirtualLock(64 MB) without privileges -> %s (error %lu)\n",
           raw_ok ? "LOCKED" : "fails", (unsigned long)(raw_ok ? 0 : raw_err));
    CHECK(raw && !raw_ok);
    if (raw) VirtualFree(raw, 0, MEM_RELEASE);

    AzConfig cfg = az_test_config(7, 2);
    cfg.cap = 10000;
    AzTestWorld world;
    bool made = az_test_world_ex(&world, &cfg, 2, AZ_TEST_UNPINNED);
    show_arena("AzTestWorld, unpinned worker placement", made, &world.arena, &world.info);
    CHECK(!made && world.info.arena_refused && world.info.last_error);
    CHECK(!world.w && !world.host && !world.arena.base && !world.arena.size);
    az_test_world_free(&world);

    AzArena a;
    AzWinInfo info;
    memset(&info, 0, sizeof info);
    bool ok = az_win_arena(&a, 256 * MIB, AZ_WIN_LOCK, &info);
    show_arena("az_win_arena(256 MB, LOCK)", ok, &a, &info);
    CHECK(!ok);
    CHECK(a.base == NULL && a.size == 0);
    CHECK(info.arena_locked == 0 && info.last_error != 0);
    CHECK(has(info.note, "VirtualLock"));
    if (ok) az_win_arena_free(&a);

    memset(&info, 0, sizeof info);
    ok = az_win_arena(&a, 256 * MIB, AZ_WIN_LOCK | AZ_WIN_ALLOW_PAGEABLE, &info);
    show_arena("az_win_arena(256 MB, LOCK | ALLOW_PAGEABLE)", ok, &a, &info);
    CHECK(ok && a.base != NULL && info.arena_locked == 0 && info.note[0] != 0);
    if (ok) az_win_arena_free(&a);

    memset(&info, 0, sizeof info);
    ok = az_win_arena(&a, 256 * MIB, 0, &info);
    show_arena("az_win_arena(256 MB, 0)", ok, &a, &info);
    CHECK(ok && a.base != NULL && info.arena_locked == 0);
    if (ok) az_win_arena_free(&a);

    printf("A3. az_run in the same child\n");
    char dir[1024], az_run[1100];
    exe_dir(dir, sizeof dir);
    snprintf(az_run, sizeof az_run, "%saz_run.exe", dir);
    if (GetFileAttributesA(az_run) == INVALID_FILE_ATTRIBUTES) {
        printf("  note: %s not found (a build without tools); A3 skipped\n", az_run);
    } else {
        check_az_run(az_run, "", 3, false);
        check_az_run(az_run, "--quiet", 3, false);
        if (diagnostics) {
            check_az_run(az_run, "--allow-pageable", 0, true);
            check_az_run(az_run, "--allow-pageable --quiet", 0, true);
            check_az_run(az_run, "--no-lock", 0, true);
        }
    }
    return az_test_done();
}

static void test_arena_child(void)
{
    char self[1024], cmd[1100];
    DWORD len = GetModuleFileNameA(NULL, self, sizeof self);
    CHECK(len > 0 && len < sizeof self);
    snprintf(cmd, sizeof cmd, "\"%s\" --arena-child", self);
    static RunOut r;
    bool started = run_capture(cmd, 120000, &r);
    CHECK(started);
    if (!started) return;
    /* relay the child's report, indented */
    const char *s = r.out;
    while (*s) {
        const char *e = strchr(s, '\n');
        int n = e ? (int)(e - s) : (int)strlen(s);
        printf("  | %.*s\n", n, s);
        s += n + (e ? 1 : 0);
    }
    if (r.n_err) printf("  | [stderr] %s\n", r.err);
    CHECK(!r.timed_out);
    CHECK_EQ(r.code, 0);
}

/* ====================================================================== */
/* P. pool                                                                 */
/* ====================================================================== */
#define TABS 4
static const u32 tab_stride[TABS] = { 3, 5, 7, 11 };
static u32 tab[TABS][AZ_MAX_PARTS + 1];

static volatile LONG g_cnt[AZ_MAX_PARTS], g_tag[AZ_MAX_PARTS], g_widx[AZ_MAX_PARTS];
static volatile DWORD g_tid[AZ_MAX_PARTS];
static volatile LONG g_busy[AZ_MAX_WORKERS];
static volatile LONG g_bad_worker, g_dup_worker, g_bad_split;
static u32 g_nw;

static void part_common(u32 b, u32 e, u32 t, LONG fn_id)
{
    if (t >= g_nw || t >= AZ_MAX_WORKERS) { InterlockedIncrement(&g_bad_worker); return; }
    if (InterlockedExchange(&g_busy[t], 1) != 0) InterlockedIncrement(&g_dup_worker);
    u32 stride = e - b, ti = TABS;
    for (u32 i = 0; i < TABS; i++) if (tab_stride[i] == stride) ti = i;
    if (ti == TABS || b % stride) {
        InterlockedIncrement(&g_bad_split);
    } else {
        u32 k = b / stride;
        if (k < AZ_MAX_PARTS) {
            InterlockedIncrement(&g_cnt[k]);
            g_tag[k] = (fn_id << 8) | (LONG)stride;
            g_widx[k] = (LONG)t;
            g_tid[k] = GetCurrentThreadId();
        }
        for (volatile u32 i = 0; i < (k & 7u) * 16u; i++) { }   /* uneven parts */
    }
    InterlockedExchange(&g_busy[t], 0);
}
static void job_a(struct AzWorld *w, u32 b, u32 e, u32 t) { (void)w; part_common(b, e, t, 0xA); }
static void job_b(struct AzWorld *w, u32 b, u32 e, u32 t) { (void)w; part_common(b, e, t, 0xB); }
static void job_empty(struct AzWorld *w, u32 b, u32 e, u32 t) { (void)w; (void)b; (void)e; (void)t; }

static void clear_records(u32 n)
{
    for (u32 k = 0; k < n && k < AZ_MAX_PARTS; k++) { g_cnt[k] = 0; g_tag[k] = 0; g_widx[k] = -1; g_tid[k] = 0; }
}

/* parts 0..n_parts-1 ran once each with the expected tag; parts n_parts..upto-1 never */
static bool records_ok(u32 n_parts, u32 upto, LONG tag)
{
    for (u32 k = 0; k < upto && k < AZ_MAX_PARTS; k++) {
        if (k < n_parts && (g_cnt[k] != 1 || g_tag[k] != tag)) return false;
        if (k >= n_parts && g_cnt[k] != 0) return false;
    }
    return true;
}

/* --- finding the pool's threads: every part waits until all have arrived,
 * so each thread holds exactly one part --- */
static volatile LONG g_arrived;
static LONG g_expect;
static void job_checkin(struct AzWorld *w, u32 b, u32 e, u32 t)
{
    (void)w; (void)e;
    InterlockedIncrement(&g_arrived);
    g_tid[b] = GetCurrentThreadId();
    g_widx[b] = (LONG)t;
    double t0 = now_us();
    while (g_arrived < g_expect && now_us() - t0 < 5e6) _mm_pause();
}

typedef struct PoolThreads {
    u32 n;                       /* threads found, the caller included */
    DWORD tid[AZ_MAX_WORKERS];   /* by worker index */
    HANDLE h[AZ_MAX_WORKERS];    /* by worker index; [0] is the caller's own */
} PoolThreads;

static void find_threads(AzHost *h, PoolThreads *pt)
{
    memset(pt, 0, sizeof *pt);
    u32 n = h->n_workers;
    u32 sp[AZ_MAX_WORKERS + 1];
    for (u32 k = 0; k <= n; k++) sp[k] = k;
    clear_records(n);
    g_arrived = 0;
    g_expect = (LONG)n;
    h->parallel_for(h->ctx, NULL, job_checkin, sp, n);
    for (u32 k = 0; k < n; k++) {
        LONG t = g_widx[k];
        if (t < 0 || (u32)t >= n || pt->tid[t]) continue;   /* duplicates are counted below */
        pt->tid[t] = g_tid[k];
        pt->h[t] = OpenThread(THREAD_SUSPEND_RESUME | THREAD_QUERY_INFORMATION, FALSE, g_tid[k]);
        pt->n++;
    }
}

static void close_threads(PoolThreads *pt)
{
    for (u32 t = 0; t < AZ_MAX_WORKERS; t++) if (pt->h[t]) CloseHandle(pt->h[t]);
    memset(pt, 0, sizeof *pt);
}

static u32 expected_lp(const AzWinInfo *info, u32 t)
{
    return t < info->n_pcores ? info->pcore_lp[t] : info->ecore_lp[t - info->n_pcores];
}

static int bits(u64 m) { return __builtin_popcountll(m); }

/* --- P4: a region run by another thread while the workers are suspended --- */
typedef struct Drive {
    AzHost *h;
    AzJobFn fn;
    const u32 *sp;
    u32 n_parts;
    DWORD tid;
} Drive;

static DWORD WINAPI drive_main(LPVOID arg)
{
    Drive *d = (Drive *)arg;
    d->tid = GetCurrentThreadId();
    d->h->parallel_for(d->h->ctx, NULL, d->fn, d->sp, d->n_parts);
    return 0;
}

static void test_suspended(AzHost *h, PoolThreads *pt, u32 many)
{
    for (u32 t = 1; t < pt->n; t++) if (pt->h[t]) SuspendThread(pt->h[t]);
    const u32 sizes[2] = { 2, many };
    for (int i = 0; i < 2; i++) {
        u32 n_parts = sizes[i];
        clear_records(AZ_MAX_PARTS);
        Drive d = { h, job_a, tab[0], n_parts, 0 };
        HANDLE th = CreateThread(NULL, 1 << 20, drive_main, &d, 0, NULL);
        CHECK(th != NULL);
        if (!th) continue;
        double t0 = now_us();
        bool in_time = WaitForSingleObject(th, 5000) == WAIT_OBJECT_0;
        double ms = (now_us() - t0) / 1e3;
        if (!in_time) {                     /* let the old pool finish, then fail */
            for (u32 t = 1; t < pt->n; t++) if (pt->h[t]) ResumeThread(pt->h[t]);
            WaitForSingleObject(th, 20000);
            for (u32 t = 1; t < pt->n; t++) if (pt->h[t]) SuspendThread(pt->h[t]);
        }
        CloseHandle(th);
        u32 on_caller = 0;
        for (u32 k = 0; k < n_parts; k++) if (g_widx[k] == 0 && g_tid[k] == d.tid) on_caller++;
        printf("  every worker suspended, %4u parts: %s in %.2f ms; %u of %u parts ran on the calling thread\n",
               n_parts, in_time ? "finished" : "STALLED (more than 5 s)", ms, on_caller, n_parts);
        CHECK(in_time);
        CHECK(records_ok(n_parts, AZ_MAX_PARTS, (0xA << 8) | 3));
        CHECK_EQ(on_caller, n_parts);
    }
    for (u32 t = 1; t < pt->n; t++) if (pt->h[t]) ResumeThread(pt->h[t]);
}

/* --- P5: a thread that suspends and resumes workers at random --- */
static volatile LONG g_chaos_stop, g_chaos_count;
static PoolThreads *g_chaos_pt;
static DWORD WINAPI chaos_main(LPVOID arg)
{
    (void)arg;
    u64 s = 0x2545F4914F6CDD1Dull;
    const u32 nw = g_chaos_pt->n;
    while (!g_chaos_stop && nw > 1) {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        u32 t = 1 + (u32)(s >> 33) % (nw - 1);
        u32 us = (u32)(s >> 13) % 300u;
        HANDLE h = g_chaos_pt->h[t];
        if (h && SuspendThread(h) != (DWORD)-1) {
            busy_us(us);
            ResumeThread(h);
            InterlockedIncrement(&g_chaos_count);
        }
        busy_us(40);
    }
    return 0;
}

static void test_stress(AzHost *h, PoolThreads *pt, u32 maxp, u32 regions)
{
    g_chaos_pt = pt;
    g_chaos_stop = 0;
    g_chaos_count = 0;
    HANDLE chaos = CreateThread(NULL, 1 << 16, chaos_main, NULL, 0, NULL);
    CHECK(chaos != NULL);
    u64 s = 0x9E3779B97F4A7C15ull;
    u32 bad = 0, first_bad = 0, total_parts = 0, r = 0;
    clear_records(AZ_MAX_PARTS);
    double t0 = now_us();
    /* at least `regions` regions, and go on until the other thread has
     * suspended workers 200 times (it may start late), at most 10 s */
    while (r < regions || (g_chaos_count < 200 && now_us() - t0 < 10e6)) {
        r++;
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        u32 n_parts = 1 + (u32)(s >> 33) % maxp;
        u32 ti = r % TABS;
        AzJobFn fn = (r & 1) ? job_b : job_a;
        LONG tag = ((r & 1) ? 0xB : 0xA) << 8 | (LONG)tab_stride[ti];
        clear_records(maxp);
        h->parallel_for(h->ctx, NULL, fn, tab[ti], n_parts);
        if (!records_ok(n_parts, maxp, tag)) { if (!bad) first_bad = r; bad++; }
        total_parts += n_parts;
    }
    double ms = (now_us() - t0) / 1e3;
    g_chaos_stop = 1;
    if (chaos) { WaitForSingleObject(chaos, 10000); CloseHandle(chaos); }
    /* a late worker of the last region would show up now */
    busy_us(2000);
    LONG late = 0;
    for (u32 k = 0; k < AZ_MAX_PARTS; k++) if (g_cnt[k] > 1) late++;
    printf("  %u regions (%u parts, up to %u per region) in %.0f ms with %ld random suspensions: "
           "%u bad regions (first %u), worker index out of range %ld, index held twice %ld, wrong splits %ld\n",
           r, total_parts, maxp, ms, g_chaos_count, bad, first_bad, g_bad_worker, g_dup_worker, g_bad_split);
    CHECK_EQ(bad, 0);
    CHECK_EQ(late, 0);
    CHECK_EQ(g_bad_worker, 0);
    CHECK_EQ(g_dup_worker, 0);
    CHECK_EQ(g_bad_split, 0);
    CHECK(g_chaos_count >= 200);
}

/* --- P7: an empty region after a serial gap (information only) --- */
static int cmp_d(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

static void report_wakeup(AzHost *h, u32 n_parts)
{
    static const double gaps[] = { 0, 20, 100, 300, 1000, 3000 };
    enum { REPS = 300 };
    static double dt[REPS];
    printf("  empty region of %u parts after a serial gap (noisy, parallel load): ", n_parts);
    for (u32 g = 0; g < sizeof gaps / sizeof gaps[0]; g++) {
        double sum = 0;
        for (int i = 0; i < REPS; i++) {
            busy_us(gaps[g]);
            double t0 = now_us();
            h->parallel_for(h->ctx, NULL, job_empty, tab[0], n_parts);
            dt[i] = now_us() - t0;
            sum += dt[i];
        }
        qsort(dt, REPS, sizeof dt[0], cmp_d);
        printf("%s%.0f us gap: mean %.1f, median %.1f, max %.0f us", g ? "; " : "", gaps[g], sum / REPS,
               dt[REPS / 2], dt[REPS - 1]);
    }
    printf("\n");
}

static void test_pool(u32 want, u32 flags, const char *label, u32 regions)
{
    AzWinInfo info;
    memset(&info, 0, sizeof info);
    HANDLE me = OpenThread(THREAD_QUERY_INFORMATION | THREAD_SET_INFORMATION, FALSE, GetCurrentThreadId());
    PROCESSOR_NUMBER ideal0, ideal1;
    GROUP_AFFINITY aff0, aff1;
    memset(&ideal0, 0, sizeof ideal0);
    memset(&ideal1, 0, sizeof ideal1);
    GetThreadIdealProcessorEx(me, &ideal0);
    GetThreadGroupAffinity(me, &aff0);
    int prio0 = GetThreadPriority(me);

    AzHost *h = az_win_host_create(want, flags, &info);
    CHECK(h != NULL);
    if (!h) { CloseHandle(me); return; }
    g_nw = h->n_workers;
    printf("P. pool, %s: asked %u, got %u workers, max_parts %u\n", label, want, h->n_workers, h->max_parts);
    if (h->n_workers < 2) {
        printf("  note: fewer than two workers on this machine; pool checks skipped\n");
        az_win_host_destroy(h);
        CloseHandle(me);
        return;
    }
    /* P1 */
    CHECK(h->max_parts > h->n_workers);
    u32 maxp = h->max_parts > h->n_workers ? h->max_parts : h->n_workers * 8u;
    if (maxp > h->n_workers * 8u) maxp = h->n_workers * 8u;
    if (maxp > AZ_MAX_PARTS) maxp = AZ_MAX_PARTS;

    /* P2: a region of every size up to maxp, each part once */
    u32 bad = 0;
    for (u32 n = 1; n <= maxp; n++) {
        clear_records(maxp);
        h->parallel_for(h->ctx, NULL, job_a, tab[0], n);
        if (!records_ok(n, maxp, (0xA << 8) | 3)) bad++;
    }
    printf("  regions of 1 to %u parts: %u with a part missing or run twice\n", maxp, bad);
    CHECK_EQ(bad, 0);
    CHECK_EQ(g_bad_worker, 0);
    CHECK_EQ(g_dup_worker, 0);

    /* P3: placement. First let every worker fall asleep (they spin about
     * 0.2 ms): the check-in region's first part blocks the caller until every
     * thread has arrived, so it finishes only if the sleepers are woken while
     * the caller is held up inside a part. */
    Sleep(20);
    PoolThreads pt;
    find_threads(h, &pt);
    CHECK_EQ(pt.n, h->n_workers);
    CHECK(pt.tid[0] == GetCurrentThreadId());
    DWORD_PTR pmask = 0, smask = 0;
    GetProcessAffinityMask(GetCurrentProcess(), &pmask, &smask);
    u32 narrow = 0, wrong_ideal = 0;
    for (u32 t = 0; t < pt.n; t++) {
        HANDLE th = t == 0 ? me : pt.h[t];
        GROUP_AFFINITY ga;
        PROCESSOR_NUMBER pn;
        memset(&ga, 0, sizeof ga);
        memset(&pn, 0, sizeof pn);
        if (!th || !GetThreadGroupAffinity(th, &ga)) { narrow++; continue; }
        if (bits(pmask) > 1 && bits(ga.Mask) <= 1) narrow++;
        if (!(flags & AZ_WIN_NO_PIN) && GetThreadIdealProcessorEx(th, &pn) && pn.Number != expected_lp(&info, t))
            wrong_ideal++;
    }
    printf("  %u threads found; %u pinned to a single logical processor (process mask has %d); "
           "%u without their ideal processor\n", pt.n, narrow, bits(pmask), wrong_ideal);
    CHECK_EQ(narrow, 0);
    if (!(flags & AZ_WIN_NO_PIN)) CHECK_EQ(wrong_ideal, 0);

    /* P4, P5 */
    test_suspended(h, &pt, maxp);
    test_stress(h, &pt, maxp, regions);
    if (!(flags & AZ_WIN_NO_PIN)) report_wakeup(h, maxp);
    close_threads(&pt);

    az_win_host_destroy(h);
    /* the calling thread is left as it was (finding s3) */
    GetThreadIdealProcessorEx(me, &ideal1);
    GetThreadGroupAffinity(me, &aff1);
    int prio1 = GetThreadPriority(me);
    printf("  after destroy, calling thread: ideal %u -> %u, affinity bits %d -> %d, priority %d -> %d\n",
           ideal0.Number, ideal1.Number, bits(aff0.Mask), bits(aff1.Mask), prio0, prio1);
    CHECK(ideal1.Number == ideal0.Number && ideal1.Group == ideal0.Group);
    CHECK(aff1.Mask == aff0.Mask);
    CHECK_EQ(prio1, prio0);
    CloseHandle(me);
}

/* P1 for one worker, P6 */
static void test_small_and_repeated(void)
{
    AzWinInfo info;
    memset(&info, 0, sizeof info);
    AzHost *h = az_win_host_create(1, 0, &info);
    CHECK(h != NULL);
    if (h) {
        CHECK_EQ(h->n_workers, 1);
        CHECK(h->max_parts <= 1);
        g_nw = 1;
        clear_records(16);
        h->parallel_for(h->ctx, NULL, job_b, tab[1], 5);
        CHECK(records_ok(5, 16, (0xB << 8) | 5));
        az_win_host_destroy(h);
    }
    double t0 = now_us();
    u32 fails = 0;
    for (u32 i = 0; i < 40; i++) {
        AzHost *p = az_win_host_create(4, (i & 1) ? AZ_WIN_NO_PIN : 0, &info);
        if (!p) { fails++; continue; }
        g_nw = p->n_workers;
        clear_records(32);
        p->parallel_for(p->ctx, NULL, job_a, tab[0], 32);
        if (!records_ok(32, 32, (0xA << 8) | 3) && p->n_workers > 1 && p->max_parts >= 32) fails++;
        if (i % 3 == 0) busy_us(500);                 /* let some workers fall asleep */
        az_win_host_destroy(p);
    }
    printf("P6. 40 hosts of 4 workers created, used and destroyed in %.0f ms; %u failed\n",
           (now_us() - t0) / 1e3, fails);
    CHECK_EQ(fails, 0);
}

/* T1 (finding v16): the SMT threads of one physical core are one core, so the
 * default worker count is the number of physical P cores and workers 0 and 1
 * never share a core. Synthetic topologies, then this machine's. */
static void topo_case(const char *label, const AzWinCpuSet *sets, u32 n, u32 want_p, u32 want_e, u32 want_smt,
                      const u32 *want_plp, const u32 *want_elp)
{
    AzWinInfo info;
    memset(&info, 0, sizeof info);
    az_win_topology_classify(&info, sets, n);
    u32 g0 = 0;
    for (u32 i = 0; i < n; i++) g0 += sets[i].group == 0;
    CHECK_EQ(info.n_logical, g0);
    CHECK_EQ(info.n_pcores, want_p);
    CHECK_EQ(info.n_ecores, want_e);
    CHECK_EQ(info.n_smt, want_smt);
    for (u32 k = 0; k < want_p && k < info.n_pcores; k++) CHECK_EQ(info.pcore_lp[k], want_plp[k]);
    for (u32 k = 0; k < want_e && k < info.n_ecores; k++) CHECK_EQ(info.ecore_lp[k], want_elp[k]);
    printf("T1. %-36s P %2u, E %2u, SMT %2u; workers 0 and 1 on LPs %u and %u\n", label, info.n_pcores,
           info.n_ecores, info.n_smt, info.pcore_lp[0], info.n_pcores > 1 ? info.pcore_lp[1] : info.pcore_lp[0]);
}

static void test_topology(void)
{
    AzWinCpuSet s[32], r[32];
    u32 plp[16], elp[16];
    /* hybrid without SMT: LPs 0-7 P (class 1), 8-23 E (class 0), one core each */
    for (u32 i = 0; i < 24; i++) s[i] = (AzWinCpuSet){ i, i, 0, (u8)(i < 8 ? 1 : 0) };
    for (u32 k = 0; k < 8; k++) plp[k] = k;
    for (u32 k = 0; k < 16; k++) elp[k] = 8 + k;
    topo_case("hybrid 8P + 16E, no SMT", s, 24, 8, 16, 0, plp, elp);
    /* SMT on the P cores: LPs 2c and 2c+1 share core 2c (8 cores, 16 threads), then 16 E */
    for (u32 i = 0; i < 16; i++) s[i] = (AzWinCpuSet){ i, i & ~1u, 0, 1 };
    for (u32 i = 16; i < 32; i++) s[i] = (AzWinCpuSet){ i, i, 0, 0 };
    for (u32 k = 0; k < 8; k++) plp[k] = 2 * k;
    for (u32 k = 0; k < 16; k++) elp[k] = 16 + k;
    topo_case("hybrid 8P (2 threads each) + 16E", s, 32, 8, 16, 8, plp, elp);
    for (u32 i = 0; i < 32; i++) r[i] = s[31 - i];        /* list order must not matter */
    topo_case("the same, list reversed", r, 32, 8, 16, 8, plp, elp);
    /* not hybrid: 4 cores x 2 threads, all class 0; a second group is ignored */
    for (u32 i = 0; i < 8; i++) s[i] = (AzWinCpuSet){ i, i & ~1u, 0, 0 };
    for (u32 i = 8; i < 16; i++) s[i] = (AzWinCpuSet){ i - 8, (i - 8) & ~1u, 1, 0 };
    for (u32 k = 0; k < 4; k++) plp[k] = 2 * k;
    topo_case("4 cores x 2 threads, plus group 1", s, 16, 4, 0, 4, plp, elp);

    AzWinInfo info;
    memset(&info, 0, sizeof info);
    az_win_topology(&info);
    CHECK(info.n_pcores >= 1);
    CHECK_EQ(info.n_pcores + info.n_ecores + info.n_smt, info.n_logical);
    printf("T1. this machine: %u logical processors, P %u, E %u, SMT %u\n", info.n_logical, info.n_pcores,
           info.n_ecores, info.n_smt);
}

int main(int argc, char **argv)
{
    if (argc > 1 && !strcmp(argv[1], "--arena-child")) return arena_child(false);
    if (argc > 1 && !strcmp(argv[1], "--diagnostics")) return arena_child(true);
    setvbuf(stdout, NULL, _IONBF, 0);
    for (u32 i = 0; i < TABS; i++)
        for (u32 k = 0; k <= AZ_MAX_PARTS; k++) tab[i][k] = tab_stride[i] * k;

    test_topology();
    test_arena_unrestricted();
    test_world_residency();
    test_arena_child();
    test_pool(8, 0, "8 workers, default placement", 20000);
    test_pool(8, AZ_WIN_NO_PIN, "8 workers, AZ_WIN_NO_PIN", 5000);
    test_pool(16, AZ_WIN_USE_ECORES, "16 workers with E cores", 10000);
    test_small_and_repeated();
    return az_test_done();
}
