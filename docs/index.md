# Asmodensius-Native Documentation Index (`docs/index.md`)

## Architecture & Engineering Contracts

- [ARCHITECTURE.md](ARCHITECTURE.md) -- Canonical ABI v4 architecture, 7-stage tick pipeline, determinism rules D1-D8, and conservation rules C1-C7.
- [TRIGGER-REGISTER.md](TRIGGER-REGISTER.md) -- Deferred capability register (`TR-01`..`TR-18`), closed bare-metal compute milestones, and open gates.
- [adr/0001-architecture-and-verification-boundaries.md](adr/0001-architecture-and-verification-boundaries.md) -- Architecture Decision Record on freestanding C23, multi-substrate parity, and formal verification boundaries.
- [configuration.md](configuration.md) -- CLI flags, environment variables, and CMake build options.
- [STREAM.md](STREAM.md) -- Real-time viewer streaming protocol.

## Verification, Formal Proofs & Governance

- [statement-fidelity.md](statement-fidelity.md) -- Line-by-line Lean 4.35.0 theorem (`A01`..`A30`) to C23/PTX fidelity audit matrix.
- [verification.md](verification.md) -- Full verification stack (`77/77` CTest, `164/164` `test_gpu`, UBSan traps, 6-kernel Lean 4.35.0, Tectonic PDF).
- [MEGA_ATLAS_CROSSCHECK.md](MEGA_ATLAS_CROSSCHECK.md) -- Unsolved Problems Mega-Atlas non-retirement audit (`0 / 1,900` retired).
- [AI_DISCLOSURE.md](AI_DISCLOSURE.md) -- CRediT authorship and transparent AI-assistance disclosure.
- [reviews/2026-11-25-10-pass-red-team-audit.md](reviews/2026-11-25-10-pass-red-team-audit.md) -- Full 10-pass red-team audit and 3-cycle hardening report.

## Benchmarks, Evaluation & Operations

- [benchmarks.md](benchmarks.md) & [BENCH.md](BENCH.md) -- Multi-substrate bare-metal throughput, placement tables, performance budgets, and `az_bench` methodology.
- [preregistration.md](preregistration.md) & [eval/SEEDS.md](eval/SEEDS.md) -- Frozen seed allocation and preregistered confirmatory trial rules.
- [eval/MECHANISMS.md](eval/MECHANISMS.md) -- Native mechanism inventory across 958 catalog keys.
- [operations.md](operations.md) -- Backup, mirror/bundle archival, release packaging, and Z: drive hygiene.
- [threat-model.md](threat-model.md) -- STRIDE threat model for native core, snapshot parser, and localhost viewers.
- [run-log.md](run-log.md) -- Chronological verification and release run log.
- [faq.md](faq.md) & [glossary.md](glossary.md) -- Frequently asked questions and technical terminology.
