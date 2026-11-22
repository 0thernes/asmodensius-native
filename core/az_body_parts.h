/* Mechanical and sensory summaries for one traversal of the variable body. */
#ifndef AZ_BODY_PARTS_H
#define AZ_BODY_PARTS_H
#include "az_abi.h"

typedef struct AzBodyParts {
    u64 sensor_matter;
    u32 count;
    u8 fin;
} AzBodyParts;

/* Divide the 72-bit weighted sum by total matter. Since health <= 255 the
 * quotient fits in a byte. Common bodies need just one native u64 divide. */
AZ_INLINE u8 az_fin_fraction(u64 high, u64 low, u64 total)
{
    if (!total) return 0;
    if (!high) return (u8)(low / total);
    u32 value = 0;
    for (u32 bit = 128; bit; bit >>= 1) {
        const u32 trial = value | bit;
        const u64 lower = (u64)(u32)total * trial;
        const u64 upper = (total >> 32) * trial + (lower >> 32);
        const u64 prod_hi = upper >> 32;
        const u64 prod_lo = (upper << 32) | (u32)lower;
        if (prod_hi < high || (prod_hi == high && prod_lo <= low)) value = trial;
    }
    return (u8)value;
}

AZ_INLINE AzBodyParts az_body_parts(const AzWorld *w, u32 slot)
{
    AzBodyParts part = {0};
    const AzPart *p = az_parts(w, slot, &part.count);
    u64 total = 0, high = 0, low = 0;
    for (u32 k = 0; k < part.count; ++k) {
        total += p[k].matter;
        if (p[k].kind == AZ_PART_FIN) {
            const u64 before = low;
            low += (u64)p[k].matter * p[k].health;
            high += low < before;
        } else if (p[k].kind == AZ_PART_SENSOR) {
            part.sensor_matter += (u64)p[k].matter * p[k].health / 255u;
        }
    }
    part.fin = az_fin_fraction(high, low, total);
    return part;
}
#endif
