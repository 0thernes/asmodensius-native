/* az_gpu_layout.h - the layout facts the host and the device must agree on.
 *
 * Included by both translation units (gcc x86-64 host, clang nvptx64 device).
 * Each side evaluates AZ_GPU_LAYOUT_INIT with its own compiler; az_gpu_open
 * runs the device kernel az_gpu_k_layout and compares the two word for word.
 * A mismatch (different ABI version, struct padding, dimensions) refuses the
 * module instead of computing wrong numbers.
 */
#ifndef AZ_GPU_LAYOUT_H
#define AZ_GPU_LAYOUT_H

#include "az_abi.h"
#include "az_dense.h"

#define AZ_GPU_LAYOUT_WORDS 36u
#define AZ_GPU_LAYOUT_INIT {                                   \
    0x415A4750u,                 /* 'AZGP' magic            */ \
    AZ_ABI_VERSION,                                            \
    (u32)sizeof(AzDenseNet),                                   \
    (u32)__builtin_offsetof(AzDenseNet, w1),                   \
    (u32)__builtin_offsetof(AzDenseNet, w2),                   \
    (u32)__builtin_offsetof(AzDenseNet, b1),                   \
    (u32)__builtin_offsetof(AzDenseNet, b2),                   \
    (u32)__builtin_offsetof(AzDenseNet, shift1),               \
    (u32)__builtin_offsetof(AzDenseNet, shift2),               \
    AZ_DENSE_SENS, AZ_DENSE_HID, AZ_DENSE_MOT, AZ_DENSE_MODG,  \
    AZ_W1_LEN, AZ_W2_LEN,                                      \
    (u32)sizeof(void *),                                       \
    (u32)sizeof(AzBrain),                                      \
    (u32)__builtin_offsetof(AzBrain, n_hb),                    \
    (u32)__builtin_offsetof(AzBrain, n_mb),                    \
    (u32)__builtin_offsetof(AzBrain, n_t1),                    \
    (u32)__builtin_offsetof(AzBrain, n_t2),                    \
    (u32)__builtin_offsetof(AzBrain, off_b1),                  \
    (u32)__builtin_offsetof(AzBrain, off_b2),                  \
    (u32)__builtin_offsetof(AzBrain, off_c1),                  \
    (u32)__builtin_offsetof(AzBrain, off_c2),                  \
    (u32)__builtin_offsetof(AzBrain, off_in1),                 \
    (u32)__builtin_offsetof(AzBrain, off_in2),                 \
    (u32)__builtin_offsetof(AzBrain, off_w1),                  \
    (u32)__builtin_offsetof(AzBrain, off_w2),                  \
    (u32)__builtin_offsetof(AzBrain, gain),                    \
    (u32)__builtin_offsetof(AzBrain, bias),                    \
    (u32)__builtin_offsetof(AzBrain, shift1),                  \
    (u32)__builtin_offsetof(AzBrain, shift2),                  \
    AZ_SENS, AZ_MOT, AZ_VEL_MAX }

#endif
