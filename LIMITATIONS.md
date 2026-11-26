# Limitations, Threats to Validity, and Real-vs-Metaphor Boundary

## 1. Real-vs-Metaphor Boundary (Law 8)

`Asmodensius-Native` uses biological terms (`creature`, `genome`, `brain`, `metabolism`, `predation`, `colony`, `species`, `evolution`) strictly as labels for deterministic integer data structures and state-transition rules in C23.

- **No Consciousness, Sentience, or Life Claim:** This engine does NOT claim consciousness, sentience, moral patienthood, biological life, or artificial general intelligence.
- **Indicator-Only Evaluations:** The `butlin14` catalog (`docs/eval/MECHANISMS.md`) tracks mechanistic architectural indicators inspired by Butlin et al. (2023) without asserting that meeting any indicator implies phenomenal experience. There is no aggregate "consciousness score."
- **No Quantum Physics or Quantum Speedup:** `core/tk_qsv.c` and `core/tk_qgt.c` are purely classical fixed-point linear-algebra routines operating on small state vectors and geometric tensors on an ordinary x86-64 CPU. They provide zero quantum speedup and zero physical quantum randomness.

## 2. Formal-to-Implementation Verification Gap

- The 30 Lean 4.35.0 theorems (`lean/AsmodensiusProofs/`) are about hand-written `Nat` definitions in `Spec.lean`. Nothing links those definitions to the C23, NASM or PTX code except constants copied by hand from `core/az_abi.h`.
- 13 of the 30 are statements about the model definitions: bounded transfers preserve totals (`A01`..`A03`), the xor key fold for `uid < 2^32` and its failure at `uid = 2^32` (`A17`, `A30`), the capped receive (`A21`), the six-face flux bound (`A22`), the `NUT_X` conversion balance (`A23`), the lesion mask (`A24`), 8-byte offset injectivity below 32 GiB (`A26`), two non-vacuity witnesses (`A27`, `A28`) and the species refusal at 101 (`A29`).
- Lean does **not** verify GCC/Clang C23 compilation, NASM AVX2/AVX-VNNI machine-code emission, or NVIDIA CUDA driver PTX JIT compilation. That implementation gap is bounded empirically -- not eliminated formally -- via `77/77` CTest, `ubsan_trap_build`, `core_obj_gate`, `avx_stack_gate`, and `164/164` `test_gpu.exe`.
- The other 17 are not invariants of the engine. `A04`, `A05` hold because the record update never touches the field; `A06`, `A13`, `A14`, `A25` are constant identities; `A15`, `A16`, `A20` are literal arithmetic (for example `1 + 1 = 2`); `A07`..`A11` are generic `Nat` facts that do not mention the species, arena or store definitions (`A09` concludes its two hypotheses); `A12` and `A19` are `x = x` and an identity function; `A18` assumes `stepGpu = stepCpu` and concludes equal hashes. Their names (for example `A15_dual_visual_worlds_verified`) overstate what they prove. See [docs/statement-fidelity.md](docs/statement-fidelity.md).
- `A22` formalizes documented Rule C7 (each face flux at most `amt / 8`). The transport code at the audited commit (`core/az_chem.c`, `spread()`) instead caps each species' total outflow per pass at `a / 4` with a decreasing budget, so `A22` is not a statement about that code.
- The Law 14 text checker (`build/work/scripts/check_text.py`) is not tracked in this repository and no CI step runs it. Earlier "0 violations" statements cannot be reproduced from the repository; the project's own handoff notes record files with non-ASCII, CR or tab bytes.
- Several CI steps are report-only (`continue-on-error: true`): the Build Standard self-audit, the live citation check, SLSA provenance attestation and the gitleaks scan. A green CI run does not establish their results.

## 3. Measured Hardware Tradeoffs and Negative Results

- **1M-Creature GPU PCIe/Compaction Bottleneck:** Although `--gpu 1` executes 4,999,522 sparse brains at 1M founders with 0 fallbacks and bit-exact hash `31e5d00c106ce679`, synchronizing variable-size dirty store blocks across PCIe after two-half compaction yields `1,122.495 ms/tick`, which is slower than 24-core AVX-VNNI CPU execution (`757.988 ms/tick`). Moving the entire world state into VRAM remains deferred (`TR-05`).
- **Windows 2 MB Large Pages (`TR-03`):** `--large-pages` and `AZ_WIN_LARGE_PAGES` are wired on `az_run` and `az_live`, but standard non-elevated Windows tokens lack `SeLockMemoryPrivilege`, so runs fall back cleanly to 4 KiB `VirtualLock` (`large_pages=0`).
- **Single-Host Windows Target (Law 10):** The native host (`platform/win/az_host_win.c`) relies on Windows APIs (`VirtualLock`, `WaitOnAddress`, `BCryptGenRandom`, `GetSystemCpuSetInformation`). Linux CI validates the 6-kernel Lean proof suite, Tectonic PDF, live citations, and repository standard compliance, while running the C23/PTX simulation requires Windows x86-64 (and an NVIDIA GPU for `--gpu 1` / `test_gpu`).

## 4. Scientific and Long-Horizon Limitations

- **Open-Ended Evolution Not Demonstrated:** Development lesion trials (`seeds 1-24`, 64 ticks) verified that all 23 lesion switches alter state hashes and 8 passed prespecified primary thresholds, but long-horizon open-ended evolutionary dynamics (`AI-045`, `BIO-008`) remain unestablished.
- **Unburned Confirmatory Seeds:** Reporting blocks `5001-5024`, `6001-6024`, and `6101-6124` remain reserved pending frozen preregistered trials (`docs/eval/SEEDS.md`, `docs/preregistration.md`).
- **Open Trigger Register:** 12 capabilities remain intentionally deferred in [docs/TRIGGER-REGISTER.md](docs/TRIGGER-REGISTER.md) (`TR-02`, `TR-03`, `TR-05`, `TR-06`, `TR-08`..`TR-12`, `TR-14`..`TR-17`).
- **Mega-Atlas Non-Retirement:** `0 / 1,900` overarching problems in `unsolved-problems-mega-atlas` are solved or retired.
