# Datasheet for Datasets: Asmodensius-Native Evaluation & Mechanism Trace Catalogs (`docs/DATASHEET.md`)

Following Gebru et al. (2021), *Datasheets for Datasets* (Communications of the ACM, 64(12):86-92), this datasheet documents the empirical evaluation and traceability CSV catalogs stored under `docs/eval/` and indexed in `data/MANIFEST.sha256` and `croissant.json` for `Asmodensius-Native` v1.2.0.

## 1. Motivation

- **For what purpose was the dataset created?** To maintain an auditable, non-theatrical mapping between architectural/evaluation catalogs (`butlin14`, `cqm25`, `antilarp13`, `passes100`, `life100`, etc.), concrete C23 functions in `core/`, explicit `AZ_LESION_*` lesion switches, and development-seed evaluation records under Project Laws 4, 8, and 9.
- **Who created the dataset?** Alexander Stephen Donahue (Independent Researcher, Spokane, WA, USA; ORCID `0009-0006-8168-9648`), with engineering-script and catalog-structuring assistance from AI coding assistants under direct human oversight (see `docs/AI_DISCLOSURE.md`).
- **Who funded the creation of the dataset?** Self-funded independent research; zero external grants or corporate sponsors.

## 2. Composition

The evaluation dataset comprises six immutable UTF-8/ASCII CSV tables under `docs/eval/` (zero personal data; zero biological or clinical human data):

| File | Rows | Bytes | SHA-256 | Role |
|---|---:|---:|---|---|
| `docs/eval/MECHANISMS.csv` | 958 | 544,019 | `767bff1f9c0c9be2b618d846f1bd171f85759781a4f44c99038e74ab658b1fab` | Maps catalog items to C23 functions and `AZ_LESION_*` bits. |
| `docs/eval/MECHANISMS-crosswalk.csv` | 958 | 815,231 | `b9064e13755322f08417d873d3d229e148ad6d3a9816f66dc0e3683522a936b5` | Crosswalk linking source criteria to pre-sense and R8 fixtures. |
| `docs/eval/MECHANISMS-historical.csv` | 1,125 | 214,944 | `91b1a371467a13a0a32c2466594a43a78a3fbc1cd8685295a105195fb1aa50c3` | Historical nomenclature archive prior to wave-2 freeze. |
| `docs/eval/dispositions.csv` | 6,305 | 1,582,701 | `66cb5adcb1ef395887c3db5189ff249f79b49656163496f97b2c3190ee0645f4` | Per-item disposition, rationale, and honesty flags. |
| `docs/eval/TRACE.csv` | 9,838 | 4,977,947 | `0353445901aa52e7ddfd4b3d4617c2fc8746d3b9d04e764ba8a90e726e240620` | Master provenance trace linking dispositions, preregistration hashes, seed blocks, and verdicts. |
| `docs/eval/ISSUES.csv` | 221 | 160,169 | `fc19d75ca7216f2bfe5590c4b5d7c5c6264efa36d054a048900ada3e09fb8030` | Engineering defect and verification closure register. |

- **Does the dataset contain data that might be considered confidential or personally identifiable?** No. All entries describe deterministic software mechanisms, synthetic integer-simulation runs, and architectural checklist items.

## 3. Collection Process

- Generated deterministically by repository inventory scripts and human-reviewed audit passes against `core/`, `tests/`, and `docs/eval/SEEDS.md`.
- Exploratory development lesion measurements used development seeds `1..24`.
- **Reserved Confirmatory Seed Protection:** Reporting and confirmation seed blocks `5001..5024`, `6001..6024`, and `6101..6124` remain strictly unburned (verified in `docs/eval/SEEDS.md`).

## 4. Preprocessing, Cleaning, and Labeling

- All six CSV files conform to strict 7-bit ASCII, LF line endings, zero tabs, and trailing final newline (verified by `python3 -I tools/check_text.py`).
- `docs/eval/TRACE.csv` is retained byte-identical (`4,977,947` bytes, strictly under the 5 MiB repository hygiene limit) and verified by SHA-256 in `data/MANIFEST.sha256`.

## 5. Uses and Non-Uses (Real-vs-Metaphor Boundary, Law 8)

- **Intended Uses:** Auditing mechanism-to-lesion coverage, checking honest epistemic labeling of simulation diagnostics, and preparing preregistered confirmatory runs on reserved seed blocks.
- **Prohibited / Misleading Uses:** These catalogs MUST NOT be interpreted as evidence of consciousness, sentience, moral patienthood, biological life, AGI, quantum speedup, or resolution of any open problem in `unsolved-problems-mega-atlas` (`0 / 1,900` retired). There is no aggregate "consciousness score."

## 6. Distribution

- Distributed openly in the public GitHub repository (`https://github.com/0thernes/asmodensius-native`), indexed in `croissant.json` (MLCommons Croissant 1.0) and `ro-crate-metadata.json` (RO-Crate 1.1), and archived via Software Heritage (`swh:1:snp:08a957772115194b51a4eadb77331b8bab261773`).
- Licensed under **Creative Commons Attribution 4.0 International (CC-BY-4.0)** (`LICENSE-DOCS`, `LICENSES/CC-BY-4.0.txt`, `REUSE.toml`).

## 7. Maintenance

- Maintained by Alexander Stephen Donahue (`0_0@0thernes.art`).
- Any update to any CSV file requires regenerating `data/MANIFEST.sha256`, `CHECKSUMS.sha256`, `CHECKSUMS.sha256.tsr`, `CHECKSUMS.sha256.ots`, `croissant.json`, and `ro-crate-metadata.json`, and logging any corrections in `ERRATA.md`.
