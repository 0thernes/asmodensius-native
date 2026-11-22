// kernel.cu - device code for the cuda-ptx sample.
// Compiled to PTX text by clang (no CUDA toolkit, no CUDA headers):
//   clang -x cuda -nocudainc -nocudalib --cuda-gpu-arch=sm_80 --cuda-device-only -O2 -S kernel.cu -o kernel.ptx
// Integer work only. No floating point anywhere.

#define KERNEL extern "C" __attribute__((global))
#define DEVICE static inline __attribute__((device, always_inline))

// murmur3 32-bit finaliser (public domain, Austin Appleby). Bijective on uint32.
DEVICE unsigned int hash32(unsigned int x) {
    x ^= x >> 16;
    x *= 0x85ebca6bu;
    x ^= x >> 13;
    x *= 0xc2b2ae35u;
    x ^= x >> 16;
    return x;
}

// One thread per element. Global index = blockIdx.x * blockDim.x + threadIdx.x.
// Each thread hashes its index and atomically adds 1 into a (bins) -bin histogram.
// bins must be a power of two.
KERNEL void hist_kernel(unsigned int *hist, unsigned int n, unsigned int bins) {
    unsigned int i = (unsigned int)__nvvm_read_ptx_sreg_ctaid_x() *
                     (unsigned int)__nvvm_read_ptx_sreg_ntid_x() +
                     (unsigned int)__nvvm_read_ptx_sreg_tid_x();
    if (i < n) {
        unsigned int b = hash32(i) & (bins - 1u);
        __atomic_fetch_add(&hist[b], 1u, __ATOMIC_RELAXED);
    }
}

// Lesion control: the same kernel with a plain (non-atomic) increment.
// Expected to LOSE counts under contention; shows the atomic is doing real work.
KERNEL void hist_kernel_racy(unsigned int *hist, unsigned int n, unsigned int bins) {
    unsigned int i = (unsigned int)__nvvm_read_ptx_sreg_ctaid_x() *
                     (unsigned int)__nvvm_read_ptx_sreg_ntid_x() +
                     (unsigned int)__nvvm_read_ptx_sreg_tid_x();
    if (i < n) {
        unsigned int b = hash32(i) & (bins - 1u);
        hist[b] = hist[b] + 1u;
    }
}

// Empty kernel, used only to time launch + synchronise overhead.
KERNEL void empty_kernel(void) {}
