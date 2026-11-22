/* az_kernels.c - CPUID feature detection and kernel dispatch. Owner: lead.
 * AZ_HAVE_<TWIN> is defined by CMake when the twin's .asm file exists. */
#include "az_kernels.h"
#include <cpuid.h>

#ifdef AZ_HAVE_KIN_AVX2
_Static_assert(AZ_VEL_MAX == 16000, "keep az_kin_avx2.asm clamp constants in sync");
#endif

/* the brain twins address AzBrain by these offsets (kernels/az_brain_vnni.asm) */
_Static_assert(offsetof(AzBrain, n_hb) == 0 && offsetof(AzBrain, n_mb) == 2 && offsetof(AzBrain, shift1) == 12 &&
               offsetof(AzBrain, shift2) == 13 && offsetof(AzBrain, n_sg) == 14 && offsetof(AzBrain, gain) == 16 &&
               offsetof(AzBrain, bias) == 24 && offsetof(AzBrain, off_b1) == 32 && offsetof(AzBrain, off_b2) == 36 &&
               offsetof(AzBrain, off_c1) == 40 && offsetof(AzBrain, off_c2) == 44 && offsetof(AzBrain, off_in1) == 48 &&
               offsetof(AzBrain, off_in2) == 52 && offsetof(AzBrain, off_w1) == 56 && offsetof(AzBrain, off_w2) == 60,
               "AzBrain offsets are fixed for the twins");

static u64 xgetbv0(void)
{
    u32 eax, edx;
    __asm__ volatile("xgetbv" : "=a"(eax), "=d"(edx) : "c"(0));
    return ((u64)edx << 32) | eax;
}

u32 az_cpu_features(void)
{
    u32 f = 0;
    unsigned a, b, c, d;
    if (!__get_cpuid(1, &a, &b, &c, &d)) return 0;
    bool osxsave = (c >> 27) & 1;
    bool avx = (c >> 28) & 1;
    bool fma = (c >> 12) & 1;
    bool ymm_ok = osxsave && ((xgetbv0() & 6) == 6);
    if (!(avx && ymm_ok)) return 0;
    if (fma) f |= AZ_CPU_FMA;
    if (__get_cpuid_count(7, 0, &a, &b, &c, &d)) {
        if ((b >> 5) & 1) f |= AZ_CPU_AVX2;
        if ((b >> 8) & 1) f |= AZ_CPU_BMI2;
        if (((b >> 16) & 1) && ((xgetbv0() & 0xE0) == 0xE0)) f |= AZ_CPU_AVX512F;
        unsigned a1, b1, c1, d1;
        if (__get_cpuid_count(7, 1, &a1, &b1, &c1, &d1)) {
            if ((a1 >> 4) & 1) f |= AZ_CPU_VNNI;
            if ((d1 >> 4) & 1) f |= AZ_CPU_VNNI_INT8;
        }
    }
    if (__get_cpuid_count(0x1A, 0, &a, &b, &c, &d)) {
        if ((a >> 24) == 0x40) f |= AZ_CPU_HYBRID_P;
    }
    return f;
}

u32 az_kernels_install(AzWorld *w, u32 want)
{
    const u32 f = az_cpu_features();
    u32 got = 0;
    w->k.brain_step = az_brain_step_c;
    w->k.philox_n = az_philox4x32_n;     /* the C reference unless the twin below is chosen */
    w->k.kin_axis = az_kin_axis_c;
    (void)f; (void)want;

#ifdef AZ_HAVE_BRAIN_VNNI
    if ((want & AZ_K_BRAIN_VNNI) && az_twin_ok_brain_vnni(f)) {
        w->k.brain_step = az_brain_step_vnni;
        got |= AZ_K_BRAIN_VNNI;
    } else if ((want & AZ_K_BRAIN_AVX2) && az_twin_ok_avx2(f)) {
        w->k.brain_step = az_brain_step_avx2;
        got |= AZ_K_BRAIN_AVX2;
    }
#endif
#ifdef AZ_HAVE_PHILOX_AVX2
    if ((want & AZ_K_PHILOX_AVX2) && az_twin_ok_avx2(f)) {
        w->k.philox_n = az_philox_n_avx2;
        got |= AZ_K_PHILOX_AVX2;
    }
#endif
#ifdef AZ_HAVE_KIN_AVX2
    if ((want & AZ_K_KIN_AVX2) && az_twin_ok_avx2(f)) {
        w->k.kin_axis = az_kin_axis_avx2;
        got |= AZ_K_KIN_AVX2;
    }
#endif
    w->k.asm_mask = got;
    return got;
}
