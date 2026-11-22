# Fact-check: the bare-metal claims in the two GrokBot exhibits

Agent key: `baremetal`. Date: 2026-11-17. Web research plus read-only local
measurements; nothing was built into the project and nothing was booted.
Exhibits: `grokbot_message_1.md` (M1), `grokbot_message_2.md` (M2). They are
quoted as data.

Verdict scale: correct / mostly_correct / misleading / wrong / unverifiable.

## Summary

The bare-metal route M2 describes (UEFI application, `ExitBootServices`, GOP
framebuffer, INIT-SIPI-SIPI) is a real and workable technique, and M1 is right
that it gives up the NVIDIA GPU. What is oversold is the benefit: "zero jitter",
"100% deterministic", "no TLB misses" and the fixed framebuffer address are not
true as written, and three blocking prerequisites are never mentioned (Secure
Boot is ON on this laptop, AVX state must be enabled by the program itself, and
APs start in real mode and need a trampoline). M2 section 4 ("honest
trade-offs") qualifies hand-written ASM, O(N^2) queries and GPU-vs-CPU; it
contains no qualification of any bare-metal claim. Bare metal also buys nothing
for the owner's law 6: bit-exact replay comes from integer state and a
counter-based RNG, on any OS.

## Verdict table

| # | Claim (where) | Verdict |
|---|---|---|
| 1 | Write a UEFI x86-64 `.efi` application and call `ExitBootServices()`; put it on USB as `BOOTX64.EFI` (M2 s3.1, s5.3) | correct |
| 2 | "the framebuffer (`0xE0000000` GOP linear memory)" (M2 s1 table) | misleading |
| 3 | GOP gives a raw pointer; "writes RGBA pixels directly into VRAM at hardware refresh speed" (M2 s3.2) | mostly_correct |
| 4 | Wake the other P-cores with INIT-SIPI-SIPI over the Local APIC (M2 s3.3) | mostly_correct |
| 5 | "no interrupts", "Zero OS jitter", "100% deterministic control over the 275HX" (M2 s1, s3; M1 s2) | misleading |
| 6 | "no virtual memory TLB misses" (M2 s3) | wrong |
| 7 | The AVX2 kernel runs on bare metal after the three listed steps (M2 s3, implied; AVX enabling never mentioned) | misleading (omission) |
| 8 | Bare metal "loses Blackwell GPU (requires proprietary NVIDIA GSP driver)" (M1 s2) | mostly_correct |
| 9 | "boot directly from a USB stick on your laptop" (M2 s5.3; Secure Boot never mentioned) | misleading (omission) |
| 10 | FPGA / RTL row: "no CPU ... no ISA. Neurons are physical logic gates and wires. Signals propagate at near-light speed" (M2 s1) | misleading |
| 11 | "CPU boots straight into your binary. You own 100% of physical RAM, L1/L2/L3 caches" (M2 s1) | mostly_correct |
| 12 | "Cores 0-7 (8 Lion Cove P-Cores) ... Cores 8-23 (16 Skymont E-Cores)" (M1 s4.3) | wrong on this machine |

## Evidence per claim

### 1. UEFI application + ExitBootServices + BOOTX64.EFI: correct

- UEFI 2.10 s7.4.6: "The ExitBootServices() function is called by the currently
  executing UEFI OS loader image to terminate all boot services." If the MapKey
  is stale it returns `EFI_INVALID_PARAMETER` and the GetMemoryMap/Exit pair
  must be repeated. Afterwards "No further calls to boot service functions or
  EFI device-handle-based protocols may be used"; the boot-services watchdog is
  disabled. Runtime services (for example `ResetSystem`) remain.
  https://uefi.org/specs/UEFI/2.10/07_Services_Boot_Services.html
- UEFI 2.10 s3.5: for removable media the firmware appends the default file name
  `\EFI\BOOT\BOOT{machine type}.EFI`; for x64 that is `BOOTx64.EFI`. The boot
  manager arms a 5-minute watchdog before `StartImage()` and disarms it if the
  image returns. https://uefi.org/specs/UEFI/2.10/03_Boot_Manager.html
- Note: the firmware already hands over in 64-bit long mode (source in claim 6),
  so "booting ... into 64-bit Long Mode" needs no mode switch by the program.

### 2. Framebuffer at 0xE0000000: misleading

The GOP framebuffer address is not a constant. It is read at run time from
`EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE.FrameBufferBase` ("Base address of graphics
linear frame buffer"), it lives inside whatever PCI BAR the firmware assigned to
the active GPU, and a mode may have no linear framebuffer at all
(`PixelBltOnly`: "implies that a linear frame buffer is not available for this
mode"). https://uefi.org/specs/UEFI/2.10/12_Protocols_Console_Support.html

Fairness: the same message, section 3 item 2, says the correct thing ("query the
GOP ... to grab the raw physical pointer"). The literal in the table is an
example presented as a fact.

Local measurement (read-only, under Windows; the firmware's boot-time
assignment can differ from what Windows shows):

```
PS> Get-CimInstance Win32_PnPEntity | ? PNPClass -eq 'Display' | % { ...
      Get-CimAssociatedInstance -ResultClassName Win32_DeviceMemoryAddress ... }
== Intel(R) Graphics
   mem 0x00B420000000 - 0x00B420FFFFFF  (16 MiB)
   mem 0x00A410000000 - 0x00A41FFFFFFF  (256 MiB)
== NVIDIA GeForce RTX 5070 Ti Laptop GPU
   mem 0x00008C000000 - 0x00008FFFFFFF  (64 MiB)
   mem 0x00A400000000 - 0x00A401FFFFFF  (32 MiB)
```

No display BAR on this laptop is at `0xE0000000`; the Intel iGPU's 256 MiB
graphics-memory BAR is above 4 GiB. `Win32_VideoController` reports the
2560x1600 mode on the Intel adapter and none on the NVIDIA adapter, so the
internal panel is currently driven by the iGPU; which GPU provides GOP at boot
is a firmware setting and was not tested.

### 3. Direct pixel writes: mostly_correct

Direct writes are exactly what GOP is for: "The linear address of the hardware
frame buffer is also exposed so software can write directly to the video
hardware." (same URL as claim 2). Caveats the exhibit omits: the pixel order is
whatever `PixelFormat` says and may be BGR
(`PixelBlueGreenRedReserved8BitPerColor`), not "RGBA"; `PixelsPerScanLine` can
exceed the visible width; the GOP chapter defines no vertical-sync or page-flip
service, so "at hardware refresh speed" means tearing, not synced frames (this
last point is an inference from the absence of such a service in the chapter).
On an iGPU the "VRAM" is ordinary system RAM.

### 4. INIT-SIPI-SIPI: mostly_correct

- It is the right mechanism after `ExitBootServices`. The alternative,
  `EFI_MP_SERVICES_PROTOCOL` (`StartupAllAPs`, `StartupThisAP`), is a PI-spec
  protocol and "is available only during boot time"; it cannot run a tick loop
  that outlives boot services. https://dox.ipxe.org/MpService_8h.html
  (EDK2 `MpService.h`, PI 1.2 Volume 2). Using MP Services while staying inside
  boot services is the lower-effort way to try multi-core code first.
- Omitted work: an AP that receives a SIPI starts in real-address mode at
  physical `000VV000H` (below 1 MiB), so the program must place a 16-bit
  trampoline there and walk the AP through protected mode into long mode with
  its own stack, GDT and page tables; interrupt sources must be inhibited during
  the sequence. Intel SDM Vol. 3A, "Multiple-Processor (MP) Initialization"
  (section 8.4 in older editions, renumbered since):
  https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html
  (landing page; the quoted details were read through search-result excerpts of
  the manual, not from Intel's PDF in this session).
- "your other 7 Lion Cove P-cores" cannot be addressed as IDs 1..7. Measured
  x2APIC IDs of the eight P-cores here: 0, 8, 32, 40, 48, 56, 80, 88 (see claim
  12). A bare-metal program has to read the ACPI MADT and CPUID leaf 0x1A.
- `CLI` masks only maskable interrupts; NMI, machine-check and SMI still arrive
  (claim 5).
- Unknown: whether this firmware leaves the APIC in x2APIC mode (then the ICR is
  MSR 0x830, not the memory-mapped register). Not measurable from user mode.

### 5. "No interrupts, zero OS jitter, 100% deterministic": misleading

True part: with no OS there is no scheduler, no DPCs, no background services,
no pagefile. False part: the firmware and the hardware keep interrupting and
re-clocking the cores, and the program cannot switch that off.

- SMM/SMI. "SMIs are not serviced by the Linux kernel, which means that it does
  not even know that they are occurring. SMIs are instead set up by BIOS code
  and are serviced by BIOS code, usually for 'critical' events such as
  management of thermal sensors and fans. Sometimes though, SMIs are used for
  other tasks and those tasks can spend an inordinate amount of time in the
  handler". The detector works by spinning with interrupts disabled and looking
  for gaps in the TSC, which is precisely the M2 `CLI` loop.
  https://kernel.org/doc/html/v6.17/trace/hwlat_detector.html
  The Intel SDM describes the SMI as a nonmaskable external interrupt that
  operates independently of the normal interrupt mechanism and the local APIC,
  with priority over NMI (Vol. 3, "System Management Interrupt (SMI)"; wording
  seen via secondary quotation, e.g.
  https://www.nccgroup.com/research/enumerating-system-management-interrupts/).
  How often this laptop's firmware raises SMIs: unknown, not measured.
- Thermal throttling. Intel datasheets: automatic Thermal Control Circuit
  activation needs no software, steps frequency/voltage down and then modulates
  the clock; "Assertion of the PROCHOT# signal is independent of any register
  settings within the processor"; disabling automatic mode puts the processor
  out of specification. (Statements from several Core-family datasheets found by
  search; the Arrow Lake-HX datasheet itself was not opened, so the exact
  behaviour of this part is not confirmed.) Without an OS there is also no OS
  power/thermal policy at all, only the embedded controller and these hardware
  protections.
- Turbo. Core frequency under turbo depends on temperature and power limits, so
  wall-clock time per tick varies run to run. M1 section 5 itself concedes that
  the chassis "will thermally throttle ... within minutes", which contradicts
  "100% deterministic control" in M1 section 2.
- Result determinism is unaffected either way and is not a bare-metal benefit:
  integer state plus Philox gives the same hash under Windows.

### 6. "No virtual memory TLB misses": wrong

- UEFI 2.10 s2.3.4 (x64 platforms), state during boot services: "Long mode, in
  64-bit mode" and "Paging mode is enabled and any memory space defined by the
  UEFI memory map is identity mapped (virtual address equals physical
  address)". https://uefi.org/specs/UEFI/2.10/02_Overview.html
- Long mode cannot run without paging: IA-32e paging is in use when
  `CR0.PG = 1, CR4.PAE = 1, IA32_EFER.LME = 1`, and the processor sets
  `IA32_EFER.LMA = CR0.PG & IA32_EFER.LME`; clearing `CR0.PG` leaves 64-bit
  mode. Intel SDM Vol. 3A chapter 4 (Paging); landing URL as in claim 4, text
  seen via search excerpts.
- So every memory access is still translated through the TLB, and a miss still
  costs a page walk. What bare metal removes is page *faults* to disk and the
  OS's 4 KiB mappings; with your own page tables you can map the arena with
  1 GiB pages (measured: `CPUID.80000001:EDX Page1GB=1`). The same reduction is
  available under Windows with large pages, which needs
  `SeLockMemoryPrivilege` (not currently held, per COMMON.md).

### 7. AVX on bare metal: the enabling step is missing

- Requirement: AVX instructions raise #UD unless `CR4.OSXSAVE` (bit 18) is set
  and `XCR0[2:1] = 11b`, which the supervisor sets with `XSETBV`. Intel SDM
  Vol. 1 "Detection of AVX" (section 14.3 in many editions); quoted in
  https://doc.rust-lang.org/src/std_detect/detect/os/x86.rs.html and described
  with the #UD consequence in https://wiki.osdev.org/AVX.
- The UEFI handoff state promises only x87 control word `0x037F`, MXCSR
  `0x1F80`, `CR0.EM = 0`, `CR0.TS = 0`; it says nothing about `CR4` or AVX state
  (UEFI 2.10 s2.3.4, URL in claim 6). A UEFI application must therefore check
  and enable it itself; there are reports of exactly this #UD in EFI apps
  (https://f.osdev.org/viewtopic.php?p=351297).
- Each AP started by SIPI comes up with AVX disabled and needs the same
  sequence.
- Local measurement showing the OS, not the hardware, provides this today:

```
$ gcc -std=c2x -O2 -Wall -Wextra -o cpuprobe.exe cpuprobe.c   (no output)
$ ./cpuprobe.exe
CPUID.1:ECX XSAVE=1 OSXSAVE=1 AVX=1
XCR0=0x7  (bit1 SSE=1, bit2 AVX=1)
CPUID.7.0:EBX AVX2=1 AVX512F=0  EDX Hybrid=1
CPUID.80000001:EDX Page1GB=1 LongMode=1
```

  Whether this laptop's firmware happens to leave `OSXSAVE` set at handoff:
  unknown; stage A of the USB test below would show it.
- Also note `AVX512F=0`: M2's opening "AVX2/AVX-512" is half inapplicable here.

### 8. NVIDIA GPU without the vendor driver: mostly_correct

- NVIDIA driver README (580.119.02), "kernel open": "Blackwell and later are
  only supported by the open kernel modules" and "the open kernel modules
  depend on the GPU System Processor (GSP) first introduced in Turing".
  https://download.nvidia.com/XFree86/Linux-x86_64/580.119.02/README/kernel_open.html
- The open-gpu-kernel-modules repository states that its modules must be used
  with the GSP firmware and user-space components of the matching driver
  release. https://github.com/NVIDIA/open-gpu-kernel-modules
- Correction of wording: for Blackwell the kernel driver is open source
  (MIT/GPLv2); what is proprietary is the signed GSP firmware and the user-space
  stack that CUDA needs. The practical conclusion stands: a freestanding UEFI
  program would have to reimplement that driver stack to run any compute on the
  5070 Ti, so bare metal means CPU only. A GOP framebuffer from a GPU's option
  ROM is display only, not compute.

### 9. Booting an unsigned BOOTX64.EFI on this laptop: blocked until the owner acts

- Measured: Secure Boot is enabled here.

```
PS> Get-ItemProperty 'HKLM:\SYSTEM\CurrentControlSet\Control\SecureBoot\State'
UEFISecureBootEnabled : 1
```

- UEFI 2.10 chapter 32: "The signature database is checked when the UEFI Boot
  Manager is about to start a UEFI image"; an image with no certificate and no
  hash in `db` is not started (s32.4.2, s32.6.1). Authorisation is possible by
  hash: "The security database db must either contain an entry with a hash
  value of the image ..." (s32.6.3.3).
  https://uefi.org/specs/UEFI/2.10/32_Secure_Boot_and_Driver_Signing.html
- What the owner would have to do, in order of simplicity:
  1. Disable Secure Boot in firmware setup (Shift+Restart, Troubleshoot,
     Advanced options, UEFI Firmware Settings), test, re-enable. Microsoft
     warns: "After disabling Secure Boot and installing other software and
     hardware, you may need to restore your PC to the factory state to
     re-activate Secure Boot."
     https://learn.microsoft.com/en-us/windows-hardware/manufacture/desktop/disabling-secure-boot
  2. Keep Secure Boot on and enrol the image's SHA-256 hash or an own signing
     certificate in `db` through the firmware's key-management menu, if this
     firmware offers one (unknown). Every rebuild changes the hash, so a
     certificate is the only practical form; mistakes in key management can stop
     Windows from booting.
  3. Chain through `shim`, which validates the next image against a built-in or
     machine-owner key (https://github.com/rhboot/shim). More moving parts than
     a first experiment deserves.
- BitLocker interaction: BitLocker normally binds to PCR 7 (Secure Boot state);
  with Secure Boot off it falls back to other PCRs and recovery prompts become
  likely; Microsoft's guidance is `Suspend-BitLocker` before firmware changes.
  https://learn.microsoft.com/en-us/troubleshoot/windows-client/windows-security/bitlocker-recovery-known-issues
  Measured unelevated: the shell property `System.Volume.BitLockerProtection`
  for `C:` returned `2` (protection off). This is not authoritative; confirm
  with an elevated `manage-bde -status` before relying on it.

### 10. FPGA / RTL row: misleading

- "No CPU, no fetch-decode-execute, no ISA": true by definition of custom RTL.
- "Neurons are physical logic gates and wires": on an FPGA they are look-up
  tables, flip-flops and programmable routing switches, which is why FPGAs are
  slower and larger than the gates the sentence evokes.
- "near-light speed": on-chip delay is dominated by gate/LUT switching and
  resistive-capacitive routing. Measured gap at the same process node: FPGA
  critical paths about 3 to 4 times slower than a standard-cell ASIC, about 14
  times the dynamic power, 18 to 40 times the area for logic. Kuon and Rose,
  "Measuring the Gap Between FPGAs and ASICs", IEEE TCAD 26(2), 2007.
  https://www.eecg.utoronto.ca/~jayar/pubs/kuon/kuontcad06.pdf
  The win of hardware is parallelism and fixed latency, not signal velocity.
- Not actionable here: there is no FPGA in the stated machine, and the project's
  languages are C23 and NASM.

### 11. "Boots straight into your binary; you own 100% of RAM": mostly_correct

After a successful `ExitBootServices` the image "implicitly owns all unused
memory in the map" (UEFI 2.10 s7.2, URL in claim 1). Not owned: runtime-services
code/data, ACPI NVS and reserved ranges, SMM memory, and the iGPU's stolen
memory. The firmware's own initialisation runs before the binary, and SMM code
keeps running after it (claim 5).

### 12. P-cores are not "cores 0-7" on this machine: wrong (measured)

```
$ ./cpuprobe.exe            (tail; source S:/factcheck/baremetal/cpuprobe.c)
logical processors: 24
lp  x2apic  coretype
 0       0  0x40 Core(P)
 1       8  0x40 Core(P)
 2      16  0x20 Atom(E)   ... lp 2-9: Atom(E), x2apic 16..30
10      32  0x40 Core(P)
11      40  0x40 Core(P)
12      48  0x40 Core(P)
13      56  0x40 Core(P)
14      64  0x20 Atom(E)   ... lp 14-21: Atom(E), x2apic 64..78
22      80  0x40 Core(P)
23      88  0x40 Core(P)
```

Under this Windows build the P-cores are logical processors 0, 1, 10, 11, 12,
13, 22, 23. An affinity mask of bits 0-7 would put six of eight "P-core"
threads on E-cores. Any pinning code must classify cores with CPUID leaf 0x1A
(or `GetLogicalProcessorInformationEx` efficiency classes), never by index.
The 24 distinct APIC IDs confirm each row really ran on a different processor.

## USB boot test: a short, safe procedure

Purpose: settle the three unknowns above (GOP address and pixel format, whether
firmware leaves AVX enabled, how much SMI jitter there is) with the smallest
possible exposure. It is an optional experiment; nothing in the project needs it.

Stage 0, no hardware exposure.
1. Boot the image in a virtual machine first. M2's own suggestion of QEMU is the
   right first step, but QEMU is not installed on this machine and agents may
   not install it; that is the owner's call. A VM cannot answer the SMI or
   real-GOP questions.
2. With Secure Boot still ON, boot the unsigned stick once from the one-time
   boot menu. Expected result: the firmware refuses it. This confirms claim 9
   and changes nothing.

Stage A, image that never leaves boot services (returns to the firmware).
1. Use a dedicated FAT32 USB stick containing only `\EFI\BOOT\BOOTX64.EFI`.
2. The image prints, through the console protocol: GOP mode, `PixelFormat`,
   `PixelsPerScanLine`, `FrameBufferBase`; `CR4` and, if `OSXSAVE` is set,
   `XCR0`; the number of processors from MP Services if that protocol exists.
   Then it waits for a key and returns `EFI_SUCCESS`.
3. It must not call `ExitBootServices`, `SetVariable`, any Block I/O or file
   write, any MSR write, or any port I/O.
4. In an elevated shell run `manage-bde -status`. If any volume is protected,
   note where the recovery key is and run `Suspend-BitLocker` first.
5. Shut Windows down fully, enter firmware setup, change only the Secure Boot
   enable switch (no key deletion, no "custom/setup mode"), boot the stick from
   the one-time boot menu (do not reorder boot priorities).
6. Photograph the screen, press a key, re-enable Secure Boot, boot Windows,
   check `UEFISecureBootEnabled` is 1 again, resume BitLocker if suspended.

Stage B, only if stage A was clean: leave boot services.
1. Same stick and firmware steps. The image calls `GetMemoryMap` +
   `ExitBootServices` (retry on stale key), enables AVX itself, fills the
   framebuffer, spins on the boot processor only with a TSC gap histogram for a
   fixed short time (tens of seconds), draws the histogram, then calls the
   runtime service `ResetSystem`. No AP start-up in the first run.
2. There is no keyboard after `ExitBootServices`; if it hangs, hold the power
   button.
3. Mains power, hard surface, short runs.

Risks, honestly stated.
- Secure Boot is off for the duration, and Microsoft warns that re-activating it
  can require restoring factory keys or factory state.
- If BitLocker is on and not suspended, expect a recovery prompt; without the
  recovery key the data is unreachable.
- Firmware variable writes are the one way a test image can do lasting harm:
  the Linux kernel made efivarfs entries immutable because deleting variables
  "is bricking some peoples' laptops" on firmware that stores needed data there
  (kernel commit "efi: Make efivarfs entries immutable by default", mirror:
  https://gem5.googlesource.com/arm/linux/+/ed8b0de5a33d2a2557dce7f9429dca8cb5bc5879).
  How this laptop's firmware behaves is unknown. Hence rule 3 of stage A.
- During boot services the internal NVMe is reachable through Block I/O; a bug
  that writes there destroys data. Hence no storage protocol use at all.
- After `ExitBootServices` there is no OS thermal or power policy. Hardware
  protection (throttling, thermal shutdown) remains, but fan behaviour depends
  on the embedded controller. Keep runs short, especially before trying all
  cores.
- The 5-minute boot watchdog is disabled by `ExitBootServices`; a hang will not
  reset itself.
- Changing anything else in firmware setup (keys, CSM, graphics mode) is out of
  scope and can stop Windows from starting.

## What was not verified

- Nothing was booted; every bare-metal statement about this laptop's firmware
  (GOP provider, framebuffer address at boot, AVX state at handoff, x2APIC mode,
  SMI rate, availability of MP Services or of a `db` enrolment menu) is unknown.
- Intel SDM passages were read through search-result excerpts and mirrors, not
  from Intel's PDF. The Arrow Lake-HX datasheet was not opened.
- The PI specification text for MP Services was read through the EDK2 header
  documentation, not from uefi.org (the guessed uefi.org URL returned 404).
- BitLocker state was read unelevated through a shell property only.

## Commands run in this session

- `Get-ItemProperty HKLM:\SYSTEM\CurrentControlSet\Control\SecureBoot\State` (output in claim 9)
- `(New-Object -ComObject Shell.Application).NameSpace('C:').Self.ExtendedProperty('System.Volume.BitLockerProtection')` gave `2`
- `Get-CimInstance Win32_VideoController` and the `Win32_DeviceMemoryAddress` association (output in claim 2)
- `gcc -std=c2x -O2 -Wall -Wextra -o cpuprobe.exe cpuprobe.c` then `./cpuprobe.exe` in `S:/factcheck/baremetal/` (output in claims 7 and 12)
