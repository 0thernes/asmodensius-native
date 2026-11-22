#ifndef AZ_OBSERVED_ROWS_H
#define AZ_OBSERVED_ROWS_H
#include "az_abi.h"
#include <string.h>
/* Reorder a hosted slot-indexed sector by the same injective destination-to-source
   permutation as the world. Caller supplies one saved row and cap bytes of marks. */
static inline void azt_reorder_rows(const AzWorld *w, void *base, size_t width, void *saved, u8 *marks)
{
    const u32 n = w->n_live;
    u8 *rows = base;
    memset(marks, 0, n);
    for (u32 i = 0; i < n; ++i) if (w->order[i] < n) marks[w->order[i]] = 1;
    for (u32 pass = 0; pass < 2; ++pass) {
        for (u32 start = 0; start < n; ++start) {
            if (marks[start] != pass) continue;
            u32 dest = start, src = w->order[dest];
            if (src == dest) { marks[dest] = 2; continue; }
            memcpy(saved, rows + (size_t)start * width, width);
            for (;;) {
                marks[dest] = 2;
                if (src == start) { memcpy(rows + (size_t)dest * width, saved, width); break; }
                memcpy(rows + (size_t)dest * width, rows + (size_t)src * width, width);
                if (src >= n) break;
                dest = src;
                src = w->order[dest];
            }
        }
    }
}
#endif
