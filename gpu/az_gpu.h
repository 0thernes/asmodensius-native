/* az_gpu.h - GPU path: CUDA driver-API host for the integer kernels.
 *
 * No CUDA toolkit is needed at build time or at run time. The host loads
 * nvcuda.dll (part of the NVIDIA display driver) with LoadLibrary, and the
 * driver JIT-compiles PTX text that clang produced from gpu/az_gpu_dev.c.
 * That device file includes the same core/az_brain.c and core/az_rng.c the
 * CPU build compiles, so there is one definition of the semantics.
 *
 * Law: every kernel is bit-identical to its CPU reference (gpu/test_gpu.c).
 *
 * Hosted C (libc and Windows allowed). Not part of the freestanding core:
 * the core never calls this; a host or a tool does, between ticks.
 *
 * Threading: all calls on one AzGpu must come from the thread that opened it
 * (the driver context is current on that thread only).
 */
#ifndef AZ_GPU_H
#define AZ_GPU_H

#include "az_abi.h"
#include "az_dense.h"

enum {
    AZ_GPU_OK = 0,
    AZ_GPU_NO_DRIVER,   /* nvcuda.dll not found, or it lacks a required symbol   */
    AZ_GPU_NO_DEVICE,   /* driver present but cuInit failed or no CUDA device    */
    AZ_GPU_NO_PTX,      /* this build carries no device code (clang was absent)  */
    AZ_GPU_BAD_MODULE,  /* the driver refused the PTX, or the layout check failed */
    AZ_GPU_BAD_ARG,     /* a buffer is too small or a pointer is NULL            */
    AZ_GPU_ERROR        /* a driver call failed; see az_gpu_error()              */
};

typedef struct AzGpu AzGpu;

typedef struct AzGpuOptions {
    const char *dll_path;   /* NULL = <system directory>\nvcuda.dll               */
    const char *ptx_text;   /* NULL = the PTX embedded at build time; else NUL-terminated PTX */
    u32 device;             /* device ordinal, usually 0                          */
    u32 block;              /* threads per block; 0 = 256                         */
} AzGpuOptions;

typedef struct AzGpuInfo {
    int  status;            /* AZ_GPU_* of az_gpu_open                            */
    int  driver_version;    /* cuDriverGetVersion, e.g. 13040                     */
    int  n_devices;
    int  cc_major, cc_minor;/* compute capability                                 */
    u64  mem_bytes;
    u32  ptx_bytes;
    u32  jit_us;            /* time the driver took to load the PTX               */
    char device_name[128];
    char ptx_version[16];   /* ".version" line of the PTX                         */
    char ptx_target[32];    /* ".target" line of the PTX                          */
    char dll[260];
    char msg[512];          /* what happened, always filled, one or two lines     */
} AzGpuInfo;

/* a device buffer */
typedef struct AzGpuBuf {
    u64 dptr;               /* CUdeviceptr; 0 = not allocated                     */
    u64 bytes;
} AzGpuBuf;

/* device buffers of the brain step; same layout as the CPU columns */
typedef struct AzGpuBrain {
    AzGpuBuf sens;          /* u8  [n * AZ_DENSE_SENS]                                  */
    AzGpuBuf hid;           /* u8  [n * AZ_DENSE_HID], read and written in place        */
    AzGpuBuf gain;          /* u8  [n * AZ_DENSE_MODG]                                  */
    AzGpuBuf bias;          /* i8  [n * AZ_DENSE_MODG]                                  */
    AzGpuBuf clade;         /* u16 [n]                                            */
    AzGpuBuf clades;        /* AzDenseNet [n_clades] as raw bytes                    */
    AzGpuBuf motor;         /* i8  [n * AZ_DENSE_MOT]                                   */
    u32      n_clades;
} AzGpuBrain;

const char *az_gpu_status_name(int status);

/* Open the driver, create a context, load the device module, verify the
 * shared layout. Returns AZ_GPU_OK and *out, or a status with *out = NULL.
 * `info` may be NULL; when given, info->msg always says what happened. On a
 * machine without an NVIDIA driver this returns AZ_GPU_NO_DRIVER: it reports
 * and returns, it never aborts. */
int  az_gpu_open(AzGpu **out, const AzGpuOptions *opt, AzGpuInfo *info);
void az_gpu_close(AzGpu *g);
const char *az_gpu_error(const AzGpu *g);     /* text of the last failure on g */
int  az_gpu_set_block(AzGpu *g, u32 block);   /* threads per block, 1..1024; results do not depend on it */

/* device buffers */
int az_gpu_alloc(AzGpu *g, AzGpuBuf *b, u64 bytes);
int az_gpu_free(AzGpu *g, AzGpuBuf *b);
int az_gpu_upload(AzGpu *g, AzGpuBuf b, u64 offset, const void *src, u64 bytes);
int az_gpu_download(AzGpu *g, void *dst, AzGpuBuf b, u64 offset, u64 bytes);
int az_gpu_sync(AzGpu *g);                    /* wait for queued kernels */

/* page-locked host memory from the driver (faster copies); free with az_gpu_host_free */
void *az_gpu_host_alloc(AzGpu *g, u64 bytes);
void  az_gpu_host_free(AzGpu *g, void *p);

/* Kernel launches on device buffers. They queue the kernel and return; call
 * az_gpu_sync (or download, which waits) before reading results. Every
 * launch checks the buffer sizes against n first. */
int az_gpu_philox(AzGpu *g, AzGpuBuf out4n, AzGpuBuf ctr4n, const u32 key[2], u32 n);
int az_gpu_brain_step(AzGpu *g, const AzGpuBrain *b, u32 n);
int az_gpu_empty(AzGpu *g);

/* Whole-trip conveniences on host memory: allocate, upload, run, download, free. */
int az_gpu_philox_host(AzGpu *g, u32 *out4n, const u32 *ctr4n, const u32 key[2], u32 n);
int az_gpu_brain_step_host(AzGpu *g, const AzDenseNet *clades, u32 n_clades,
                           const u8 *sens, u8 *hid, const u8 *gain, const i8 *bias,
                           const u16 *clade, i8 *motor, u32 n);

#endif /* AZ_GPU_H */
