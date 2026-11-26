# Glossary (`docs/glossary.md`)

- **ABI v4 (`AZ_ABI_VERSION = 4`):** The frozen C23 binary interface in `core/az_abi.h` defining `AZ_SENS = 64`, `AZ_MOT = 24`, `AZ_TRAITS = 112`, `AZ_PHEN = 16`, `AZ_UNIT = 64`, `AZ_CHUNK = 128`, and 23 named `AZ_LESION_*` bits.
- **Arena (`AzArena`):** The single pre-allocated, Windows `VirtualLock`-pinned contiguous memory region holding all simulation state with zero heap allocations after creation and zero pagefile backing.
- **AVX-VNNI Twin (`kernels/az_brain_vnni.asm`):** Hand-assembled NASM x86-64 kernel emitting raw `vpdpbusd` bytes for exact integer `u8 x i8 -> i32` neural dot products bit-identical to `core/az_brain.c`.
- **AZ_CHUNK (`128`):** The 128-entity Structure-of-Arrays (SoA) cache block processed as one unit in hot tick loops and software prefetch spans (`TR-01`).
- **AZ_UNIT (`64` bytes):** One cache-line granule in the two-half compacting variable-size creature store (`core/az_store.c`).
- **Exact Books (Law 5):** Exact integer equality (`==`, no tolerance) of `az_total_matter(w)` and `az_total_energy(w)` after every tick, with `field_underflow == 0` and `dropped_events == 0`.
- **Indicator-Only (Law 8):** Reporting policy under which cognitive metrics (such as `butlin14`) measure mechanistic architectural properties without claiming consciousness, sentience, biological life, or AGI.
- **Lab Mode (`--lab`):** Seeded deterministic execution mode using counter-based Philox4x32-10 and canonical UID tie-breaking for bit-exact replay across threads, reorder periods, CPU twins, and GPU PTX.
- **Lesion (`AZ_LESION_*`):** One of 23 named data-driven switches in `AzConfig.lesion` disabling a single biological or cognitive mechanism on the same code path to verify non-theatrical causal effect (Law 4).
- **Palomar 6-Kernel Verification:** Lean 4.35.0 verification discipline separating pure definitions (`Spec.lean`), isolated statement-only `Challenge.lean`, and sorry-free `Solution.lean` checked locally by the Lean reference kernel (`lean4`, `leanchecker`, `leanchecker-paranoid`) and the independent checkers `nanoda_bin`, `con-leche` and `con-ron`, and in Ubuntu CI also by `lean4lean` (`lake check --paranoid`) and `lake comparator`.
- **Philox4x32-10:** Counter-based parallel PRNG (Salmon et al. 2011) keyed by world seed and indexed by `(uid, tick, stream, idx)` so parallel workers obtain order-independent draws (`TR-13`).
- **PTX Twin (`gpu/az_gpu_dev.c`):** Pure-integer NVIDIA PTX module (`41,817 B`, 0 externs, 0 floats) compiled by Clang 21 directly from freestanding `core/az_brain.c`, `core/az_rng.c`, and `core/az_kin_axis.c` (`TR-18`).
- **Trigger Register (`docs/TRIGGER-REGISTER.md`):** Explicit ledger (`TR-01`..`TR-18`) governing deferred capabilities and the measurable gates required before each capability is opened or closed.
- **Wild Mode (Default):** Unseeded default execution drawing fresh OS entropy (`BCryptGenRandom`) every tick so no two wild worlds repeat (Law 6).
