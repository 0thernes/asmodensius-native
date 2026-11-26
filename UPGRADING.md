# Upgrading (`UPGRADING.md`)

## Upgrading from v1.0.0 to v1.1.0

### Breaking Changes

- **None** in the C23 simulation ABI (`AZ_ABI_VERSION = 4`), CLI flags (`az_run`, `az_live`, `az_bench`, `az_lesion`, `az_snap`), or snapshot/ledger formats. All existing `build/z` binaries and `77/77` CTest targets remain compatible.

### Lean 4 Proof Suite Changes

- `lean/AsmodensiusProofs.lean` is now a Lean 4.35.0 Palomar `module` aggregating `AsmodensiusProofs.Spec`, `AsmodensiusProofs.Core` (`A01`..`A26`), and `AsmodensiusProofs.NonVacuity` (`A27`..`A30`).
- All 20 legacy theorem names (`AsmodensiusProofs.A01_*` through `AsmodensiusProofs.A20_*`) are preserved with identical signatures.
- `AZ_CAP_MAX` in `AsmodensiusProofs.Spec` was corrected from `2000000` to `0x7FFFFFC0` to match `core/az_abi.h:410`.
- Build with `cd lean && lake build` (requires `leanprover/lean4:v4.35.0` via `elan`).

### Licensing and Metadata Changes

- Explicit dual open licensing added at repository root: `LICENSE` (Apache-2.0 for code, Lean proofs, and scripts) and `LICENSE-DOCS` (CC-BY-4.0 for white papers, docs, figures, and datasets).
- Standardized citation files (`CITATION.cff`, `CITATION.bib`, `.zenodo.json`) added for v1.1.0.

### Deprecation Policy

- ABI changes to `core/az_abi.h`, `core/az_types.h`, or `core/*.def` occur only between recorded milestones with an explicit `ABI-V*-NOTICE.md` document (Project Law 13).
- Experimental CLI flags `--allow-pageable` and `--no-lock` remain diagnostics-only and are permanently disallowed in reported benchmark runs.
