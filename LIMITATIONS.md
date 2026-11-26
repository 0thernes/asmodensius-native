# Limitations, Threats to Validity, and Real-vs-Metaphor Boundary

## 1. Real-vs-Metaphor Boundary (Law 8)

`Asmodensius-Native` uses biological terms (`creature`, `genome`, `brain`, `metabolism`, `predation`, `colony`, `species`, `evolution`) strictly as labels for deterministic integer data structures and state-transition rules in C23.

- **No Consciousness, Sentience, or Life Claim:** This engine does NOT claim consciousness, sentience, moral patienthood, biological life, or artificial general intelligence.
- **Indicator-Only Evaluations:** The `butlin14` catalog (`docs/eval/MECHANISMS.md`) tracks mechanistic architectural indicators inspired by Butlin et al. (2023) without asserting that meeting any indicator implies phenomenal experience. There is no aggregate "consciousness score."
- **No Quantum Physics or Quantum Speedup:** `core/tk_qsv.c` and `core/tk_qgt.c` are purely classical fixed-point linear-algebra routines operating on small state vectors and geometric tensors on an ordinary x86-64 CPU. They provide zero quantum speedup and zero physical quantum randomness.

## 2. Formal-to-Implementation Verification Gap

- The 30 Pure Lean 4.35.0 theorems (`lean/AsmodensiusProofs/`) mechanically prove algebraic conservation laws, boundary refusals, ABI alignment constants, Philox key-folding equivalence (`uid < 2^32`), and 32 GiB VRAM offset injectivity over mathematical `Nat` and abstract state transitions.
- Lean does **not** verify GCC/Clang C23 compilation, NASM AVX2/AVX-VNNI machine-code emission, or NVIDIA CUDA driver PTX JIT compilation. That implementation gap is bounded empirically -- not eliminated formally -- via `77/77` CTest, `ubsan_trap_build`, `core_obj_gate`, `avx_stack_gate`, and `164/164` `test_gpu.exe`.
- Five legacy theorems (`A06`, `A13`, `A15`, `A16`, `A20`) are retained for backward compatibility as constant/arithmetic sanity checks rather than deep invariants; see [docs/statement-fidelity.md](docs/statement-fidelity.md) for the exact per-theorem classification.

## 3. Measured Hardware Tradeoffs and Negative Results

- **1M-Creature GPU PCIe/Compaction Bottleneck:** Although `--gpu 1` executes 4,999,522 sparse brains at 1M founders with 0 fallbacks and bit-exact hash `31e5d00c106ce679`, synchronizing variable-size dirty store blocks across PCIe after two-half compaction yields `1,122.495 ms/tick`, which is slower than 24-core AVX-VNNI CPU execution (`757.988 ms/tick`). Moving the entire world state into VRAM remains deferred (`TR-05`).
- **Windows 2 MB Large Pages (`TR-03`):** `--large-pages` and `AZ_WIN_LARGE_PAGES` are wired on `az_run` and `az_live`, but standard non-elevated Windows tokens lack `SeLockMemoryPrivilege`, so runs fall back cleanly to 4 KiB `VirtualLock` (`large_pages=0`).
- **Single-Host Windows Target (Law 10):** The native host (`platform/win/az_host_win.c`) relies on Windows APIs (`VirtualLock`, `WaitOnAddress`, `BCryptGenRandom`, `GetSystemCpuSetInformation`). Linux CI validates the 6-kernel Lean proof suite, Tectonic PDF, live citations, and repository standard compliance, while running the C23/PTX simulation requires Windows x86-64 (and an NVIDIA GPU for `--gpu 1` / `test_gpu`).

## 4. Scientific and Long-Horizon Limitations

- **Open-Ended Evolution Not Demonstrated:** Development lesion trials (`seeds 1-24`, 64 ticks) verified that all 23 lesion switches alter state hashes and 8 passed prespecified primary thresholds, but long-horizon open-ended evolutionary dynamics (`AI-045`, `BIO-008`) remain unestablished.
- **Unburned Confirmatory Seeds:** Reporting blocks `5001-5024`, `6001-6024`, and `6101-6124` remain reserved pending frozen preregistered trials (`docs/eval/SEEDS.md`, `docs/preregistration.md`).
- **Open Trigger Register:** 12 capabilities remain intentionally deferred in [docs/TRIGGER-REGISTER.md](docs/TRIGGER-REGISTER.md) (`TR-02`, `TR-03`, `TR-05`, `TR-06`, `TR-08`..`TR-12`, `TR-14`..`TR-17`).
- **Mega-Atlas Non-Retirement:** `0 / 1,900` overarching problems in `unsolved-problems-mega-atlas` are solved or retired.
