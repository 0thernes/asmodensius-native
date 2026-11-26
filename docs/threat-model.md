# Threat Model (`docs/threat-model.md`)

This STRIDE threat model covers the three trust boundaries of `Asmodensius-Native`:
(1) the freestanding C23/NASM/PTX simulation core, (2) binary snapshot/ledger deserializers, and (3) local browser visualization servers (`demo/viewer_server.py`, `demo/live_server.py`).

## Trust Boundaries

1. **Simulation Core (`core/`, `kernels/`, `gpu/`):** Receives structured `AzConfig` and pre-allocated arena pointers from `tools/az_*.c`. Zero network I/O, zero filesystem I/O, zero dynamic heap allocation, zero external libraries.
2. **Snapshot & Ledger Files (`--load`, `--ledger`):** Untrusted or corrupted binary files supplied on the CLI could attempt out-of-bounds indices, invalid clade IDs, or arena overflows.
3. **Localhost Viewer Servers (`demo/*.py`):** HTTP/WebSocket bridge bound to `127.0.0.1` relaying live binary frames from `az_live.exe` to a local browser tab.

## STRIDE Analysis and Mitigations

| Category | Threat Scenario | Existing Control / Mitigation | Verification Gate |
|---|---|---|---|
| **Spoofing** | Forged verification badges, fabricated axiom logs, or fake GPU results. | Verbatim `#print axioms` guarded by `scripts/check_readme_axioms.py`; 6-kernel Palomar `Challenge`/`Solution` comparator; SLSA provenance in CI; `CHECKSUMS.sha256`. | `check_readme_axioms.py`, `results/lean4-multi-kernel.txt`, CI `attest-build-provenance`. |
| **Tampering** | Corrupt snapshot/clade index causing OOB read on CPU or GPU (`az_gpu_k_brain`). | Explicit bounds checks (`ci >= n_clades` returns zero motor vector in `gpu/az_gpu_dev.c:93`; `az_world_create` rolls back `arena->used` on failure; `WG_MAX_SPECIES = 100` refusal). | `test_snapshot`, `test_gpu` (`brain guards`), `test_core`, Lean `A07`-`A09`, `A26`. |
| **Repudiation** | Unreproducible or cherry-picked scientific runs. | Counter-based Philox4x32-10 in `--lab`, SHA-256 manifests (`data/MANIFEST.sha256`, `CHECKSUMS.sha256`), reserved unburned seed register (`docs/eval/SEEDS.md`). | `cli_matrix_engine`, `cli_ledger_resume`, `docs/run-log.md`. |
| **Information Disclosure** | Accidental commit of API keys, credentials, or personal paths (Law 15). | Zero API keys or `.env` files in repo; `.gitignore` excludes `.env`; `Audit-Repo.ps1` `A24` regex secret scanner + `gitleaks` in CI. | `Audit-Repo.ps1` (`A24`), CI `gitleaks`. |
| **Denial of Service** | Oversized `--n`/`--cap`/`--species` triggering OOM, stack overflow, or pagefile thrashing. | Single page-locked arena (`VirtualLock` failure exits 3 without pagefile); `-Werror=stack-usage=2048` + `core_obj_gate` forbids `___chkstk_ms`; `--species > 100` exits 2 before allocation; viewer `auto_cap(n)` caps slots cleanly. | `core_obj_gate`, `test_core`, `cli_ws_selftest`, live `--species 101`. |
| **Elevation of Privilege / Memory Corruption** | Undefined behavior, unaligned AVX stack spills, or integer overflow in C23/ASM. | Separate UBSan trap build (`ubsan_trap_build` with 6 planted UB traps), `avx_stack_gate` rejecting unaligned 256-bit moves (GCC bug 54412), and localhost-only viewer binding (`127.0.0.1`). | `ubsan_trap_build`, `avx_stack_gate`, `cli_ws_selftest`. |

## Out-of-Scope Threats

- Exposing `demo/viewer_server.py` or `demo/live_server.py` directly on public internet interfaces is explicitly unsupported (`SECURITY.md`).
- Physical host compromise or malicious OS kernel drivers lie outside application-level defenses.
