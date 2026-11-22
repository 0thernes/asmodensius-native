// kernel_c.c - route B: the same kernels as plain C (not CUDA C++), compiled with
//   clang --target=nvptx64-nvidia-cuda -mcpu=sm_80 -O2 -S kernel_c.c -o kernel_c.ptx
// The kernel annotation clang needs in plain C is __attribute__((KATTR)),
// where KATTR is passed on the command line (-DKATTR=nvptx_kernel or device_kernel).

#ifndef KATTR
#define KATTR nvptx_kernel
#endif
#define KERNEL __attribute__((KATTR))

static inline __attribute__((always_inline)) unsigned int hash32(unsigned int x) {
    x ^= x >> 16;
    x *= 0x85ebca6bu;
    x ^= x >> 13;
    x *= 0xc2b2ae35u;
    x ^= x >> 16;
    return x;
}

KERNEL void hist_kernel(unsigned int *hist, unsigned int n, unsigned int bins) {
    unsigned int i = (unsigned int)__nvvm_read_ptx_sreg_ctaid_x() *
                     (unsigned int)__nvvm_read_ptx_sreg_ntid_x() +
                     (unsigned int)__nvvm_read_ptx_sreg_tid_x();
    if (i < n) {
        unsigned int b = hash32(i) & (bins - 1u);
        __atomic_fetch_add(&hist[b], 1u, __ATOMIC_RELAXED);
    }
}

KERNEL void empty_kernel(void) {}
