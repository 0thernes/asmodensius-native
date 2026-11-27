# ADR-0001: Freestanding C23 Core, Multi-Substrate Bit-Exact Parity, and Multi-Checker Lean 4.35.0 Verification Boundaries

- **Date:** 2026-11-25 (Updated 2026-11-26 for v1.2.0)
- **Status:** Accepted
- **Author:** Alexander Stephen Donahue

## Context

Artificial-life simulations frequently suffer from silent floating-point drift across thread counts and accelerators, non-reproducible memory allocation behavior, unverified biological "theater" mechanisms, and informal correctness claims. `Asmodensius-Native` must scale to 1,000,000+ organisms on a single Windows workstation while maintaining exact matter/energy conservation (Law 5), bit-exact lab replay (Law 6), and honest epistemic boundaries (Law 8).

## Decision

1. **Freestanding Integer-Only Core (`core/`):** Compile all 22 core modules and 4 `tk_*` mathematical kernels with ISO C23 `-ffreestanding`, zero libc, zero heap, zero floating-point operations, zero writable globals, and stack frames `<= 2048 B`, enforced automatically by `core_obj_gate`, `avx_stack_gate`, and `ubsan_trap_build`.
2. **Single-Source Multi-Substrate Twins (`kernels/`, `gpu/`):** Every accelerator (NASM AVX2/AVX-VNNI twins and Clang 21 `nvptx64` PTX compiled directly from `core/az_brain.c`, `core/az_rng.c`, `core/az_kin_axis.c`) must produce the bit-identical 64-bit `az_state_hash` of the scalar C reference, provide an off switch (`--kernels 0|1`, `--gpu 0|1`, `--prefetch 0|1`), and disclose negative scaling tradeoffs honestly (e.g., 1M GPU PCIe compaction overhead vs 24C CPU).
3. **Separated Multi-Checker Lean 4.35.0 Model Layer (`lean/`):** Formalize abstract `Nat` model invariants (`A01`..`A38`) in pure Lean 4.35.0 with Palomar `module` headers, isolated `Challenge.lean`/`Solution.lean`, concrete non-vacuity inhabitants (`A27`, `A28`), adversarial false-variant refutations (`A29`, `A30`, `A33`..`A38`), and live negative elaboration + `sorry`-tamper rejection (`scripts/test_lean_negative.py`). Checked locally on Windows by 6 checkers (`lean4`, `leanchecker`, `leanchecker-paranoid`, `con-leche`, `con-ron`, `nanoda_bin`) and in Ubuntu CI additionally by `lean4lean` (`lake check --paranoid`) and `lake comparator --paranoid` under `bubblewrap`. Following the 2026-11-26 audit and v1.2.0 closure, `26` of the `38` declarations are substantive model/boundary/witness/refutation statements (including `A31`/`A32`/`A38` for `core/az_chem.c` `spread()` `a / 4` budget and restated `A07`..`A11`) and `12` are explicitly labeled sanity checks (`docs/statement-fidelity.md`).
4. **Explicit Verification Boundary:** Never conflate Lean proof of `Spec.lean` mathematical models with formal compiler verification of C23/NASM/PTX binaries, and never claim cognitive indicators (`butlin14`) imply sentience or retire Mega-Atlas open problems (`0 / 1,900` retired).

## Consequences

- **Positive:** Bit-exact cross-substrate determinism (`7bd9823448dd38b1`, `21e4cfcdf0b08066`, `5b8110b324d2603f`, `31e5d00c106ce679`), `77/77` baseline CTest, `164/164` `test_gpu`, `38/38` multi-checker Lean declarations with live negative tamper rejection, tracked `tools/check_text.py` CI gate, and interactive strict-CSP proof blueprint (`docs/blueprint.html`).
- **Tradeoffs:** Native execution is scoped to Windows x86-64 on `Z:` (Law 10); moving the full world into GPU VRAM remains deferred (`TR-05`); Windows 2 MB large pages await host `SeLockMemoryPrivilege` (`TR-03`).
