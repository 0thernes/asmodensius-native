/* az_gpu.c - CUDA driver-API host. See az_gpu.h.
 *
 * nvcuda.dll is loaded at run time with LoadLibrary + GetProcAddress: no
 * import library and no CUDA header at build time. The prototypes below were
 * written by hand and checked against cuda.h of the 12.9 redistributable
 * (CUDA_VERSION 12090); the versioned names are the ones that header's
 * macros map to. CUDAAPI is __stdcall, the ordinary x64 convention.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "az_gpu.h"
#include "az_dense.h"
#include "az_gpu_layout.h"

#ifdef AZ_GPU_HAVE_PTX
extern const char az_gpu_ptx[];        /* generated: the PTX text, NUL-terminated */
extern const unsigned az_gpu_ptx_len;
#endif

typedef int CUresult;                    /* CUDA_SUCCESS = 0 */
typedef int CUdevice;
typedef unsigned long long CUdeviceptr;
typedef struct CUctx_st *CUcontext;
typedef struct CUmod_st *CUmodule;
typedef struct CUfunc_st *CUfunction;
typedef struct CUstream_st *CUstream;

enum {
    CU_JIT_INFO_LOG_BUFFER = 3,
    CU_JIT_INFO_LOG_BUFFER_SIZE_BYTES = 4,
    CU_JIT_ERROR_LOG_BUFFER = 5,
    CU_JIT_ERROR_LOG_BUFFER_SIZE_BYTES = 6,
    CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR = 75,
    CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR = 76
};

#define CUDAAPI __stdcall

struct AzGpu {
    HMODULE dll;
    CUcontext ctx;
    CUmodule mod;
    CUstream stream;
    CUfunction k_layout, k_philox, k_brain, k_tile_brain, k_kin_axis, k_empty;
    u32 block;
    char err[512];

    CUresult (CUDAAPI *cuInit)(unsigned int);
    CUresult (CUDAAPI *cuDriverGetVersion)(int *);
    CUresult (CUDAAPI *cuDeviceGetCount)(int *);
    CUresult (CUDAAPI *cuDeviceGet)(CUdevice *, int);
    CUresult (CUDAAPI *cuDeviceGetName)(char *, int, CUdevice);
    CUresult (CUDAAPI *cuDeviceGetAttribute)(int *, int, CUdevice);
    CUresult (CUDAAPI *cuDeviceTotalMem)(size_t *, CUdevice);
    CUresult (CUDAAPI *cuCtxCreate)(CUcontext *, unsigned int, CUdevice);
    CUresult (CUDAAPI *cuCtxDestroy)(CUcontext);
    CUresult (CUDAAPI *cuCtxSynchronize)(void);
    CUresult (CUDAAPI *cuStreamCreate)(CUstream *, unsigned int);
    CUresult (CUDAAPI *cuStreamDestroy)(CUstream);
    CUresult (CUDAAPI *cuStreamSynchronize)(CUstream);
    CUresult (CUDAAPI *cuModuleLoadDataEx)(CUmodule *, const void *, unsigned int, int *, void **);
    CUresult (CUDAAPI *cuModuleUnload)(CUmodule);
    CUresult (CUDAAPI *cuModuleGetFunction)(CUfunction *, CUmodule, const char *);
    CUresult (CUDAAPI *cuMemAlloc)(CUdeviceptr *, size_t);
    CUresult (CUDAAPI *cuMemFree)(CUdeviceptr);
    CUresult (CUDAAPI *cuMemGetInfo)(size_t *, size_t *);
    CUresult (CUDAAPI *cuMemAllocHost)(void **, size_t);
    CUresult (CUDAAPI *cuMemFreeHost)(void *);
    CUresult (CUDAAPI *cuMemHostRegister)(void *, size_t, unsigned int);
    CUresult (CUDAAPI *cuMemHostUnregister)(void *);
    CUresult (CUDAAPI *cuMemcpyHtoD)(CUdeviceptr, const void *, size_t);
    CUresult (CUDAAPI *cuMemcpyDtoH)(void *, CUdeviceptr, size_t);
    CUresult (CUDAAPI *cuMemcpyHtoDAsync)(CUdeviceptr, const void *, size_t, CUstream);
    CUresult (CUDAAPI *cuMemcpyDtoHAsync)(void *, CUdeviceptr, size_t, CUstream);
    CUresult (CUDAAPI *cuLaunchKernel)(CUfunction, unsigned int, unsigned int, unsigned int,
                                       unsigned int, unsigned int, unsigned int,
                                       unsigned int, CUstream, void **, void **);
    CUresult (CUDAAPI *cuGetErrorName)(CUresult, const char **);
};

const char *az_gpu_status_name(int status)
{
    switch (status) {
    case AZ_GPU_OK:         return "ok";
    case AZ_GPU_NO_DRIVER:  return "no NVIDIA driver";
    case AZ_GPU_NO_DEVICE:  return "no CUDA device";
    case AZ_GPU_NO_PTX:     return "no device code in this build";
    case AZ_GPU_BAD_MODULE: return "device module refused";
    case AZ_GPU_BAD_ARG:    return "bad argument";
    case AZ_GPU_ERROR:      return "driver call failed";
    default:                return "unknown status";
    }
}

static void set_msg(char *dst, size_t cap, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(dst, cap, fmt, ap);
    va_end(ap);
}

static const char *cu_name(const AzGpu *g, CUresult r)
{
    const char *name = NULL;
    if (g->cuGetErrorName) g->cuGetErrorName(r, &name);
    return name ? name : "?";
}

/* record a failed driver call on g and return AZ_GPU_ERROR */
static int fail(AzGpu *g, const char *what, CUresult r)
{
    set_msg(g->err, sizeof g->err, "%s failed: CUresult %d (%s)", what, r, cu_name(g, r));
    return AZ_GPU_ERROR;
}
static int bad_arg(AzGpu *g, const char *what)
{
    if (g) set_msg(g->err, sizeof g->err, "bad argument: %s", what);
    return AZ_GPU_BAD_ARG;
}

const char *az_gpu_error(const AzGpu *g) { return g ? g->err : "no gpu"; }

int az_gpu_set_block(AzGpu *g, u32 block)
{
    if (!g || block == 0 || block > 1024) return bad_arg(g, "block must be 1..1024");
    g->block = block;
    return AZ_GPU_OK;
}

/* resolve every driver symbol; returns the first missing name or NULL */
static const char *resolve(AzGpu *g)
{
#define SYM(field, name)                                                  \
    do {                                                                  \
        FARPROC p_ = GetProcAddress(g->dll, name);                        \
        if (!p_) return name;                                             \
        memcpy(&g->field, &p_, sizeof p_);                                \
    } while (0)
    SYM(cuGetErrorName, "cuGetErrorName");
    SYM(cuInit, "cuInit");
    SYM(cuDriverGetVersion, "cuDriverGetVersion");
    SYM(cuDeviceGetCount, "cuDeviceGetCount");
    SYM(cuDeviceGet, "cuDeviceGet");
    SYM(cuDeviceGetName, "cuDeviceGetName");
    SYM(cuDeviceGetAttribute, "cuDeviceGetAttribute");
    SYM(cuDeviceTotalMem, "cuDeviceTotalMem_v2");
    SYM(cuCtxCreate, "cuCtxCreate_v2");
    SYM(cuCtxDestroy, "cuCtxDestroy_v2");
    SYM(cuCtxSynchronize, "cuCtxSynchronize");
    SYM(cuStreamCreate, "cuStreamCreate");
    SYM(cuStreamDestroy, "cuStreamDestroy_v2");
    SYM(cuStreamSynchronize, "cuStreamSynchronize");
    SYM(cuModuleLoadDataEx, "cuModuleLoadDataEx");
    SYM(cuModuleUnload, "cuModuleUnload");
    SYM(cuModuleGetFunction, "cuModuleGetFunction");
    SYM(cuMemAlloc, "cuMemAlloc_v2");
    SYM(cuMemFree, "cuMemFree_v2");
    SYM(cuMemGetInfo, "cuMemGetInfo_v2");
    SYM(cuMemAllocHost, "cuMemAllocHost_v2");
    SYM(cuMemFreeHost, "cuMemFreeHost");
    SYM(cuMemHostRegister, "cuMemHostRegister_v2");
    SYM(cuMemHostUnregister, "cuMemHostUnregister");
    SYM(cuMemcpyHtoD, "cuMemcpyHtoD_v2");
    SYM(cuMemcpyDtoH, "cuMemcpyDtoH_v2");
    SYM(cuMemcpyHtoDAsync, "cuMemcpyHtoDAsync_v2");
    SYM(cuMemcpyDtoHAsync, "cuMemcpyDtoHAsync_v2");
    SYM(cuLaunchKernel, "cuLaunchKernel");
#undef SYM
    return NULL;
}

static double now_us(void)
{
    LARGE_INTEGER f, c;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart * 1e6 / (double)f.QuadPart;
}

/* copy the token after `key` on the first line that starts with it */
static void ptx_header(const char *ptx, const char *key, char *out, size_t cap)
{
    size_t klen = strlen(key);
    out[0] = 0;
    for (const char *l = ptx; l && *l; ) {
        if (!strncmp(l, key, klen)) {
            const char *p = l + klen;
            while (*p == ' ' || *p == '\t') p++;
            size_t i = 0;
            while (p[i] && p[i] != '\r' && p[i] != '\n' && i + 1 < cap) { out[i] = p[i]; i++; }
            out[i] = 0;
            return;
        }
        const char *nl = strchr(l, '\n');
        l = nl ? nl + 1 : NULL;
    }
}

/* one 1D launch covering n threads */
static int launch_on(AzGpu *g, CUfunction f, const char *what, void **params, u32 n, CUstream st)
{
    u32 block = g->block;
    u32 grid = n ? (u32)(((u64)n + block - 1) / block) : 1;
    CUresult r = g->cuLaunchKernel(f, grid, 1, 1, block, 1, 1, 0, st, params, NULL);
    return r ? fail(g, what, r) : AZ_GPU_OK;
}

static int launch(AzGpu *g, CUfunction f, const char *what, void **params, u32 n)
{
    return launch_on(g, f, what, params, n, NULL);
}

/* run the layout kernel and compare with the host's own layout words */
static int check_layout(AzGpu *g, char *why, size_t cap)
{
    const u32 host[AZ_GPU_LAYOUT_WORDS] = AZ_GPU_LAYOUT_INIT;
    u32 dev[AZ_GPU_LAYOUT_WORDS];
    AzGpuBuf b = { 0, 0 };
    int s = az_gpu_alloc(g, &b, sizeof dev);
    if (s) { set_msg(why, cap, "%s", g->err); return s; }
    memset(dev, 0, sizeof dev);
    s = az_gpu_upload(g, b, 0, dev, sizeof dev);
    if (!s) {
        void *params[1] = { &b.dptr };
        CUresult r = g->cuLaunchKernel(g->k_layout, 1, 1, 1, 1, 1, 1, 0, NULL, params, NULL);
        if (r) s = fail(g, "cuLaunchKernel(az_gpu_k_layout)", r);
    }
    if (!s) s = az_gpu_download(g, dev, b, 0, sizeof dev);
    az_gpu_free(g, &b);
    if (s) { set_msg(why, cap, "%s", g->err); return s; }
    for (u32 i = 0; i < AZ_GPU_LAYOUT_WORDS; i++) {
        if (dev[i] != host[i]) {
            set_msg(why, cap, "layout word %u differs: device %u, host %u "
                    "(the PTX was built from a different az_abi.h)", i, dev[i], host[i]);
            return AZ_GPU_BAD_MODULE;
        }
    }
    return AZ_GPU_OK;
}

int az_gpu_open(AzGpu **out, const AzGpuOptions *opt, AzGpuInfo *info)
{
    AzGpuInfo local;
    AzGpuOptions o;
    if (!info) info = &local;
    memset(info, 0, sizeof *info);
    memset(&o, 0, sizeof o);
    if (opt) o = *opt;
    if (out) *out = NULL;
    int status = AZ_GPU_OK;
#define LEAVE(st, ...) do { status = (st); set_msg(info->msg, sizeof info->msg, __VA_ARGS__); goto done; } while (0)

    AzGpu *g = (AzGpu *)calloc(1, sizeof *g);
    if (!out || !g) LEAVE(AZ_GPU_BAD_ARG, "az_gpu_open: no output pointer or out of memory");
    g->block = o.block ? o.block : 256;
    if (g->block > 1024) LEAVE(AZ_GPU_BAD_ARG, "block %u is above the device limit of 1024", g->block);

    /* 1. the driver DLL. Full path into the system directory: never the search path. */
    if (o.dll_path) {
        set_msg(info->dll, sizeof info->dll, "%s", o.dll_path);
    } else {
        char sys[200];
        UINT n = GetSystemDirectoryA(sys, sizeof sys);
        if (n == 0 || n >= sizeof sys) LEAVE(AZ_GPU_NO_DRIVER, "GetSystemDirectory failed");
        set_msg(info->dll, sizeof info->dll, "%s\\nvcuda.dll", sys);
    }
    g->dll = LoadLibraryA(info->dll);
    if (!g->dll)
        LEAVE(AZ_GPU_NO_DRIVER, "no NVIDIA driver: LoadLibrary(%s) failed, Windows error %lu",
              info->dll, GetLastError());
    const char *missing = resolve(g);
    if (missing)
        LEAVE(AZ_GPU_NO_DRIVER, "%s lacks the symbol %s (driver too old or not a CUDA driver)",
              info->dll, missing);

    /* 2. a device and a context */
    CUresult r = g->cuInit(0);
    if (r) LEAVE(AZ_GPU_NO_DEVICE, "cuInit failed: CUresult %d (%s)", r, cu_name(g, r));
    g->cuDriverGetVersion(&info->driver_version);
    r = g->cuDeviceGetCount(&info->n_devices);
    if (r || info->n_devices <= 0) LEAVE(AZ_GPU_NO_DEVICE, "the driver reports no CUDA device");
    if (o.device >= (u32)info->n_devices)
        LEAVE(AZ_GPU_NO_DEVICE, "device %u requested, %d present", o.device, info->n_devices);
    CUdevice dev = 0;
    r = g->cuDeviceGet(&dev, (int)o.device);
    if (r) LEAVE(AZ_GPU_NO_DEVICE, "cuDeviceGet failed: CUresult %d (%s)", r, cu_name(g, r));
    g->cuDeviceGetName(info->device_name, (int)sizeof info->device_name, dev);
    g->cuDeviceGetAttribute(&info->cc_major, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR, dev);
    g->cuDeviceGetAttribute(&info->cc_minor, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR, dev);
    size_t total = 0;
    g->cuDeviceTotalMem(&total, dev);
    info->mem_bytes = total;
    r = g->cuCtxCreate(&g->ctx, 0, dev);
    if (r) LEAVE(AZ_GPU_NO_DEVICE, "cuCtxCreate failed: CUresult %d (%s)", r, cu_name(g, r));
    r = g->cuStreamCreate(&g->stream, 0);
    if (r) LEAVE(AZ_GPU_ERROR, "cuStreamCreate failed: CUresult %d (%s)", r, cu_name(g, r));

    /* 3. the device module: PTX text, compiled by the driver */
    const char *ptx = o.ptx_text;
#ifdef AZ_GPU_HAVE_PTX
    if (!ptx) ptx = az_gpu_ptx;
#endif
    if (!ptx)
        LEAVE(AZ_GPU_NO_PTX, "this build carries no device code: clang was not found when it was "
              "configured, and no PTX text was passed in AzGpuOptions.ptx_text");
    info->ptx_bytes = (u32)strlen(ptx);
    ptx_header(ptx, ".version", info->ptx_version, sizeof info->ptx_version);
    ptx_header(ptx, ".target", info->ptx_target, sizeof info->ptx_target);
    {
        static const int jit_keys[4] = { CU_JIT_INFO_LOG_BUFFER, CU_JIT_INFO_LOG_BUFFER_SIZE_BYTES,
                                         CU_JIT_ERROR_LOG_BUFFER, CU_JIT_ERROR_LOG_BUFFER_SIZE_BYTES };
        int keys[4];
        char *info_log = (char *)calloc(1, 4096), *err_log = (char *)calloc(1, 4096);
        void *vals[4] = { info_log, (void *)(uintptr_t)4096, err_log, (void *)(uintptr_t)4096 };
        memcpy(keys, jit_keys, sizeof keys);
        double t0 = now_us();
        r = (info_log && err_log) ? g->cuModuleLoadDataEx(&g->mod, ptx, 4, keys, vals) : 2;
        info->jit_us = (u32)(now_us() - t0);
        if (r) {
            set_msg(info->msg, sizeof info->msg, "the driver refused the PTX (.version %s, .target %s): "
                    "CUresult %d (%s). %.300s", info->ptx_version, info->ptx_target, r, cu_name(g, r),
                    err_log ? err_log : "");
            status = AZ_GPU_BAD_MODULE;
        }
        free(info_log);
        free(err_log);
        if (r) goto done;
    }
    {
        static const char *const names[6] = { "az_gpu_k_layout", "az_gpu_k_philox",
                                              "az_gpu_k_brain", "az_gpu_k_tile_brain",
                                              "az_gpu_k_kin_axis", "az_gpu_k_empty" };
        CUfunction *slots[6] = { &g->k_layout, &g->k_philox, &g->k_brain,
                                 &g->k_tile_brain, &g->k_kin_axis, &g->k_empty };
        for (int i = 0; i < 6; i++) {
            r = g->cuModuleGetFunction(slots[i], g->mod, names[i]);
            if (r) LEAVE(AZ_GPU_BAD_MODULE, "the PTX module has no kernel %s: CUresult %d (%s)",
                         names[i], r, cu_name(g, r));
        }
    }

    /* 4. host and device must agree on the ABI before any kernel is trusted */
    {
        char why[300];
        int s = check_layout(g, why, sizeof why);
        if (s) LEAVE(s, "layout check: %s", why);
    }

    set_msg(info->msg, sizeof info->msg,
            "%s, compute %d.%d, %llu MiB, driver API %d; PTX %u bytes (.version %s, .target %s) "
            "loaded in %u us", info->device_name, info->cc_major, info->cc_minor,
            (unsigned long long)(info->mem_bytes >> 20), info->driver_version, info->ptx_bytes,
            info->ptx_version, info->ptx_target, info->jit_us);
#undef LEAVE
done:
    info->status = status;
    if (status != AZ_GPU_OK) {
        az_gpu_close(g);
        return status;
    }
    *out = g;
    return AZ_GPU_OK;
}

void az_gpu_close(AzGpu *g)
{
    if (!g) return;
    if (g->stream && g->cuStreamDestroy) g->cuStreamDestroy(g->stream);
    if (g->mod && g->cuModuleUnload) g->cuModuleUnload(g->mod);
    if (g->ctx && g->cuCtxDestroy) g->cuCtxDestroy(g->ctx);
    if (g->dll) FreeLibrary(g->dll);
    free(g);
}

int az_gpu_mem_info(AzGpu *g, u64 *free_bytes, u64 *total_bytes)
{
    if (!g) return AZ_GPU_BAD_ARG;
    size_t f = 0, t = 0;
    CUresult r = g->cuMemGetInfo(&f, &t);
    if (r) return fail(g, "cuMemGetInfo", r);
    if (free_bytes) *free_bytes = (u64)f;
    if (total_bytes) *total_bytes = (u64)t;
    return AZ_GPU_OK;
}

/* ---- device buffers --------------------------------------------------- */

int az_gpu_alloc(AzGpu *g, AzGpuBuf *b, u64 bytes)
{
    if (!g || !b || bytes == 0) return bad_arg(g, "az_gpu_alloc");
    CUdeviceptr p = 0;
    CUresult r = g->cuMemAlloc(&p, (size_t)bytes);
    if (r) { b->dptr = 0; b->bytes = 0; return fail(g, "cuMemAlloc", r); }
    b->dptr = p;
    b->bytes = bytes;
    return AZ_GPU_OK;
}

int az_gpu_free(AzGpu *g, AzGpuBuf *b)
{
    if (!g || !b) return bad_arg(g, "az_gpu_free");
    if (!b->dptr) return AZ_GPU_OK;
    CUresult r = g->cuMemFree(b->dptr);
    b->dptr = 0;
    b->bytes = 0;
    return r ? fail(g, "cuMemFree", r) : AZ_GPU_OK;
}

static bool in_range(AzGpuBuf b, u64 offset, u64 bytes)
{
    return b.dptr != 0 && offset <= b.bytes && bytes <= b.bytes - offset;
}

int az_gpu_upload(AzGpu *g, AzGpuBuf b, u64 offset, const void *src, u64 bytes)
{
    if (!g || !src || !in_range(b, offset, bytes)) return bad_arg(g, "az_gpu_upload range");
    if (bytes == 0) return AZ_GPU_OK;
    CUresult r = g->cuMemcpyHtoD(b.dptr + offset, src, (size_t)bytes);
    return r ? fail(g, "cuMemcpyHtoD", r) : AZ_GPU_OK;
}

int az_gpu_download(AzGpu *g, void *dst, AzGpuBuf b, u64 offset, u64 bytes)
{
    if (!g || !dst || !in_range(b, offset, bytes)) return bad_arg(g, "az_gpu_download range");
    if (bytes == 0) return AZ_GPU_OK;
    CUresult r = g->cuMemcpyDtoH(dst, b.dptr + offset, (size_t)bytes);
    return r ? fail(g, "cuMemcpyDtoH", r) : AZ_GPU_OK;
}

int az_gpu_upload_async(AzGpu *g, AzGpuBuf b, u64 offset, const void *src, u64 bytes)
{
    if (!g || !src || !in_range(b, offset, bytes)) return bad_arg(g, "az_gpu_upload_async range");
    if (bytes == 0) return AZ_GPU_OK;
    CUresult r = g->cuMemcpyHtoDAsync(b.dptr + offset, src, (size_t)bytes, g->stream);
    return r ? fail(g, "cuMemcpyHtoDAsync", r) : AZ_GPU_OK;
}

int az_gpu_download_async(AzGpu *g, void *dst, AzGpuBuf b, u64 offset, u64 bytes)
{
    if (!g || !dst || !in_range(b, offset, bytes)) return bad_arg(g, "az_gpu_download_async range");
    if (bytes == 0) return AZ_GPU_OK;
    CUresult r = g->cuMemcpyDtoHAsync(dst, b.dptr + offset, (size_t)bytes, g->stream);
    return r ? fail(g, "cuMemcpyDtoHAsync", r) : AZ_GPU_OK;
}

int az_gpu_stream_sync(AzGpu *g)
{
    if (!g) return AZ_GPU_BAD_ARG;
    CUresult r = g->cuStreamSynchronize(g->stream);
    return r ? fail(g, "cuStreamSynchronize", r) : AZ_GPU_OK;
}

int az_gpu_sync(AzGpu *g)
{
    if (!g) return AZ_GPU_BAD_ARG;
    CUresult r = g->cuCtxSynchronize();
    return r ? fail(g, "cuCtxSynchronize", r) : AZ_GPU_OK;
}

void *az_gpu_host_alloc(AzGpu *g, u64 bytes)
{
    void *p = NULL;
    if (!g || bytes == 0) return NULL;
    CUresult r = g->cuMemAllocHost(&p, (size_t)bytes);
    if (r) { fail(g, "cuMemAllocHost", r); return NULL; }
    return p;
}

void az_gpu_host_free(AzGpu *g, void *p)
{
    if (g && p) g->cuMemFreeHost(p);
}

int az_gpu_host_register(AzGpu *g, void *p, u64 bytes)
{
    if (!g || !p || bytes == 0) return bad_arg(g, "az_gpu_host_register");
    CUresult r = g->cuMemHostRegister(p, (size_t)bytes, 0u);
    return r ? fail(g, "cuMemHostRegister", r) : AZ_GPU_OK;
}

void az_gpu_host_unregister(AzGpu *g, void *p)
{
    if (g && p && g->cuMemHostUnregister) g->cuMemHostUnregister(p);
}

/* ---- kernels ---------------------------------------------------------- */

int az_gpu_philox(AzGpu *g, AzGpuBuf out4n, AzGpuBuf ctr4n, const u32 key[2], u32 n)
{
    if (!g || !key) return bad_arg(g, "az_gpu_philox");
    if (n == 0) return AZ_GPU_OK;
    if (!in_range(out4n, 0, (u64)n * 16) || !in_range(ctr4n, 0, (u64)n * 16))
        return bad_arg(g, "az_gpu_philox: a buffer is smaller than n * 16 bytes");
    u32 k0 = key[0], k1 = key[1];
    void *params[5] = { &out4n.dptr, &ctr4n.dptr, &k0, &k1, &n };
    return launch(g, g->k_philox, "cuLaunchKernel(az_gpu_k_philox)", params, n);
}

int az_gpu_brain_step(AzGpu *g, const AzGpuBrain *b, u32 n)
{
    if (!g || !b) return bad_arg(g, "az_gpu_brain_step");
    if (n == 0) return AZ_GPU_OK;
    if (!in_range(b->sens, 0, (u64)n * AZ_DENSE_SENS) || !in_range(b->hid, 0, (u64)n * AZ_DENSE_HID) ||
        !in_range(b->gain, 0, (u64)n * AZ_DENSE_MODG) || !in_range(b->bias, 0, (u64)n * AZ_DENSE_MODG) ||
        !in_range(b->clade, 0, (u64)n * sizeof(u16)) || !in_range(b->motor, 0, (u64)n * AZ_DENSE_MOT) ||
        b->n_clades == 0 || !in_range(b->clades, 0, (u64)b->n_clades * sizeof(AzDenseNet)))
        return bad_arg(g, "az_gpu_brain_step: a buffer is smaller than n creatures need");
    AzGpuBrain a = *b;
    u32 nn = n;
    void *params[9] = { &a.sens.dptr, &a.hid.dptr, &a.gain.dptr, &a.bias.dptr, &a.clade.dptr,
                        &a.clades.dptr, &a.n_clades, &a.motor.dptr, &nn };
    return launch(g, g->k_brain, "cuLaunchKernel(az_gpu_k_brain)", params, n);
}

int az_gpu_tile_brain(AzGpu *g, const AzGpuTileBrain *tb, u32 n, bool async_stream)
{
    if (!g || !tb) return bad_arg(g, "az_gpu_tile_brain");
    if (n == 0) return AZ_GPU_OK;
    if (!tb->store.dptr ||
        !in_range(tb->brain_off8, 0, (u64)n * sizeof(u32)) ||
        !in_range(tb->mind_off8, 0, (u64)n * sizeof(u32)) ||
        !in_range(tb->hid_off8, 0, (u64)n * sizeof(u32)) ||
        !in_range(tb->sens_in, 0, (u64)n * AZ_SENS) ||
        !in_range(tb->gain_in, 0, (u64)n * 8u) ||
        !in_range(tb->bias_in, 0, (u64)n * 8u) ||
        !in_range(tb->motor_out, 0, (u64)n * AZ_MOT) ||
        !tb->mind_scratch.dptr)
        return bad_arg(g, "az_gpu_tile_brain: buffer smaller than n creatures need");
    AzGpuTileBrain a = *tb;
    u32 nn = n;
    void *params[12] = { &a.store.dptr, &a.brain_off8.dptr, &a.mind_off8.dptr, &a.hid_off8.dptr,
                         &a.sens_in.dptr, &a.hid_in.dptr, &a.gain_in.dptr, &a.bias_in.dptr,
                         &a.mind_scratch.dptr, &a.hid_out.dptr, &a.motor_out.dptr, &nn };
    return launch_on(g, g->k_tile_brain, "cuLaunchKernel(az_gpu_k_tile_brain)", params, n,
                     async_stream ? g->stream : NULL);
}

int az_gpu_kin_axis(AzGpu *g, AzGpuBuf pos, AzGpuBuf vel, AzGpuBuf dv, AzGpuBuf drag, u32 n, u32 shift)
{
    if (!g) return bad_arg(g, "az_gpu_kin_axis");
    if (n == 0) return AZ_GPU_OK;
    if (!in_range(pos, 0, (u64)n * sizeof(u32)) || !in_range(vel, 0, (u64)n * sizeof(i16)) ||
        !in_range(dv, 0, (u64)n * sizeof(i16)) || !in_range(drag, 0, (u64)n * sizeof(u8)))
        return bad_arg(g, "az_gpu_kin_axis: buffer smaller than n entities need");
    u32 nn = n, sh = shift;
    void *params[6] = { &pos.dptr, &vel.dptr, &dv.dptr, &drag.dptr, &nn, &sh };
    return launch(g, g->k_kin_axis, "cuLaunchKernel(az_gpu_k_kin_axis)", params, n);
}

int az_gpu_empty(AzGpu *g)
{
    if (!g) return AZ_GPU_BAD_ARG;
    CUresult r = g->cuLaunchKernel(g->k_empty, 1, 1, 1, 1, 1, 1, 0, NULL, NULL, NULL);
    return r ? fail(g, "cuLaunchKernel(az_gpu_k_empty)", r) : AZ_GPU_OK;
}

/* ---- whole trips on host memory ---------------------------------------- */

int az_gpu_philox_host(AzGpu *g, u32 *out4n, const u32 *ctr4n, const u32 key[2], u32 n)
{
    if (!g || !out4n || !ctr4n || !key) return bad_arg(g, "az_gpu_philox_host");
    if (n == 0) return AZ_GPU_OK;
    AzGpuBuf out = { 0, 0 }, ctr = { 0, 0 };
    u64 bytes = (u64)n * 16;
    int s = az_gpu_alloc(g, &out, bytes);
    if (!s) s = az_gpu_alloc(g, &ctr, bytes);
    if (!s) s = az_gpu_upload(g, ctr, 0, ctr4n, bytes);
    if (!s) s = az_gpu_philox(g, out, ctr, key, n);
    if (!s) s = az_gpu_download(g, out4n, out, 0, bytes);
    az_gpu_free(g, &out);
    az_gpu_free(g, &ctr);
    return s;
}

int az_gpu_brain_step_host(AzGpu *g, const AzDenseNet *clades, u32 n_clades,
                           const u8 *sens, u8 *hid, const u8 *gain, const i8 *bias,
                           const u16 *clade, i8 *motor, u32 n)
{
    if (!g || !clades || !n_clades || !sens || !hid || !gain || !bias || !clade || !motor)
        return bad_arg(g, "az_gpu_brain_step_host");
    if (n == 0) return AZ_GPU_OK;
    AzGpuBrain b;
    memset(&b, 0, sizeof b);
    b.n_clades = n_clades;
    int s = az_gpu_alloc(g, &b.sens, (u64)n * AZ_DENSE_SENS);
    if (!s) s = az_gpu_alloc(g, &b.hid, (u64)n * AZ_DENSE_HID);
    if (!s) s = az_gpu_alloc(g, &b.gain, (u64)n * AZ_DENSE_MODG);
    if (!s) s = az_gpu_alloc(g, &b.bias, (u64)n * AZ_DENSE_MODG);
    if (!s) s = az_gpu_alloc(g, &b.clade, (u64)n * sizeof(u16));
    if (!s) s = az_gpu_alloc(g, &b.motor, (u64)n * AZ_DENSE_MOT);
    if (!s) s = az_gpu_alloc(g, &b.clades, (u64)n_clades * sizeof(AzDenseNet));
    if (!s) s = az_gpu_upload(g, b.sens, 0, sens, (u64)n * AZ_DENSE_SENS);
    if (!s) s = az_gpu_upload(g, b.hid, 0, hid, (u64)n * AZ_DENSE_HID);
    if (!s) s = az_gpu_upload(g, b.gain, 0, gain, (u64)n * AZ_DENSE_MODG);
    if (!s) s = az_gpu_upload(g, b.bias, 0, bias, (u64)n * AZ_DENSE_MODG);
    if (!s) s = az_gpu_upload(g, b.clade, 0, clade, (u64)n * sizeof(u16));
    if (!s) s = az_gpu_upload(g, b.clades, 0, clades, (u64)n_clades * sizeof(AzDenseNet));
    if (!s) s = az_gpu_brain_step(g, &b, n);
    if (!s) s = az_gpu_download(g, hid, b.hid, 0, (u64)n * AZ_DENSE_HID);
    if (!s) s = az_gpu_download(g, motor, b.motor, 0, (u64)n * AZ_DENSE_MOT);
    az_gpu_free(g, &b.sens);
    az_gpu_free(g, &b.hid);
    az_gpu_free(g, &b.gain);
    az_gpu_free(g, &b.bias);
    az_gpu_free(g, &b.clade);
    az_gpu_free(g, &b.motor);
    az_gpu_free(g, &b.clades);
    return s;
}

int az_gpu_kin_axis_host(AzGpu *g, u32 *pos, i16 *vel, const i16 *dv, const u8 *drag, u32 n, u32 shift)
{
    if (!g || !pos || !vel || !dv || !drag) return bad_arg(g, "az_gpu_kin_axis_host");
    if (n == 0) return AZ_GPU_OK;
    AzGpuBuf bp = { 0, 0 }, bv = { 0, 0 }, bd = { 0, 0 }, br = { 0, 0 };
    int s = az_gpu_alloc(g, &bp, (u64)n * sizeof(u32));
    if (!s) s = az_gpu_alloc(g, &bv, (u64)n * sizeof(i16));
    if (!s) s = az_gpu_alloc(g, &bd, (u64)n * sizeof(i16));
    if (!s) s = az_gpu_alloc(g, &br, (u64)n * sizeof(u8));
    if (!s) s = az_gpu_upload(g, bp, 0, pos, (u64)n * sizeof(u32));
    if (!s) s = az_gpu_upload(g, bv, 0, vel, (u64)n * sizeof(i16));
    if (!s) s = az_gpu_upload(g, bd, 0, dv, (u64)n * sizeof(i16));
    if (!s) s = az_gpu_upload(g, br, 0, drag, (u64)n * sizeof(u8));
    if (!s) s = az_gpu_kin_axis(g, bp, bv, bd, br, n, shift);
    if (!s) s = az_gpu_download(g, pos, bp, 0, (u64)n * sizeof(u32));
    if (!s) s = az_gpu_download(g, vel, bv, 0, (u64)n * sizeof(i16));
    az_gpu_free(g, &bp);
    az_gpu_free(g, &bv);
    az_gpu_free(g, &bd);
    az_gpu_free(g, &br);
    return s;
}
