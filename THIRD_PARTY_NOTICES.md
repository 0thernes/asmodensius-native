# Third-party notices

Asmodensius-Native uses code ideas and, in its tests, code copies from four
repositories by tsotchke, all published under the MIT License. This file
says, for each one, what was taken, from which commit, which project files
derive from it, and where its licence file is. The licence texts themselves
are not repeated here: each one sits, unchanged, in the folder named below
(copied as a file from the local read-only mirror under `T:\mirrors`).

Nothing from these repositories is linked into the simulation. The engine's
`core/tk_*` kernels are new integer code written for this project; they follow
the mathematics and conventions of the upstream code and say so in their
headers. The upstream files themselves appear only under `tests/upstream/`,
where they serve as test oracles that the `tests/test_tk_*` programs compare
against.

Recorded 2026-11-17. The commit of each mirror is the one checked out there on
that day (`git -C T:\mirrors\<repo> rev-parse HEAD`), and it is the commit the
copies were taken from (`PROVENANCE.txt` or the file header in each folder).

| Component | Upstream | Commit (mirror) | What this project took | Project files derived from it | Licence file |
|---|---|---|---|---|---|
| libirrep | https://github.com/tsotchke/libirrep (version file 1.5.0) | 6c6acab7960bd747d32bc841c9d1f61a0051f1b7 | Eight files as a test oracle for real solid and spherical harmonics, byte-identical to the mirror since 2026-11-17 (their `#include <irrep/...>` lines resolve through an include path CMake gives `test_tk_sh`; `tests/upstream/libirrep/PROVENANCE.txt` lists the SHA-256 of each file). | `core/tk_sh.c`, `core/tk_sh.h` (fixed-point real solid harmonics after `src/solid_harmonics.c`); oracle use in `tests/test_tk_sh.c` | `tests/upstream/libirrep/LICENSE` |
| spin_based_neural_network | https://github.com/tsotchke/spin_based_neural_network | 9e8f2d0cd5ee6eab5e297ad626e1f60aed6b0fef | `src/thermodynamic/hopfield.c` and `include/thermodynamic/hopfield.h`, unmodified, as a test oracle (compiled only into `test_tk_hop`; `PROVENANCE.txt` has their SHA-256). | `core/tk_hop.c`, `core/tk_hop.h` (64-spin Hopfield memory on pattern words, integer code after the Hebbian store and synchronous update of `hopfield.c`); oracle use in `tests/test_tk_hop.c` | `tests/upstream/spin_nn/LICENSE` |
| quantum_geometric_tensor (QGTL) | https://github.com/tsotchke/quantum_geometric_tensor | 770994aa8ddd858e6bb16eebb25250a7b9f67c96 | Three line ranges of `differential_geometry.h` and `differential_geometry.c` (the complex type, four complex helpers, the Fubini-Study metric and Berry curvature kernels) joined into one fragment with carriage returns removed, as a test oracle (`tests/upstream/qgtl/diffgeo_qgt_fragment.inc`, whose header lists the line ranges). | `core/tk_qgt.c`, `core/tk_qgt.h` (quantum geometric tensor in fixed point); oracle use in `tests/test_tk_qgt.c` | `tests/upstream/qgtl/LICENSE` (copied 2026-11-17, SHA-256 afa02f43ee4f95406248e5ec107402f133abccdeaa5baa9f5c1247a4fba9c55e, byte-identical to the mirror's) |
| Moonlab | https://github.com/tsotchke/moonlab (version 1.2.1) | cd3b2347e3739327548708073d7e4ac710cc6a6c | 22 files of the dense state-vector simulator under `tests/upstream/moonlab/src/`, byte-identical to the mirror, built into one test translation unit by the project's own `tests/upstream/moonlab/moonlab_oracle.h`. | `core/tk_qsv.c`, `core/tk_qsv.h` (a classical integer simulation of small state vectors after Moonlab's conventions; no upstream source line); oracle use in `tests/test_tk_qsv.c` | `tests/upstream/moonlab/LICENSE` |

`core/tk_qsv` and `core/tk_qgt` are classical simulations of small quantum
state vectors and tensors. They give no quantum speed-up and produce no quantum
randomness (owner's law 7).

When a vendored copy is updated, take the files from the mirror again, update
the commit above and the folder's `PROVENANCE.txt` or header, and copy the
licence file again as a file.
