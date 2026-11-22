/* Qualification observer. Refuse before constructing any unlocked world. */
#include "az_abi.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <psapi.h>

AzWorld *__real_az_world_create(AzArena *, const AzConfig *, AzHost *);
AzWorld *__wrap_az_world_create(AzArena *, const AzConfig *, AzHost *);

static unsigned long long worlds, pages_checked;
static char receipt[4096];
static void report(void)
{
    FILE *f = fopen(receipt, "wb");
    if (!f) { fprintf(stderr, "cannot write arena guard receipt\n"); _Exit(82); }
    if (fprintf(f, "worlds=%llu pages_checked=%llu locked=1\n", worlds, pages_checked) < 0 || fclose(f)) {
        fputs("incomplete arena guard receipt\n", stderr);
        _Exit(82);
    }
}
AzWorld *__wrap_az_world_create(AzArena *arena, const AzConfig *cfg, AzHost *host)
{
    /* Intentionally tiny or misaligned failure fixtures cannot create a world. */
    if (arena && arena->base && ((uintptr_t)arena->base % 4096u) == 0u &&
        arena->used <= arena->size && arena->size - arena->used >= sizeof(AzWorld)) {
        if (!worlds) {
            const char *dir=getenv("AZ_QUAL_LOCK_RECEIPTS");
            if (!dir) { fputs("missing qualification output directory\n", stderr); _Exit(82); }
            FILETIME created, exit_time, kernel, user;
            if (!GetProcessTimes(GetCurrentProcess(), &created, &exit_time, &kernel, &user)) _Exit(82);
            snprintf(receipt, sizeof receipt, "%s/%lu-%lu-%lu.txt", dir,
                GetCurrentProcessId(), created.dwHighDateTime, created.dwLowDateTime);
            if (atexit(report) != 0) _Exit(82);
        }
        SYSTEM_INFO info;
        GetSystemInfo(&info);
        const u64 total = (arena->size + info.dwPageSize - 1u) / info.dwPageSize;
        PSAPI_WORKING_SET_EX_INFORMATION ws[512];
        for (u64 first=0; first < total;) {
            const u32 n=(u32)(total-first < 512u ? total-first : 512u);
            for (u32 i=0; i<n; i++) {
                ws[i].VirtualAddress = arena->base + (first+i)*(u64)info.dwPageSize;
                ws[i].VirtualAttributes.Flags=0;
            }
            if (!QueryWorkingSetEx(GetCurrentProcess(), ws, (DWORD)(n*sizeof(ws[0])))) _Exit(82);
            for (u32 i=0; i<n; i++) {
                if (!ws[i].VirtualAttributes.Valid ||
                    !(ws[i].VirtualAttributes.Locked || ws[i].VirtualAttributes.LargePage)) {
                    fprintf(stderr,"world arena is not resident and locked (page %llu of %llu, bytes %llu)\n",
                        first+i,total,(unsigned long long)arena->size);
                    _Exit(81);
                }
            }
            first+=n;
        }
        ++worlds;
        pages_checked+=total;
    }
    return __real_az_world_create(arena,cfg,host);
}
