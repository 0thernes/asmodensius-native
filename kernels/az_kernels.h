/* az_kernels.h - assembly twins of hot kernels and their run-time dispatch.
 * Owner: project lead (this header and az_kernels.c). Each twin lives in its
 * own kernels/<name>.asm, owned by the module owner who wrote the C reference.
 *
 * Law for every twin: bit-identical to its C reference on every input. The
 * twin's test (tests/test_<module>.c) proves it on >= 100,000 random inputs
 * plus edge cases. A twin is only installed when the CPU has the feature and
 * the caller asks for it; the C reference always remains available.
 */
#ifndef AZ_KERNELS_H
#define AZ_KERNELS_H

#include "az_abi.h"

/* CPU features (az_cpu_features) */
enum {
    AZ_CPU_AVX2      = 1u << 0,   /* with OS support for YMM state */
    AZ_CPU_FMA       = 1u << 1,
    AZ_CPU_BMI2      = 1u << 2,
    AZ_CPU_VNNI      = 1u << 3,   /* AVX-VNNI (VEX vpdpbusd)       */
    AZ_CPU_VNNI_INT8 = 1u << 4,   /* AVX-VNNI-INT8 (vpdpbssd)      */
    AZ_CPU_AVX512F   = 1u << 5,
    AZ_CPU_HYBRID_P  = 1u << 6    /* the calling thread runs on a performance core */
};
u32 az_cpu_features(void);

/* The one rule for when a twin may run on a CPU with features f (finding v15:
 * the installer also demanded AVX-VNNI-INT8, while the twin, az_brainbench and
 * test_brain asked only for what the twin executes). The VNNI brain twin runs
 * VEX vpdpbusd: AVX2 and AVX-VNNI, nothing more. */
#define AZ_TWIN_NEED_BRAIN_VNNI ((u32)AZ_CPU_AVX2 | (u32)AZ_CPU_VNNI)
#define AZ_TWIN_NEED_AVX2       ((u32)AZ_CPU_AVX2)
static inline bool az_twin_ok_brain_vnni(u32 f) { return (f & AZ_TWIN_NEED_BRAIN_VNNI) == AZ_TWIN_NEED_BRAIN_VNNI; }
static inline bool az_twin_ok_avx2(u32 f)       { return (f & AZ_TWIN_NEED_AVX2) == AZ_TWIN_NEED_AVX2; }

/* twins that may be installed (bits of AzKernels.asm_mask and of `want`) */
enum {
    AZ_K_BRAIN_VNNI     = 1u << 0,
    AZ_K_BRAIN_AVX2     = 1u << 1,
    AZ_K_PHILOX_AVX2    = 1u << 2,
    AZ_K_KIN_AVX2       = 1u << 3,
    AZ_K_ALL            = 0xFFFFFFFFu
};

/* Install every requested twin this CPU supports. Returns the installed mask.
 * want = 0 restores the C references. Call before the first tick. */
u32 az_kernels_install(AzWorld *w, u32 want);

/* ---- twins (each defined in its kernels .asm file when present) ---- */
/* kernels/az_brain_vnni.asm : same contract as az_brain_step_c */
void az_brain_step_vnni(const AzBrain *b, u8 *mind, i8 *motor);
void az_brain_step_avx2(const AzBrain *b, u8 *mind, i8 *motor);
/* kernels/az_philox_avx2.asm : n Philox4x32-10 blocks; out and ctr hold 4*n words */
void az_philox_n_avx2(u32 *out4n, const u32 *ctr4n, const u32 key[2], u32 n);
void az_kin_axis_avx2(u32 *pos, i16 *vel, const i16 *dv, const u8 *drag, u32 n, u32 shift);

#endif
