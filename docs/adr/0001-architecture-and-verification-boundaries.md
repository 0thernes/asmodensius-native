# ADR-0001: Freestanding C23 Core, Multi-Substrate Bit-Exact Parity, and 6-Kernel Lean 4.35.0 Verification Boundaries

- **Date:** 2026-11-25
- **Status:** Accepted
- **Author:** Alexander Stephen Donahue

## Context

Artificial-life simulations frequently suffer from silent floating-point drift across thread counts and accelerators, non-reproducible memory allocation behavior, unverified biological "theater" mechanisms, and informal correctness claims. `Asmodensius-Native` must scale to 1,000,000+ organisms on a single Windows workstation while maintaining exact matter/energy conservation (Law 5), bit-exact lab replay (Law 6), and honest epistemic boundaries (Law 8).

## Decision

1. **Freestanding Integer-Only Core (`core/`):** Compile all 22 core modules and 4 `tk_*` mathematical kernels with ISO C23 `-ffreestanding`, zero libc, zero heap, zero floating-point operations, zero writable globals, and stack frames `<= 2048 B`, enforced automatically by `core_obj_gate`, `avx_stack_gate`, and `ubsan_trap_build`.
2. **Single-Source Multi-Substrate Twins (`kernels/`, `gpu/`):** Every accelerator (NASM AVX2/AVX-VNNI twins and Clang 21 `nvptx64` PTX compiled directly from `core/az_brain.c`, `core/az_rng.c`, `core/az_kin_axis.c`) must produce the bit-identical 64-bit `az_state_hash` of the scalar C reference, provide an off switch (`--kernels 0|1`, `--gpu 0|1`, `--prefetch 0|1`), and disclose negative scaling tradeoffs honestly (e.g., 1M GPU PCIe compaction overhead vs 24C CPU).
3. **Separated 6-Kernel Lean 4.35.0 Formal Layer (`lean/`):** Formalize core algebraic, refusal, ABI, Philox-key, and VRAM-offset invariants (`A01`..`A30`) in pure Lean 4.35.0 with Palomar `module` headers, isolated `Challenge.lean`/`Solution.lean`, concrete non-vacuity inhabitants (`A27`, `A28`), and adversarial guard-necessity refutations (`A29`, `A30`), checked by 6 checkers locally on Windows and, in Ubuntu CI, also by `lake check --paranoid` (adding `lean4lean`) and the `bubblewrap` comparator. As of the 2026-11-26 statement audit, 13 of the 30 are statements about the `Spec.lean` model and 17 are definitional, constant or literal-arithmetic checks (see `docs/statement-fidelity.md`).
4. **Explicit Verification Boundary:** Never conflate Lean proof of mathematical invariants with formal compiler verification of C23/NASM/PTX binaries, and never claim cognitive indicators (`butlin14`) imply sentience or retire Mega-Atlas open problems (`0 / 1,900` retired).

## Consequences

- **Positive:** Bit-exact cross-substrate determinism (`7bd9823448dd38b1`, `21e4cfcdf0b08066`, `5b8110b324d2603f`, `31e5d00c106ce679`), `77/77` CTest, `164/164` `test_gpu`, and `30/30` multi-kernel Lean proofs with transparent failure boundaries.
- **Tradeoffs:** Native execution is scoped to Windows x86-64 on `Z:` (Law 10); moving the full world into GPU VRAM remains deferred (`TR-05`); Windows 2 MB large pages await host `SeLockMemoryPrivilege` (`TR-03`).
