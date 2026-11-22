# Bare-metal build path: a UEFI x64 probe (`BOOTX64.EFI`)

Key: `uefi`. Date: 2026-11-17. Everything lived in
`N:\docs\research\platform\samples\uefi\`.

> Dropped by the owner's rule L6 (2026-11-17): no boot images, nothing without
> an operating system, no USB boots; the engine runs only as a Windows program
> from Z:. This note stays as a record only. The samples are parked, unbuilt,
> in `N:\build\dropped-uefi-2026-11-17\research-samples-uefi\`, and every
> built `.efi` image was deleted.

## Bottom line

- A 9216-byte freestanding `BOOTX64.EFI` **builds** on this machine with
  clang 21.1.8 + lld-link + nasm, with no external headers and no libraries.
- It was **not booted**. There is no QEMU and no firmware emulator here, and
  nothing was written to a USB stick.
- The strongest thing that could be done instead was done: the **real image
  file** was mapped into a Windows process the way a firmware loader maps it
  (sections, base relocations, entry RVA), called with a fake system table, and
  it ran to completion. Its three privileged instructions faulted in ring 3 and
  were emulated, which recorded exactly what the image asks the CPU for:
  `CR4 0x668 -> 0x40668` (OSXSAVE set), then `XSETBV(0, 0x7)`.
- This machine has **Secure Boot enabled** (registry read, below), so the
  unsigned image will be refused until that is dealt with.

## Files

| File | What |
|---|---|
| `uefi_min.h` | UEFI x64 types written by hand (system table, boot services, text in/out, GOP), with 38 `_Static_assert` checks on offsets and sizes |
| `bootx64.c` | the application: `efi_main`, gradient, AVX enable, AVX2 loop, RDTSC timing, key wait. `-DHOST_MOCK` compiles the privileged steps out |
| `cpu.asm` | NASM win64: `cpu_rdtsc`, `cpu_cpuid`, `cpu_xgetbv`, and the privileged `cpu_read_cr4`, `cpu_write_cr4`, `cpu_xsetbv` |
| `host_mock.c` | fake firmware for Windows; two modes (`linked`, `image`); writes BMPs |
| `verify_image.py` | static checks through `llvm-objdump` / `llvm-readobj` (dev tooling) |
| `build.cmd` | builds everything; `build.cmd test` also runs all checks |
| `usb\EFI\BOOT\BOOTX64.EFI` | the staged tree, ready to copy to a stick |
| `out\*.bmp`, `out\*.png` | framebuffers written by the mock |
| `build\test_log.txt` | full output of the last `build.cmd test` |

## What `efi_main` does

1. Disarms the firmware's boot watchdog (`SetWatchdogTimer(0,...)`), prints a
   banner and the firmware vendor string through `ConOut->OutputString`.
2. `LocateProtocol` for the Graphics Output Protocol GUID
   `9042a9de-23dc-4a38-96fb-7aded080516a`; prints the mode; fills the linear
   framebuffer with a gradient (R left to right, G top to bottom, B falling
   along the diagonal). Handles BGR and RGB 32-bit formats and honours
   `PixelsPerScanLine`. Bit-mask and Blt-only modes are reported and skipped.
3. Enables AVX: CPUID checks (XSAVE, AVX, AVX2), then `CR4 |= OSXSAVE`
   (bit 18; OSFXSR and OSXMMEXCPT are kept set), then
   `XSETBV(0, XCR0 | x87 | SSE | AVX)`, then reads XCR0 back. CR4 comes first
   because XGETBV/XSETBV raise #UD while CR4.OSXSAVE is clear.
4. Runs an integer kernel (`x = x*1664525 + 1013904223; x ^= x >> 15`) over a
   16 KiB block of `u32` for 20000 passes, once as AVX2 (8 lanes per YMM,
   compiler vector types, no `immintrin.h`) and once as scalar code, inside
   `RaiseTPL(TPL_HIGH_LEVEL)` / `RestoreTPL`. Prints both checksums and
   `MATCH` / `MISMATCH`.
5. Times both with `LFENCE; RDTSC`, and prints the TSC ticks across a
   100 ms `Stall` as a rough calibration.
6. Waits for a key (`WaitForEvent` on `ConIn->WaitForKey`, then
   `ReadKeyStroke`) and returns `EFI_SUCCESS`.

Only `kernel_avx2` is compiled for AVX2 (`__attribute__((target("avx2")))`).
The rest of the image is baseline x86-64, because it runs before AVX is on.

## Build (ran now)

Command: `cmd /c .\build.cmd test` in the sample folder. The three build steps
it runs:

```
nasm -f win64 cpu.asm -o build\cpu.obj
Z:\Tools\LLVM-21\bin\clang.exe --target=x86_64-unknown-windows -std=c2x -O2 -Wall -Wextra
    -ffreestanding -fshort-wchar -mno-red-zone -mno-stack-arg-probe -fno-stack-protector
    -nostdlib -c bootx64.c -o build\bootx64.obj
Z:\Tools\LLVM-21\bin\lld-link.exe -entry:efi_main -subsystem:efi_application -nodefaultlib
    -Brepro -map:build\BOOTX64.map -out:build\BOOTX64.EFI build\bootx64.obj build\cpu.obj
```

Output:

```
[1/5] nasm cpu.asm
[2/5] clang bootx64.c (freestanding, x86_64-unknown-windows)
[3/5] lld-link BOOTX64.EFI
[4/5] stage usb\EFI\BOOT\BOOTX64.EFI
[5/5] gcc host_mock.exe (bootx64.c with -DHOST_MOCK)
BUILD OK
```

No warnings from clang or gcc with `-Wall -Wextra`. clang worked, so the zig
fallback was not tried. With `-Brepro` two consecutive builds gave the same
hash (ran `sha256sum` before and after a rebuild):

```
da1516ea4b8b2dcbd3bf0c54ec221e96e89411b86c7982d8d5df2c3a9fd84870 *build/BOOTX64.EFI
```

## Static verification (ran now)

`Z:\Tools\LLVM-21\bin\llvm-objdump.exe -p usb\EFI\BOOT\BOOTX64.EFI` (selected lines):

```
usb/EFI/BOOT/BOOTX64.EFI:	file format coff-x86-64
Characteristics 0x22
	executable
	large address aware
Magic                   020b	(PE32+)
AddressOfEntryPoint     0000000000001260
ImageBase               0000000140000000
SectionAlignment        00001000
FileAlignment           00000200
SizeOfImage             00009000
Subsystem               0000000a	(EFI application)
DllCharacteristics      00008160
					HIGH_ENTROPY_VA
					DYNAMIC_BASE
					NX_COMPAT
Entry 1 0000000000000000 00000000 Import Directory [parts of .idata]
Entry 5 0000000000008000 00000014 Base Relocation Directory [.reloc]
```

So: machine x86-64, subsystem EFI application, import table absent (RVA 0,
size 0). `python verify_image.py build\BOOTX64.EFI build\bootx64.obj` tail:

```
  ok    llvm-readobj: Machine IMAGE_FILE_MACHINE_AMD64
  ok    llvm-readobj: IMAGE_SUBSYSTEM_EFI_APPLICATION
  ok    llvm-readobj --coff-imports: no imports listed
  ok    no section is writable and executable
  ok    image contains MOV RAX, CR4
  ok    image contains MOV CR4, RCX
  ok    image contains XSETBV
  ok    image contains XGETBV
  ok    image contains RDTSC
  ok    image contains CPUID
  ok    image contains VPMULLD (AVX2 integer multiply)
  ok    image contains VZEROUPPER
  ok    VPMULLD uses YMM registers
  functions in bootx64.obj: memset, memcpy, efi_main, kernel_avx2, kernel_scalar
  ok    kernel_avx2 has 25 VEX instructions
  ok    no VEX instruction outside kernel_avx2
RESULT static: PASS
```

Two things found and fixed on the way:

- The first link had **no `.reloc` section** (all code was RIP-relative, zero
  base relocations). Some firmware loaders are said to want one before they
  load an image away from its preferred base; that claim is from memory and
  was not checked. The image now has a pointer table (`g_avx_reason`) that it
  really uses, which gives 5 `DIR64` relocations and a `.reloc` section.
- The struct layouts were cross-checked against an independent source that is
  already on this machine: zig 0.17's `lib/std/os/uefi`. Boot services: 45 of
  45 fields in the same order (`diff` of the two name lists was empty). System
  table, GOP, GOP mode, GOP mode info, pixel-format enum, GOP GUID, text
  in/out protocols and the three status codes were compared by eye against the
  zig files and agree.

## Host mock (ran now)

`build\host_mock.exe linked out` calls the `efi_main` compiled into the exe
from the same `bootx64.c` with `-DHOST_MOCK` (gcc on PATH). The fake system
table's GOP points at a `malloc` buffer pre-filled with `0xDEADBEEF`. Five
scenarios: BGR 640x480, RGB 800x600 with a 832-pixel stride, BGR 1x1,
Blt-only, and no GOP at all. Per scenario it checks: return status; every
visible pixel against the mock's own statement of the gradient; scan-line
padding and 64 guard words untouched; AVX2 and scalar checksums against the
mock's own scalar reference (`0xC0C55D33`); one `LocateProtocol`, one 100 ms
`Stall`, one watchdog disarm, one key wait; TPL raised and restored; no boot
service called above `TPL_APPLICATION`. Tail:

```
== scenario no_gop (linked) ==
    | Asmodensius-Native UEFI probe: hello from efi_main
    | firmware vendor: MockFirmware  revision 0x00010000
    | GOP not found, status 0x800000000000000E
    | AVX enable: HOST_MOCK build, CR4 and XSETBV skipped
    | CR4 0x0000000000000000 -> 0x0000000000000000  XCR0 0x0000000000000007 -> 0x0000000000000007
    | AVX state: ok
    | TSC ticks per 100 ms stall: 308221213
    | AVX2   sum 0xC0C55D33 ticks 11205182
    | scalar sum 0xC0C55D33 ticks 56625053
    | checksum: MATCH
    | press any key to return to the firmware
    scenario PASS
RESULT linked: 91 checks, 0 failed
```

BMPs written: `out\linked_bgr_640x480.bmp`, `out\linked_rgb_800x600_stride832.bmp`,
`out\linked_bgr_1x1.bmp`. `out\image_bgr_640x480.png` was looked at: blue top
left, red top right, green bottom left, yellow bottom right, smooth.

### Beyond the brief: the real image, run in ring 3

`build\host_mock.exe image usb\EFI\BOOT\BOOTX64.EFI out` does not recompile
anything. It reads the PE32+ file, maps its sections, applies the base
relocations, marks `.text` read+execute, and calls the entry point with the
same fake system table. `MOV RAX,CR4`, `MOV CR4,RCX` and `XSETBV` fault in
user mode; a vectored exception handler recognises the three opcodes inside
the image, emulates them against a fake CR4 that starts at `0x668` (OSXSAVE
clear), and counts them. Tail:

```
loaded usb\EFI\BOOT\BOOTX64.EFI
  machine 0x8664 subsystem 10 preferred base 0x140000000 size 0x9000 entry rva 0x1260
  mapped at 00000180C3F10000 (delta +1647186083840), DIR64 relocations applied: 5, import table size: 0
...
== scenario no_gop (image) ==
    | Asmodensius-Native UEFI probe: hello from efi_main
    | firmware vendor: MockFirmware  revision 0x00010000
    | GOP not found, status 0x800000000000000E
    | CR4 0x0000000000000668 -> 0x0000000000040668  XCR0 0x0000000000000007 -> 0x0000000000000007
    | AVX state: ok
    | TSC ticks per 100 ms stall: 348682839
    | AVX2   sum 0xC0C55D33 ticks 13509185
    | scalar sum 0xC0C55D33 ticks 62412397
    | checksum: MATCH
    | press any key to return to the firmware
    [emu] CR4 0x668 -> 0x40668 (reads 2, writes 1); XSETBV(0, 0x7) x1; fault code 0xc0000005
    scenario PASS
RESULT image: 116 checks, 0 failed

ALL OK
```

The BMPs from the real image are byte-identical to the ones from the
`HOST_MOCK` build (`cmp` on the 640x480 and 800x600 pairs: identical).
Windows reported the faults as `0xC0000005`, not `0xC0000096`; the handler
keys on the opcode bytes, not on the code.

### Do the checks have teeth? (ran now)

Two deliberate bugs, each behind a macro, each built with
`gcc -std=c2x -O2 -Wall -Wextra -DHOST_MOCK -DHAVE_LINKED_EFI_MAIN -D<LESION> bootx64.c host_mock.c build/cpu.obj -o <exe>`
and run as `<exe> linked <dir>`:

```
-DLESION_STRIDE  (ignore PixelsPerScanLine)
    FAIL: 479200 visible pixels differ from the gradient
    FAIL: 18432 scan-line padding words were overwritten
    FAIL: guard words after the framebuffer were overwritten
RESULT linked: 91 checks, 3 failed          (only the stride-832 scenario fails)

-DLESION_AVX     (shift by 14 instead of 15 in the AVX2 kernel only)
    | AVX2   sum 0xDFA77000 ticks 11866781
    | scalar sum 0xC0C55D33 ticks 56501731
    | checksum: MISMATCH
RESULT linked: 91 checks, 10 failed
```

### Timing (noisy, parallel load; ring 3 under Windows, not firmware)

81.92 million word updates (4096 words x 20000 passes). Across the ten runs
in `build\test_log.txt`: AVX2 11.0 to 16.3 million TSC ticks, scalar 55.0 to
77.1 million. That is roughly 0.13 to 0.20 ticks per word for AVX2 and 0.67
to 0.94 for scalar. The "ticks per 100 ms" line (3.08e8 to 3.53e8)
comes from `Sleep(100)` in the mock and is only a sanity value. None of these
numbers say anything about speed under real firmware.

## USB layout (not done here)

One FAT32 partition on the stick, and exactly this tree:

```
<stick root>\
└── EFI\
    └── BOOT\
        └── BOOTX64.EFI      9216 bytes, sha256 da1516ea...fd84870
```

`\EFI\BOOT\BOOTX64.EFI` is the default removable-media boot path for x64 UEFI;
the firmware finds it without any boot entry. The tree is already staged at
`N:\docs\research\platform\samples\uefi\usb\`, so with the stick as `E:`
(check the letter; formatting erases the stick):

```
format E: /FS:FAT32 /Q
xcopy /E /I N:\docs\research\platform\samples\uefi\usb\EFI E:\EFI
```

Then boot the stick from the firmware boot menu, or from Windows through
Settings > System > Recovery > Advanced startup > Use a device. Notes, all
from memory and not checked today: the firmware is only required to read FAT
(not exFAT or NTFS); Windows' `format` refuses FAT32 on volumes above 32 GB,
so a large stick needs a smaller partition; the boot-menu key for this laptop
is unknown to me.

Expected screen if it boots: the gradient, with the text lines shown above
drawn over its top-left corner, real CR4 and XCR0 values, `checksum: MATCH`,
then a wait for a key and a return to the firmware.

## Secure Boot caveat

`reg query "HKLM\SYSTEM\CurrentControlSet\Control\SecureBoot\State" /v UEFISecureBootEnabled`
returned `0x1` today: **Secure Boot is on**. `BOOTX64.EFI` is unsigned, so the
firmware will refuse to start it. The choices:

1. Turn Secure Boot off in firmware setup for the test, then turn it back on.
   On Windows 11 with BitLocker or device encryption this is expected to
   trigger a BitLocker recovery-key prompt at the next Windows boot unless
   protection is suspended first. Have the recovery key before touching the
   setting. Whether BitLocker is active on this machine is unknown (the status
   query needs elevation and was not run).
2. Sign the image with a key of your own and enrol that key in the firmware
   (`db`), or go through shim and a Machine Owner Key. Not tried; no signing
   tool was looked for.

Both points are general knowledge, not something verified on this laptop.

## Verified / not verified

Verified now, on this machine:

- the image builds from the three source files with the listed flags
- PE32+ headers: x86-64, EFI application subsystem, no import table,
  5 base relocations, no writable+executable section
- AVX instructions are confined to `kernel_avx2`; the privileged opcodes are
  present with the encodings `0F 20 E0`, `0F 22 E1`, `0F 01 D1`
- the real image relocates to a different base and runs end to end against a
  fake firmware in ring 3, calling the services in a legal order
- gradient pixels, stride handling, AVX2 = scalar = independent reference
- the image requests `CR4 = old | OSXSAVE` and then `XSETBV(0, 0x7)`, in that order
- table layouts agree with zig's std UEFI definitions

Not verified:

- **booting**: on real firmware, in QEMU/OVMF, or anywhere outside a Windows
  process
- that a CPU in firmware context accepts the real CR4 write and XSETBV (they
  were emulated, never executed)
- every real firmware call: `OutputString`, `LocateProtocol`, `Stall`,
  `RaiseTPL`, `WaitForEvent` were only ever answered by my own mock, which
  shares my reading of the specification
- that the BGR/RGB byte order looks right on a real panel
- firmware behaviour after return with CR4.OSXSAVE and XCR0 left changed (the
  program does not restore them)
- how this laptop's firmware treats YMM state in its interrupt handlers; the
  measured loop runs at `TPL_HIGH_LEVEL` and ends with `VZEROUPPER`, which is
  the precaution, not a proof
- the USB procedure and both Secure Boot routes

## What this means for the project (reasoning, not measurement)

A UEFI application gets ring 0, no OS and no pagefile, which fits the
cache-resident law. It also starts on one core with no GPU driver: the other
23 cores need the MP services protocol or hand-written AP start-up, and the
RTX 5070 Ti is reachable only as a GOP framebuffer, so CUDA is out. A
bare-metal build would be a CPU-only simulation with software rendering. The
toolchain side of that path is proven here; the hardware side is not.
