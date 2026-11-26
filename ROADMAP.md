# Roadmap (`ROADMAP.md`)

Future engineering and scientific milestones are governed by the Trigger Register ([docs/TRIGGER-REGISTER.md](docs/TRIGGER-REGISTER.md)) and the seed-allocation discipline ([docs/eval/SEEDS.md](docs/eval/SEEDS.md)).

## Completed (v1.0.0 - v1.1.0)

- [x] 22-module freestanding C23 ABI v4 core + 4 `tk_*` integer kernels (`77/77` CTest, UBSan trap build, `core_obj_gate`, `avx_stack_gate`).
- [x] Closed `TR-01` (DDR5 streaming software prefetch), `TR-04` (live `--gpu 0|1` sparse brain path), `TR-07` (24C `8P+16E` placement table), `TR-13` (bulk Philox AVX2 live), and `TR-18` (41,817 B pure-integer Clang 21 CUDA PTX, `164/164` `test_gpu`).
- [x] Upgraded Lean 4.35.0 formalization to 30 theorems (`A01`..`A30`) with 6-kernel Palomar `Challenge`/`Solution` isolation and non-vacuity/adversarial witnesses.
- [x] Published AMS-LaTeX white paper (`dist/Donahue_2026_asmodensius_native.pdf`), clean-room `dist/arxiv_submission.tar.gz`, 16 live-verified references, and Build Standard v1.2.0 scaffolding.

## Near-Term (v1.2.0)

- [ ] Complete remaining `WP-P4-06` staged social/weather/tier1 mechanism dispositions (`docs/eval/MECHANISMS.md`).
- [ ] Freeze SHA-256-pinned preregistration for `M5` / `CHAIN-01` ecological trials before touching reserved seeds `6001-6024` (`docs/preregistration.md`).
- [ ] Evaluate `TR-03` (Windows 2 MB large pages) once host `SeLockMemoryPrivilege` is granted by the owner, measuring 1M-creature tick latency delta against 4 KiB `VirtualLock`.
- [ ] Execute `TR-08` reproduce-first audit and `E1`-`E3` re-run across all active substrates.

## Long-Term (Trigger-Gated)

- [ ] `TR-05`: GPU-resident world state and deterministic CPU+GPU split to eliminate PCIe compaction overhead at 1M+ populations.
- [ ] `TR-02` & `TR-14`: In-place creature store compaction (`<= 1.25x` live blocks) enabling locked populations above 1,000,000 creatures.
- [ ] `TR-06`: Bit-exactness probe for Intel AI Boost NPU (owner approval required before any non-C/NASM/PTX result may touch world state).
- [ ] `TR-17`: Parallel child construction during birth commit while preserving canonical serial allocation order.
