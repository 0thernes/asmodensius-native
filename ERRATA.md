# Errata (`ERRATA.md`)

All known corrections to previously published or committed statements in `Asmodensius-Native` are logged chronologically below.

| Date | Version | Scope | Issue Found | Resolution | Finding Class |
|---|---|---|---|---|---|
| 2026-11-25 | 1.1.0 | `lean/AsmodensiusProofs/Spec.lean` vs v1.0.0 `lean/AsmodensiusProofs.lean` | In v1.0.0, unused constant `AZ_CAP_MAX` was defined as `2000000`, whereas `core/az_abi.h:410` defines `#define AZ_CAP_MAX 0x7FFFFFC0u` (`2147483584`). | Updated `AZ_CAP_MAX` in `lean/AsmodensiusProofs/Spec.lean` to `0x7FFFFFC0` to match `core/az_abi.h` verbatim. | `bug` |
| 2026-11-25 | 1.1.0 | Active metadata & attribution | Earlier archival references across the 7-repo ecosystem occasionally used an unformed entity name; no LLC exists. | Standardized all active metadata (`CITATION.cff`, `CITATION.bib`, `.zenodo.json`, `LICENSE`, `NOTICE`, `paper/main.tex`, `formalization.yaml`, GitHub About) to `Alexander Stephen Donahue` (`Independent Researcher, Spokane, WA, USA`). | `problem` |
| 2026-11-25 | 1.1.0 | `docs/ARCHITECTURE.md` historical ABI notes | Sections 9 and 8 of `docs/ARCHITECTURE.md` retain early ABI v3 text mentioning 48 inputs / 16 motors, whereas `core/az_abi.h` (ABI v4, authoritative per `docs/ARCHITECTURE.md` line 5) defines `AZ_SENS = 64` and `AZ_MOT = 24`. | Documented authoritative ABI v4 dimensions (`AZ_SENS = 64`, `AZ_MOT = 24`) in `docs/architecture.md`, `paper/main.tex`, and Lean theorem `A25_brain_dim_kernel_alignment` without deleting historical text in `docs/ARCHITECTURE.md`. | `gap` |
