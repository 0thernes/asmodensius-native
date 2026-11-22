// host.c - CUDA driver-API host with NO CUDA toolkit and NO import library.
// Loads C:\Windows\System32\nvcuda.dll at run time (LoadLibrary + GetProcAddress),
// JIT-loads PTX text produced by clang, runs an integer histogram kernel and
// compares the result bit-for-bit with a CPU computation.
//
// Build:  gcc -std=c2x -O2 -Wall -Wextra host.c -o host.exe
// Run:    host.exe kernel.ptx
//
// Prototypes below were checked against Z:\Tools\cuda-12.9-redist\include\cuda.h
// (CUDA_VERSION 12090). Symbol names are the ones that header's macros map to:
//   cuCtxCreate   -> cuCtxCreate_v2      cuMemAlloc   -> cuMemAlloc_v2
//   cuMemFree     -> cuMemFree_v2        cuMemcpyHtoD -> cuMemcpyHtoD_v2
//   cuMemcpyDtoH  -> cuMemcpyDtoH_v2     cuCtxDestroy -> cuCtxDestroy_v2
//   cuDeviceTotalMem -> cuDeviceTotalMem_v2
// (cuLaunchKernel, cuModuleLoadDataEx, cuCtxSynchronize ... are unversioned.)
// CUDAAPI is __stdcall on Windows, which is the ordinary x64 convention.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int CUresult;                    // enum cudaError_enum; CUDA_SUCCESS = 0
typedef int CUdevice;                    // cuda.h: typedef int CUdevice_v1
typedef unsigned long long CUdeviceptr;  // cuda.h: CUdeviceptr_v2 on _WIN64
typedef struct CUctx_st *CUcontext;
typedef struct CUmod_st *CUmodule;
typedef struct CUfunc_st *CUfunction;
typedef struct CUstream_st *CUstream;

// CUjit_option values (cuda.h lines 1168-1195) and device attributes (790-791).
enum {
    CU_JIT_INFO_LOG_BUFFER = 3,
    CU_JIT_INFO_LOG_BUFFER_SIZE_BYTES = 4,
    CU_JIT_ERROR_LOG_BUFFER = 5,
    CU_JIT_ERROR_LOG_BUFFER_SIZE_BYTES = 6,
    CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR = 75,
    CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR = 76
};

#define CUDAAPI __stdcall
static CUresult (CUDAAPI *p_cuInit)(unsigned int Flags);
static CUresult (CUDAAPI *p_cuDriverGetVersion)(int *driverVersion);
static CUresult (CUDAAPI *p_cuDeviceGetCount)(int *count);
static CUresult (CUDAAPI *p_cuDeviceGet)(CUdevice *device, int ordinal);
static CUresult (CUDAAPI *p_cuDeviceGetName)(char *name, int len, CUdevice dev);
static CUresult (CUDAAPI *p_cuDeviceGetAttribute)(int *pi, int attrib, CUdevice dev);
static CUresult (CUDAAPI *p_cuDeviceTotalMem)(size_t *bytes, CUdevice dev);
static CUresult (CUDAAPI *p_cuCtxCreate)(CUcontext *pctx, unsigned int flags, CUdevice dev);
static CUresult (CUDAAPI *p_cuCtxDestroy)(CUcontext ctx);
static CUresult (CUDAAPI *p_cuCtxSynchronize)(void);
static CUresult (CUDAAPI *p_cuModuleLoadDataEx)(CUmodule *module, const void *image,
                                                unsigned int numOptions, int *options,
                                                void **optionValues);
static CUresult (CUDAAPI *p_cuModuleUnload)(CUmodule hmod);
static CUresult (CUDAAPI *p_cuModuleGetFunction)(CUfunction *hfunc, CUmodule hmod, const char *name);
static CUresult (CUDAAPI *p_cuMemAlloc)(CUdeviceptr *dptr, size_t bytesize);
static CUresult (CUDAAPI *p_cuMemFree)(CUdeviceptr dptr);
static CUresult (CUDAAPI *p_cuMemcpyHtoD)(CUdeviceptr dstDevice, const void *srcHost, size_t ByteCount);
static CUresult (CUDAAPI *p_cuMemcpyDtoH)(void *dstHost, CUdeviceptr srcDevice, size_t ByteCount);
static CUresult (CUDAAPI *p_cuLaunchKernel)(CUfunction f,
                                            unsigned int gridDimX, unsigned int gridDimY, unsigned int gridDimZ,
                                            unsigned int blockDimX, unsigned int blockDimY, unsigned int blockDimZ,
                                            unsigned int sharedMemBytes, CUstream hStream,
                                            void **kernelParams, void **extra);
static CUresult (CUDAAPI *p_cuGetErrorName)(CUresult error, const char **pStr);

static void die_cu(const char *what, CUresult r) {
    const char *name = "?";
    if (p_cuGetErrorName) p_cuGetErrorName(r, &name);
    fprintf(stderr, "FAIL %s: CUresult %d (%s)\n", what, r, name ? name : "?");
    exit(2);
}
#define CK(call) do { CUresult r_ = (call); if (r_ != 0) die_cu(#call, r_); } while (0)

static void *sym(HMODULE h, const char *name) {
    void *p = (void *)GetProcAddress(h, name);
    if (!p) { fprintf(stderr, "FAIL GetProcAddress(%s): error %lu\n", name, GetLastError()); exit(2); }
    return p;
}

// Same hash as kernel.cu (murmur3 32-bit finaliser).
static inline uint32_t hash32(uint32_t x) {
    x ^= x >> 16; x *= 0x85ebca6bu;
    x ^= x >> 13; x *= 0xc2b2ae35u;
    x ^= x >> 16;
    return x;
}

static uint64_t fnv1a64(const void *p, size_t n) {
    const unsigned char *b = p;
    uint64_t h = 0xcbf29ce484222325ull;
    for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 0x100000001b3ull; }
    return h;
}

static double qpc_us(void) {
    static LARGE_INTEGER f;
    LARGE_INTEGER c;
    if (!f.QuadPart) QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart * 1e6 / (double)f.QuadPart;
}

static int cmp_double(const void *a, const void *b) {
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

enum { N = 1000000, BINS = 4096 };

int main(int argc, char **argv) {
    const char *ptx_path = argc > 1 ? argv[1] : "kernel.ptx";
    const char *dll_path = "C:\\Windows\\System32\\nvcuda.dll";

    HMODULE h = LoadLibraryA(dll_path);
    if (!h) { fprintf(stderr, "FAIL LoadLibrary(%s): error %lu\n", dll_path, GetLastError()); return 2; }
    printf("loaded %s\n", dll_path);

    p_cuGetErrorName       = sym(h, "cuGetErrorName");
    p_cuInit               = sym(h, "cuInit");
    p_cuDriverGetVersion   = sym(h, "cuDriverGetVersion");
    p_cuDeviceGetCount     = sym(h, "cuDeviceGetCount");
    p_cuDeviceGet          = sym(h, "cuDeviceGet");
    p_cuDeviceGetName      = sym(h, "cuDeviceGetName");
    p_cuDeviceGetAttribute = sym(h, "cuDeviceGetAttribute");
    p_cuDeviceTotalMem     = sym(h, "cuDeviceTotalMem_v2");
    p_cuCtxCreate          = sym(h, "cuCtxCreate_v2");
    p_cuCtxDestroy         = sym(h, "cuCtxDestroy_v2");
    p_cuCtxSynchronize     = sym(h, "cuCtxSynchronize");
    p_cuModuleLoadDataEx   = sym(h, "cuModuleLoadDataEx");
    p_cuModuleUnload       = sym(h, "cuModuleUnload");
    p_cuModuleGetFunction  = sym(h, "cuModuleGetFunction");
    p_cuMemAlloc           = sym(h, "cuMemAlloc_v2");
    p_cuMemFree            = sym(h, "cuMemFree_v2");
    p_cuMemcpyHtoD         = sym(h, "cuMemcpyHtoD_v2");
    p_cuMemcpyDtoH         = sym(h, "cuMemcpyDtoH_v2");
    p_cuLaunchKernel       = sym(h, "cuLaunchKernel");

    CK(p_cuInit(0));
    int drv = 0, ndev = 0, ccmaj = 0, ccmin = 0;
    CK(p_cuDriverGetVersion(&drv));
    CK(p_cuDeviceGetCount(&ndev));
    CUdevice dev = 0;
    CK(p_cuDeviceGet(&dev, 0));
    char name[128] = {0};
    CK(p_cuDeviceGetName(name, (int)sizeof name, dev));
    CK(p_cuDeviceGetAttribute(&ccmaj, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR, dev));
    CK(p_cuDeviceGetAttribute(&ccmin, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR, dev));
    size_t total = 0;
    CK(p_cuDeviceTotalMem(&total, dev));
    printf("cuDriverGetVersion=%d devices=%d device0=\"%s\" compute=%d.%d mem=%llu MiB\n",
           drv, ndev, name, ccmaj, ccmin, (unsigned long long)(total >> 20));

    CUcontext ctx = NULL;
    CK(p_cuCtxCreate(&ctx, 0, dev));

    // Read the PTX text (must be NUL-terminated for cuModuleLoadData*).
    FILE *f = fopen(ptx_path, "rb");
    if (!f) { fprintf(stderr, "FAIL open %s\n", ptx_path); return 2; }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *ptx = malloc((size_t)len + 1);
    if (!ptx || fread(ptx, 1, (size_t)len, f) != (size_t)len) { fprintf(stderr, "FAIL read %s\n", ptx_path); return 2; }
    ptx[len] = 0;
    fclose(f);
    char ver[64] = "?", tgt[64] = "?";
    for (char *l = ptx; l && *l; l = strchr(l, '\n') ? strchr(l, '\n') + 1 : NULL) {
        if (!strncmp(l, ".version", 8)) sscanf(l, ".version %63s", ver);
        if (!strncmp(l, ".target", 7))  sscanf(l, ".target %63[^\r\n]", tgt);
    }
    printf("ptx file=%s bytes=%ld .version=%s .target=%s\n", ptx_path, len, ver, tgt);

    static char info_log[8192], err_log[8192];
    int   opts[4] = { CU_JIT_INFO_LOG_BUFFER, CU_JIT_INFO_LOG_BUFFER_SIZE_BYTES,
                      CU_JIT_ERROR_LOG_BUFFER, CU_JIT_ERROR_LOG_BUFFER_SIZE_BYTES };
    void *vals[4] = { info_log, (void *)(uintptr_t)sizeof info_log,
                      err_log,  (void *)(uintptr_t)sizeof err_log };
    CUmodule mod = NULL;
    double t0 = qpc_us();
    CUresult lr = p_cuModuleLoadDataEx(&mod, ptx, 4, opts, vals);
    double t1 = qpc_us();
    if (lr != 0) {
        fprintf(stderr, "JIT error log:\n%s\n", err_log);
        die_cu("cuModuleLoadDataEx", lr);
    }
    printf("cuModuleLoadDataEx OK (driver JIT of PTX text) in %.0f us (noisy, parallel load)\n", t1 - t0);
    if (info_log[0]) printf("JIT info log: %s\n", info_log);
    if (argc > 2 && !strcmp(argv[2], "probe")) {   // load-only mode: does the driver accept this PTX?
        printf("PROBE: ACCEPTED\n");
        return 0;
    }

    CUfunction k_hist = NULL, k_empty = NULL;
    CK(p_cuModuleGetFunction(&k_hist, mod, "hist_kernel"));
    CK(p_cuModuleGetFunction(&k_empty, mod, "empty_kernel"));

    // CPU reference.
    static uint32_t cpu[BINS], gpu[3][BINS], zero[BINS];
    for (uint32_t i = 0; i < N; i++) cpu[hash32(i) & (BINS - 1)]++;
    uint64_t cpu_hash = fnv1a64(cpu, sizeof cpu);
    printf("cpu  histogram: N=%d bins=%d fnv1a64=%016llx\n", N, BINS, (unsigned long long)cpu_hash);

    CUdeviceptr d_hist = 0;
    CK(p_cuMemAlloc(&d_hist, sizeof cpu));

    // Three GPU runs: two identical launches, then one with a different block size.
    const unsigned block[3] = { 256, 256, 1024 };
    int all_ok = 1;
    for (int run = 0; run < 3; run++) {
        unsigned n = N, bins = BINS;
        unsigned grid = (N + block[run] - 1) / block[run];
        void *params[3] = { &d_hist, &n, &bins };
        CK(p_cuMemcpyHtoD(d_hist, zero, sizeof zero));
        double a = qpc_us();
        CK(p_cuLaunchKernel(k_hist, grid, 1, 1, block[run], 1, 1, 0, NULL, params, NULL));
        CK(p_cuCtxSynchronize());
        double b = qpc_us();
        CK(p_cuMemcpyDtoH(gpu[run], d_hist, sizeof gpu[run]));
        uint64_t sum = 0;
        for (int i = 0; i < BINS; i++) sum += gpu[run][i];
        int same = memcmp(gpu[run], cpu, sizeof cpu) == 0;
        all_ok &= same && sum == N;
        printf("gpu run %d: grid=%u block=%u sum=%llu fnv1a64=%016llx memcmp_vs_cpu=%s  launch+sync %.0f us (noisy, parallel load)\n",
               run + 1, grid, block[run], (unsigned long long)sum,
               (unsigned long long)fnv1a64(gpu[run], sizeof gpu[run]),
               same ? "IDENTICAL" : "DIFFERENT", b - a);
    }
    int rr = memcmp(gpu[0], gpu[1], sizeof gpu[0]) == 0;
    all_ok &= rr;
    printf("gpu run 1 vs run 2: %s\n", rr ? "IDENTICAL" : "DIFFERENT");
    printf("bins[0..7] cpu: %u %u %u %u %u %u %u %u\n", cpu[0], cpu[1], cpu[2], cpu[3], cpu[4], cpu[5], cpu[6], cpu[7]);
    printf("bins[0..7] gpu: %u %u %u %u %u %u %u %u\n", gpu[0][0], gpu[0][1], gpu[0][2], gpu[0][3], gpu[0][4], gpu[0][5], gpu[0][6], gpu[0][7]);

    // Lesion control (not part of PASS/FAIL): non-atomic increment, if the module has it.
    CUfunction k_racy = NULL;
    if (p_cuModuleGetFunction(&k_racy, mod, "hist_kernel_racy") == 0) {
        static uint32_t racy[BINS];
        unsigned n = N, bins = BINS;
        void *params[3] = { &d_hist, &n, &bins };
        CK(p_cuMemcpyHtoD(d_hist, zero, sizeof zero));
        CK(p_cuLaunchKernel(k_racy, (N + 255) / 256, 1, 1, 256, 1, 1, 0, NULL, params, NULL));
        CK(p_cuCtxSynchronize());
        CK(p_cuMemcpyDtoH(racy, d_hist, sizeof racy));
        uint64_t sum = 0;
        for (int i = 0; i < BINS; i++) sum += racy[i];
        printf("lesion (non-atomic increment): sum=%llu of %d, lost=%lld, memcmp_vs_cpu=%s\n",
               (unsigned long long)sum, N, (long long)N - (long long)sum,
               memcmp(racy, cpu, sizeof cpu) == 0 ? "IDENTICAL" : "DIFFERENT");
    }

    // Empty kernel: launch + synchronise, one thread, timed per iteration.
    enum { WARM = 200, ITERS = 2000 };
    static double us[ITERS];
    for (int i = 0; i < WARM; i++) {
        CK(p_cuLaunchKernel(k_empty, 1, 1, 1, 1, 1, 1, 0, NULL, NULL, NULL));
        CK(p_cuCtxSynchronize());
    }
    double sumus = 0;
    for (int i = 0; i < ITERS; i++) {
        double a = qpc_us();
        CK(p_cuLaunchKernel(k_empty, 1, 1, 1, 1, 1, 1, 0, NULL, NULL, NULL));
        CK(p_cuCtxSynchronize());
        us[i] = qpc_us() - a;
        sumus += us[i];
    }
    qsort(us, ITERS, sizeof us[0], cmp_double);
    printf("empty kernel launch+sync over %d iters: min %.1f us, median %.1f us, mean %.1f us, p99 %.1f us, max %.1f us (noisy, parallel load)\n",
           ITERS, us[0], us[ITERS / 2], sumus / ITERS, us[ITERS * 99 / 100], us[ITERS - 1]);

    CK(p_cuMemFree(d_hist));
    CK(p_cuModuleUnload(mod));
    CK(p_cuCtxDestroy(ctx));
    free(ptx);
    FreeLibrary(h);
    printf("RESULT: %s\n", all_ok ? "PASS" : "FAIL");
    return all_ok ? 0 : 1;
}
