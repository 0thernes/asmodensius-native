# Hardening Log (`HARDENING_LOG.md`)

Every finding across hardening cycles is classified using exactly one of the 10 Build Standard v1.2.0 finding classes (`bug`, `error`, `issue`, `problem`, `concern`, `dilemma`, `conflict`, `gap`, `hole`, `blindspot`).

## Cycle 1 (v1.0.0 Baseline & P4 Bare-Metal Repairs, 2026-11-22)

| ID | Class | Summary | Resolution | Verification |
|---|---|---|---|---|
| H01 | `bug` | `az_world_create` failed allocation paths did not restore `arena->used` (`P4-CORE-02`). | Restored `arena->used = saved_used` on every failure path in `core/az_world.c`. | `tests/test_core.c` (`failed_create_restores_arena_cursor`), Lean `A08`/`A09`. |
| H02 | `bug` | `az_spawn` reused dead slots without releasing their prior store block (`P4-CORE-03`). | Added `if (w->blk[s]) az_blk_release(w, s);` in `core/az_world.c:433`. | `tests/test_core.c` (`reused_slot_releases_store_block`), Lean `A10`/`A11`. |
| H03 | `issue` | CLI `--species > 100` was not rejected prior to arena allocation (`P406-WG-04`/`DEMO-16`). | Enforced `1..100` bounds in `tools/az_run.c`, `tools/az_live.c`, `core/az_worldgen.c`, and UI servers. | Live `--species 101` exits 2; Lean `A06`, `A07`, `A29`. |
| H04 | `dilemma` | At 1M founders, RTX 5070 Ti GPU offload (`1,122.495 ms/tick`) is slower than 24C AVX-VNNI CPU (`757.988 ms/tick`) due to PCIe compaction transfers. | Kept `--gpu 0|1` toggle bit-exact (`31e5d00c106ce679`, 0 fallbacks) and disclosed negative scaling tradeoff honestly; deferred full VRAM world residency to `TR-05`. | `docs/TRIGGER-REGISTER.md`, `LIMITATIONS.md`, `paper/main.tex`. |

## Cycle 2 (v1.1.0 Formalization, White Paper & Build Standard v1.2.0 Upgrade, 2026-11-25)

| ID | Class | Summary | Resolution | Verification |
|---|---|---|---|---|
| H05 | `bug` | v1.0.0 `lean/AsmodensiusProofs.lean` defined `AZ_CAP_MAX := 2000000`, disagreeing with `core/az_abi.h:410` (`0x7FFFFFC0`). | Updated `AZ_CAP_MAX` to `0x7FFFFFC0` in `lean/AsmodensiusProofs/Spec.lean` and added `A24`..`A26` ABI alignment proofs. | `lake build`, `docs/statement-fidelity.md`. |
| H06 | `gap` | v1.0.0 Lean suite lacked `lean-toolchain`, `lakefile.toml`, Palomar `module` headers, isolated `Challenge.lean`/`Solution.lean`, non-vacuity witnesses, and adversarial negative-boundary refutations. | Upgraded to Lean 4.35.0 Palomar 6-kernel architecture with 30 theorems (`A01`..`A30`), `NonVacuity.lean`, `Challenge.lean`, `Solution.lean`, `comparator.json`, `formalization.yaml`. | 5 Windows kernels PASS (`results/lean4-multi-kernel.txt`); 6/6 in Linux CI. |
| H07 | `conflict` | Upstream `Audit-Repo.ps1` template contained three UTF-8 middle-dot bytes (`U+00B7`) on line 245, conflicting with Project Law 14 (`check_text.py` strict 7-bit ASCII). | Replaced the three middle dots in the Markdown summary string with ASCII `|` and documented the deviation in `DEVIATIONS.md`. | `check_text.py` 0 violations; `Audit-Repo.ps1` 100% logic preserved. |
| H08 | `problem` | Active attribution and licensing lacked dual Apache-2.0 / CC-BY-4.0 open licenses and standardized independent researcher affiliation. | Added `LICENSE` (Apache-2.0), `LICENSE-DOCS` (CC-BY-4.0), `NOTICE`, `CITATION.cff`, `.zenodo.json` under `Alexander Stephen Donahue` (`Independent Researcher, Spokane, WA, USA`). | `Audit-Repo.ps1`, `paper/main.tex`. |
| H09 | `blindspot` | Legacy `docs/ARCHITECTURE.md` text still cited ABI v3 brain dimensions (`48`/`16`) while `core/az_abi.h` (ABI v4) uses `AZ_SENS=64`, `AZ_MOT=24`. | Recorded exact ABI v4 constants in `docs/architecture.md`, `ERRATA.md`, `paper/main.tex`, and Lean theorem `A25_brain_dim_kernel_alignment`. | `core/az_abi.h:28-29`, `lean/AsmodensiusProofs/Core.lean`. |
| H10 | `concern` | `TR-03` (`--large-pages`) cannot obtain 2 MB Windows large pages without elevated `SeLockMemoryPrivilege`. | Kept safe fallback to 4 KiB `VirtualLock` (`large_pages=0`) and disclosed status explicitly across `README.md`, `LIMITATIONS.md`, `docs/benchmarks.md`, and `paper/main.tex`. | Live `az_run.exe` output (`locked=1, large_pages=0`). |
| H11 | `bug` | Two stale Python bytecode caches (`demo/__pycache__/live_server.cpython-{39,312}.pyc`) were accidentally tracked in git from the v1.0.0 baseline, tripping Build Standard `A24` (`hygiene`). | Untracked via `git rm --cached` (without deleting files on disk per `AGENTS.md`) and added `__pycache__/` and `*.pyc` to `.gitignore`. | `Audit-Repo.ps1` `A24` PASS. |
