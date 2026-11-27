# Limitations, Threats to Validity, and Real-vs-Metaphor Boundary

## 1. Real-vs-Metaphor Boundary (Law 8)

`Asmodensius-Native` uses biological terms (`creature`, `genome`, `brain`, `metabolism`, `predation`, `colony`, `species`, `evolution`) strictly as labels for deterministic integer data structures and state-transition rules in C23.

- **No Consciousness, Sentience, or Life Claim:** This engine does NOT claim consciousness, sentience, moral patienthood, biological life, or artificial general intelligence.
- **Indicator-Only Evaluations:** The `butlin14` catalog (`docs/eval/MECHANISMS.md`) tracks mechanistic architectural indicators inspired by Butlin et al. (2023) without asserting that meeting any indicator implies phenomenal experience. There is no aggregate "consciousness score."
- **No Quantum Physics or Quantum Speedup:** `core/tk_qsv.c` and `core/tk_qgt.c` are purely classical fixed-point linear-algebra routines operating on small state vectors and geometric tensors on an ordinary x86-64 CPU. They provide zero quantum speedup and zero physical quantum randomness.
- **Not Peer-Reviewed:** The white paper (`dist/Donahue_2026_asmodensius_native.pdf`) is an independent technical preprint and has not undergone formal peer review.

## 2. Formal-to-Implementation Verification Gap

- All 38 Lean 4.35.0 declarations (`lean/AsmodensiusProofs/`) are about hand-written `Nat` definitions in `Spec.lean`. Nothing links those definitions to the C23, NASM, or PTX code except constants and transition models transcribed by hand from `core/az_abi.h`, `core/az_world.c`, `core/az_store.c`, `core/az_worldgen.c`, and `core/az_chem.c`.
- **Honest 26 vs 12 Separation (v1.2.0):**
  - `26` of the `38` declarations are substantive model, boundary, non-vacuity witness, or adversarial false-variant refutation statements (`A01`..`A03`, `A07`..`A11`, `A17`, `A21`..`A24`, `A26`..`A38`), including `A31`/`A32`/`A38` modeling the implemented `core/az_chem.c` `spread()` `a / 4` decreasing-budget loop and `A22_documented_rule_c7_six_face_bound` modeling documented Rule C7 (`amt / 8`) from `docs/ARCHITECTURE.md`.
  - `12` of the `38` declarations are explicitly labeled definitional, constant, congruence, or legacy arithmetic sanity checks (`A04`, `A05` field-untouched record updates; `A06`, `A13`, `A14`, `A25` ABI constant identities; `A12_sanity_readonly_hash_refl`, `A15_sanity_legacy_arith_two`, `A16_sanity_legacy_sum_240`, `A18_sanity_congruence_twin_hash`, `A19_sanity_identity_prefetch_hash`, `A20_sanity_legacy_sum_244`). None of these 12 is claimed as a proof of C observer invariance, dual visual worlds, GPU/CPU hardware parity, or x86-64 `PREFETCHT0` semantics. See [docs/statement-fidelity.md](docs/statement-fidelity.md).
- Lean does **not** verify GCC/Clang C23 compilation, NASM AVX2/AVX-VNNI machine-code emission, or NVIDIA CUDA driver PTX JIT compilation. That implementation gap is bounded empirically -- not eliminated formally -- via CTest, `ubsan_trap_build`, `core_obj_gate`, `avx_stack_gate`, and `164/164` `test_gpu.exe`.
- **Law 14 Text Hygiene Scope:** Tracked `tools/check_text.py` enforces 0 violations (7-bit ASCII, LF, no tabs except `.tsv` and `scripts/audit-repo.sh`, trailing newline) across all active tracked files in CI, while transparently counting and reporting `2` exempt verbatim owner prompt files (`docs/handoff/PROMPTS.md`, `docs/handoff/PROMPTS.xml`) and historical archival dumps (`docs/requirements/`, `docs/research/`, `tests/upstream/`) that contain UTF-8, CR, or tab bytes.
- **Report-Only CI Steps:** In `.github/workflows/ci.yml`, the Build Standard self-audit, live Crossref/arXiv network check, gitleaks secret scan, and non-tag SLSA provenance attestation are `continue-on-error: true` (report-only per Build Standard R7), so a green CI badge does not by itself enforce those report-only outputs. Hard-failing CI gates enforce the 38-declaration Lean build, `VerifyAxioms.lean` diff, `leanchecker`, `leanchecker-paranoid`, `lake check --paranoid` (`lean4lean`), `lake comparator --paranoid`, `scripts/test_lean_negative.py`, `scripts/check_readme_axioms.py`, `scripts/check_blueprint.py`, `tools/check_text.py`, `scripts/build_paper.py`, `sha256sum -c CHECKSUMS.sha256`, and `scripts/stamp_timestamps.py --verify`.

## 3. Measured Hardware Tradeoffs and Negative Results

- **1M-Creature GPU PCIe/Compaction Bottleneck:** Although `--gpu 1` executes 4,999,522 sparse brains at 1M founders with 0 fallbacks and bit-exact hash `31e5d00c106ce679`, synchronizing variable-size dirty store blocks across PCIe after two-half compaction yields `1,122.495 ms/tick`, which is slower than 24-core AVX-VNNI CPU execution (`757.988 ms/tick`). Moving the entire world state into VRAM remains deferred (`TR-05`).
- **Windows 2 MB Large Pages (`TR-03`):** `--large-pages` and `AZ_WIN_LARGE_PAGES` are wired on `az_run` and `az_live`, but standard non-elevated Windows tokens lack `SeLockMemoryPrivilege`, so runs fall back cleanly to 4 KiB `VirtualLock` (`large_pages=0`).
- **Windows VirtualAlloc Commit Pressure:** Large locked-arena CTest targets (`test_grid` allocating `1.84 GB`, `test_metab` allocating `372 MB`) require uncontended Windows virtual commit headroom; under heavy parallel workstation memory load `VirtualAlloc` can fail with Win32 error `1455` (`ERROR_COMMITMENT_LIMIT`).
- **Single-Host Windows Target (Law 10):** The native host (`platform/win/az_host_win.c`) relies on Windows APIs (`VirtualLock`, `WaitOnAddress`, `BCryptGenRandom`, `GetSystemCpuSetInformation`). Ubuntu CI validates the Lean proof suite (`lean4lean`, `lake check --paranoid`, `lake comparator`, `test_lean_negative.py`), Tectonic PDF, citation integrity, active Law-14 text hygiene, interactive blueprint, checksums, timestamps, and Build Standard compliance, while running the C23/PTX simulation requires Windows x86-64 (and an NVIDIA GPU for `--gpu 1` / `test_gpu`).

## 4. Scientific and Long-Horizon Limitations

- **Open-Ended Evolution Not Demonstrated:** Development lesion trials (`seeds 1-24`, 64 ticks) verified that all 23 lesion switches alter state hashes and 8 passed prespecified primary thresholds, but long-horizon open-ended evolutionary dynamics (`AI-045`, `BIO-008`) remain unestablished.
- **Unburned Confirmatory Seeds:** Reporting blocks `5001-5024`, `6001-6024`, and `6101-6124` remain reserved pending frozen preregistered trials (`docs/eval/SEEDS.md`, `docs/preregistration.md`).
- **Open Trigger Register:** 12 capabilities remain intentionally deferred in [docs/TRIGGER-REGISTER.md](docs/TRIGGER-REGISTER.md) (`TR-02`, `TR-03`, `TR-05`, `TR-06`, `TR-08`..`TR-12`, `TR-14`..`TR-17`).
- **Mega-Atlas Non-Retirement:** `0 / 1,900` overarching problems in `unsolved-problems-mega-atlas` are solved or retired.
