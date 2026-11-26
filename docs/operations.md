# Operations, Backups, and Mirroring (`docs/operations.md`)

## 1. Local Storage Hygiene (`Z:` Dev Drive, Law 11)

- Canonical local checkout: `Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native` (ReFS Dev Drive).
- Process-scoped temporary files (`TMP` and `TEMP`) MUST point to `<repo>\build\tmp` so build artifacts never land on `C:`.
- Free space check: maintain at least `20 GB` free on `Z:` (`AGENTS.md`). If space drops below `20 GB`, prune only disposable build folders under `build/` (never delete tracked source, `docs/eval/TRACE.csv`, or `results/`).
- Never create `subst`, `net use`, persistent PSDrives, junctions, or symlinks inside the workspace (`AGENTS.md`).
- Never leave `build/work/WRITER.lock` behind at session end.

## 2. Backups and Offsite Repository Mirror (`A35`)

`Asmodensius-Native` follows a 3-copy archival backup strategy:

1. **Primary Remote Origin:** `https://github.com/0thernes/asmodensius-native` (full commit history pushed straight to `main` per Rules R1-R2).
2. **Offline Git Bundle Backup:** Create a verifiable single-file repository bundle on external archival storage (`I:` or offline backup media) before major releases:
   ```powershell
   git bundle create "I:\Backups\asmodensius-native-v1.1.0.bundle" --all
   git bundle verify "I:\Backups\asmodensius-native-v1.1.0.bundle"
   ```
3. **Offsite Archive & Mirror Registration:** Register frozen release PDFs (`dist/Donahue_2026_asmodensius_native.pdf`), clean-room arXiv tarballs (`dist/arxiv_submission.tar.gz`), SHA-256 manifests (`CHECKSUMS.sha256`), and pinned commit SHAs in the master research mirror (`0thernes/0thernes-research-archive`) and Software Heritage (`archive.softwareheritage.org`).

## 3. Release Verification Checklist

Before cutting or tagging any release `vX.Y.Z`:
1. Confirm `VERSION`, `CHANGELOG.md`, `CITATION.cff`, `.zenodo.json`, `lean/lakefile.toml`, and `lean/formalization.yaml` carry the identical version string.
2. Verify `ctest --test-dir build/z --output-on-failure` (`77/77`) and `.\build\z\test_gpu.exe` (`164/164`).
3. Verify Lean 4.35.0 multi-kernel checks (`results/lean4-asmodensius-audit.txt`, `results/lean4-multi-kernel.txt`, `scripts/check_readme_axioms.py`).
4. Rebuild `dist/Donahue_2026_asmodensius_native.pdf` and `dist/arxiv_submission.tar.gz` via `python -I scripts/build_paper.py` and audit live citations via `python -I scripts/check_citations.py --live`.
5. Refresh `CHECKSUMS.sha256` and `data/MANIFEST.sha256`.
6. Confirm `python -I build/work/scripts/check_text.py .` exits `0` and `pwsh -NoProfile -File scripts/Audit-Repo.ps1` reports zero failures.
