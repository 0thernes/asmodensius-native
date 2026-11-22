/* Integer world-to-body projection. Prepare one inverse rotor for all signals
 * of a creature's sense step. Arithmetic and final rounding match az_fx.c. */
#ifndef AZ_BODY_AXES_H
#define AZ_BODY_AXES_H
#include "az_abi.h"

typedef struct AzBodyAxes {
    i64 row[3][3];
    u64 norm2;
    bool upright;
} AzBodyAxes;

AZ_INLINE void az_prepare_axes(AzBodyAxes *axes,const i16 rotor[4])
{
    const i64 w=rotor[0], a=-(i64)rotor[1], b=-(i64)rotor[2], c=-(i64)rotor[3];
    axes->upright=(a|b|c)==0;
    if (axes->upright) {axes->norm2=(u64)(w*w); return;}
    const i64 ww=w*w, aa=a*a, bb=b*b, cc=c*c;
    const i64 ab=a*b, ac=a*c, bc=b*c, wa=w*a, wb=w*b, wc=w*c;
    axes->norm2=(u64)(ww+aa+bb+cc);
    axes->row[0][0]=ww+aa-bb-cc; axes->row[0][1]=2*(ab-wc); axes->row[0][2]=2*(ac+wb);
    axes->row[1][0]=2*(ab+wc); axes->row[1][1]=ww-aa+bb-cc; axes->row[1][2]=2*(bc-wa);
    axes->row[2][0]=2*(ac-wb); axes->row[2][1]=2*(bc+wa); axes->row[2][2]=ww-aa-bb+cc;
}

AZ_INLINE i32 az_body_axis(const AzBodyAxes *axes,const i32 v[3],u32 axis)
{
    if (axes->upright) {
        /* The general operator returns the untouched input at norm zero. A
         * nonzero rotor has symmetric saturation even for exact identity. */
        return axes->norm2 && v[axis]==INT32_MIN ? -INT32_MAX : v[axis];
    }
    const i64 *row=axes->row[axis];
    const u64 sum=(u64)row[0]*(u64)(i64)v[0]+(u64)row[1]*(u64)(i64)v[1]+(u64)row[2]*(u64)(i64)v[2];
    const bool negative=(i64)sum<0;
    const u64 magnitude=negative ? 0ull-sum : sum;
    u64 quot=(magnitude+(axes->norm2>>1))/axes->norm2;
    if (quot>INT32_MAX) quot=INT32_MAX;
    return negative ? -(i32)quot : (i32)quot;
}
#endif
