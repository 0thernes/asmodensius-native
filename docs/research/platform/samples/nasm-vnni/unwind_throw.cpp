// unwind_throw.cpp - a C++ exception thrown from a callback travels through two
// NASM frames (clobber_all, then abi_check) to a catch in main.  The g++
// SEH unwinder (RtlUnwindEx) needs the .pdata/.xdata records from ENDPROC to
// cross those frames and to restore main's callee-saved registers.
//   g++ -O2 unwind_throw.cpp kern.obj -o unwind_throw.exe
#include <cstdint>
#include <cstdio>
extern "C" {
uint32_t abi_check(void (*fn)(void));
void clobber_all(void);
void clobber_even(void);
extern void (*clobber_callback)(void);
}
static void thrower() { throw 42; }

int main(int argc, char **)
{
    // values that g++ may keep in callee-saved registers across the try block
    volatile uint64_t seed = 0x0123456789ABCDEFull + (unsigned)argc;
    uint64_t a = seed * 3, b = seed * 5, c = seed * 7, d = seed * 11, e = seed * 13, f = seed * 17;
    double p = (double)(seed & 1023) * 1.5, q = (double)(seed & 511) * 2.5, r = (double)(seed & 255) * 3.5;
    int caught = 0, bad = 0;
    clobber_callback = thrower;
    for (int round = 0; round < 1000; round++) {
        try {
            abi_check((round & 1) ? clobber_even : clobber_all);
            bad |= 1;                                   // must not return normally
        } catch (int v) {
            caught += (v == 42);
        }
        uint64_t s = seed;
        bad |= (a != s * 3) | (b != s * 5) | (c != s * 7) | (d != s * 11) | (e != s * 13) | (f != s * 17);
        bad |= (p != (double)(s & 1023) * 1.5) | (q != (double)(s & 511) * 2.5) | (r != (double)(s & 255) * 3.5);
    }
    clobber_callback = nullptr;
    std::printf("c++ throw through 2 NASM frames: caught %d of 1000, locals intact: %s  %s\n",
                caught, bad ? "no" : "yes", (caught == 1000 && !bad) ? "PASS" : "FAIL");
    return !(caught == 1000 && !bad);
}
