/* az_host_win.h - Windows host layer for the freestanding core.
 * Provides: a page-locked arena, a worker pool with soft placement (P cores
 * first) and dynamic parts, a nanosecond clock. Hosted C; this is the only
 * place that talks to the OS. */
#ifndef AZ_HOST_WIN_H
#define AZ_HOST_WIN_H

#include "../../core/az_abi.h"

enum {
    AZ_WIN_LOCK           = 1,  /* VirtualLock the arena; failure is an error       */
    AZ_WIN_LARGE_PAGES    = 2,  /* try 2 MB pages (needs SeLockMemoryPrivilege)     */
    AZ_WIN_USE_ECORES     = 4,  /* allow E cores as workers after the P cores       */
    AZ_WIN_NO_PIN         = 8,  /* no ideal processors: placement left to the OS    */
    AZ_WIN_ALLOW_PAGEABLE = 16  /* with AZ_WIN_LOCK: accept an arena that could not
                                   be locked (the caller asked for this explicitly) */
};

typedef struct AzWinInfo {
    u32 n_logical;                    /* logical processors in group 0          */
    u32 n_pcores, n_ecores;           /* PHYSICAL cores of each kind (v16)      */
    u32 pcore_lp[64], ecore_lp[64];   /* one logical processor per physical core:
                                         the lowest-numbered thread of the core */
    u32 n_smt;                        /* further SMT threads of those cores; the
                                         pool does not use them (TR-12)         */
    u32 arena_locked;                 /* 1 if the arena is locked in RAM        */
    u32 arena_large;                  /* 1 if large pages were granted          */
    u32 arena_refused;                /* 1 if az_win_arena failed only because
                                         the lock failed (memory was freed)     */
    u32 last_error;                   /* GetLastError of the last failed step   */
    u32 workers_asked;                /* n_workers passed to az_win_host_create */
    u32 workers;                      /* workers the pool runs                  */
    char note[160];                   /* arena: what failed, in words           */
    char pool_note[96];               /* pool: why it runs fewer workers        */
} AzWinInfo;

/* Allocate the arena and, with AZ_WIN_LOCK, lock it in RAM. Returns false if
 * the allocation failed, or if AZ_WIN_LOCK was given and the arena could not
 * be locked (then nothing stays allocated, arena_refused is 1 and note says
 * why), unless AZ_WIN_ALLOW_PAGEABLE is also given: then it returns true with
 * arena_locked = 0. Without AZ_WIN_LOCK the arena is pageable by request. */
bool az_win_arena(AzArena *a, u64 bytes, u32 flags, AzWinInfo *info);
void az_win_arena_free(AzArena *a);

/* n_workers = 0 means "one per P core"; more than the allowed cores (P cores,
 * plus E cores with AZ_WIN_USE_ECORES) is lowered, and info->workers says
 * what runs. The calling thread is worker 0: it must be the only thread that
 * calls parallel_for, and it runs parts too. Workers keep the process
 * affinity; unless AZ_WIN_NO_PIN, each gets an ideal processor (a hint the
 * scheduler follows when it can). Thread priorities are not changed.
 * destroy gives the creating thread back its previous ideal processor. */
/* Fresh entropy from the operating system (AzHost.entropy; every host made
 * by az_win_host_create carries it). ctx is not used. */
bool az_win_entropy(void *ctx, void *buf, u32 bytes);

AzHost *az_win_host_create(u32 n_workers, u32 flags, AzWinInfo *info);
void    az_win_host_destroy(AzHost *h);
void    az_win_topology(AzWinInfo *info);

/* One CPU set as GetSystemCpuSetInformation reports it. CoreIndex names the
 * physical core: the SMT threads of one core share it (finding v16). */
typedef struct AzWinCpuSet { u32 lp, core, group; u8 eff_class; } AzWinCpuSet;

/* The classification behind az_win_topology, on any list (tests feed it
 * synthetic topologies). Group 0 only. The highest efficiency class of group 0
 * is "P". Each physical core is counted once, by its lowest-numbered thread,
 * whatever order the list has; its other threads count in n_smt. So the default
 * worker count is the number of physical P cores, and no two workers share a
 * core. */
void    az_win_topology_classify(AzWinInfo *info, const AzWinCpuSet *sets, u32 n);

#endif
