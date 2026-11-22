/* az_dense.h - the dense fixed-size int8 network: 48 + 32 inputs -> 32
 * hidden -> 16 outputs, one weight table shared by many creatures.
 *
 * This was the engine's brain until ABI 4 gave every creature its own brain
 * block of any size (core/az_brain.c). The dense network stays as the batch
 * kernel of the GPU twin (gpu/) and of az_brainbench: one table, many
 * creatures, the same int8 arithmetic. The engine's tick does not use it.
 */
#ifndef AZ_DENSE_H
#define AZ_DENSE_H
#include "az_abi.h"

#define AZ_DENSE_SENS    48
#define AZ_DENSE_HID     32
#define AZ_DENSE_MOT     16
#define AZ_DENSE_MODG    8
#define AZ_DENSE_NEURONS (AZ_DENSE_SENS + AZ_DENSE_HID + AZ_DENSE_MOT)

/* One dense network: a neural program plus the weights developed from it.
 * The developed network is stored in the layout the int8 kernel wants, and
 * everything one brain step reads is contiguous at the front. */
#define AZ_W1_LEN (AZ_DENSE_HID * (AZ_DENSE_SENS + AZ_DENSE_HID))   /* 2560 */
#define AZ_W2_LEN (AZ_DENSE_MOT * AZ_DENSE_HID)               /*  512 */
/* weight of hidden unit h for input j (j < AZ_DENSE_SENS: sensor; else hidden j-AZ_DENSE_SENS) */
#define AZ_W1_IDX(h, j) ((((j) >> 2) * (AZ_DENSE_HID * 4)) + ((h) * 4) + ((j) & 3))
/* weight of motor m for hidden unit h */
#define AZ_W2_IDX(m, h) ((((h) >> 2) * (AZ_DENSE_MOT * 4)) + ((m) * 4) + ((h) & 3))

typedef struct AzDenseNet {
    /* ---- read by every brain step (offsets are relied on by the assembly twins) ---- */
    i8  w1[AZ_W1_LEN];                      /* offset    0 */
    i8  w2[AZ_W2_LEN];                      /* offset 2560 */
    i32 b1[AZ_DENSE_HID];                         /* offset 3072 */
    i32 b2[AZ_DENSE_MOT];                         /* offset 3200 */
    u8  shift1, shift2;                     /* offset 3264, 3265: right shifts after each layer, 1..15 */
    u8  var;                                /* per-synapse variation amplitude   */
    u8  alive;                              /* 1 while the slot is in use        */
    u16 salt;                               /* per-synapse variation seed        */
    u16 pad0;
    /* ---- neural program (the heritable description the network is developed
     * from). Neuron index space: sensors 0..47, hidden 48..79, motors 80..95 ---- */
    u8  ntype[AZ_DENSE_NEURONS];                  /* neuron type 0..7                  */
    i8  theta[64];                          /* wiring rule: theta[post*8 + pre]  */
    i8  tbias[8];                           /* bias per neuron type              */
    /* ---- species look and body plan (AZ_B_*) ---- */
    u8  body[16];
    /* ---- bookkeeping ---- */
    u32 refcount;      /* living members                                        */
    u32 species;       /* founder species index this clade descends from        */
    u64 founder_uid;   /* uid of the individual that founded the clade: its stable id */
    u64 parent_uid;    /* founder_uid of the parent clade, 0 for a founder species */
    u64 born_tick;
    u32 pad1[8];       /* pads the record to 55 cache lines so every tile starts on a line */
} AzDenseNet;
_Static_assert(sizeof(AzDenseNet) == 3520, "AzDenseNet size is part of the ABI");
_Static_assert(__builtin_offsetof(AzDenseNet, w2) == 2560 && __builtin_offsetof(AzDenseNet, b1) == 3072 &&
               __builtin_offsetof(AzDenseNet, b2) == 3200 && __builtin_offsetof(AzDenseNet, shift1) == 3264,
               "AzDenseNet kernel offsets are part of the ABI");


/* The exact integer semantics of one dense step; the twins are bit-identical. */
void az_dense_step_c(const AzDenseNet *c, const u8 *sens, u8 *hid,
                     const u8 *gain, const i8 *bias, i8 *motor);
void az_dense_develop(AzDenseNet *c);   /* neural program -> w1, w2, b1, b2; clamps the shifts to 1..15 */
/* kernels/az_dense_vnni.asm */
void az_dense_step_vnni(const AzDenseNet *c, const u8 *sens, u8 *hid,
                        const u8 *gain, const i8 *bias, i8 *motor);
void az_dense_step_avx2(const AzDenseNet *c, const u8 *sens, u8 *hid,
                        const u8 *gain, const i8 *bias, i8 *motor);

#endif
