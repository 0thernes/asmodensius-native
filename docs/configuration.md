# Configuration and CLI Reference (`docs/configuration.md`)

All runtime behavior in `Asmodensius-Native` is controlled via explicit CLI flags, world parameters (`--param`), and optional process-scoped environment variables. There are no hidden configuration files, no network endpoints, and no API keys (Project Law 15).

## 1. Environment Variables (`.env.example`)

| Variable | Default | Purpose |
|---|---|---|
| `TMP` / `TEMP` | `<repo>\build\tmp` | Process-scoped temp folder on `Z:` required for builds and tests (`AGENTS.md`). |
| `AZ_QUAL_LOCK_RECEIPTS` | *(unset)* | Optional directory enabling working-set page-lock readback receipts in qualification CMake builds (`CMakeLists.txt:212`). |
| `AZ_WIN_LARGE_PAGES` | `0` | Requests Windows 2 MB large pages (`MEM_LARGE_PAGES`); falls back cleanly to 4 KiB `VirtualLock` if `SeLockMemoryPrivilege` is absent (`TR-03`). |

## 2. Core `az_run.exe` Flags

| Flag | Default / Range | Description |
|---|---|---|
| `--lab` | off (wild default) | Enables deterministic Philox4x32-10 test-bench mode. Unflagged runs draw fresh OS entropy every tick (Law 6). |
| `--seed S` | `42` (requires `--lab`) | 64-bit deterministic world seed; refused without `--lab`. |
| `--n N` | `100000` | Initial founder organism count (`1`..`1000000+` within locked RAM). |
| `--ticks T` | `1000` | Number of simulation ticks to execute. |
| `--cap C` | `0` (`n + n/2 + 4096`) | Maximum entity slot capacity (bounded by `AZ_CAP_MAX = 0x7FFFFFC0`). |
| `--species S` | `100` (`1..100`) | Founder species count. Values `> 100` or `< 1` exit with code `2` before arena allocation (`WG_MAX_SPECIES = 100`). |
| `--threads W` | `0` (auto P-cores) | Worker thread count (`0..64`). |
| `--ecores` | off | Activates all physical P + E cores (e.g., 24 workers on `8P+16E`, `TR-07`). |
| `--kernels 0|1` | `1` (auto) | `0` = freestanding scalar C23 reference; `1` = AVX-VNNI + bulk Philox AVX2 + horizontal kinematics AVX2 twins (`TR-13`). |
| `--gpu 0|1` | `0` | Offloads per-creature sparse tiled brain steps to CUDA PTX (`TR-04`/`TR-18`), bit-exact with CPU. |
| `--prefetch 0|1` | `1` | Software DDR5 streaming prefetch ahead of `AZ_CHUNK = 128` blocks (`TR-01`). |
| `--lesion M` | `0` (`0..0x7fffff`) | 23-bit mechanism lesion mask (`AZ_LESION_*`); unnamed bits `>= 2^23` are refused (`A24`). |
| `--large-pages` | off | Attempts 2 MB Windows large pages with safe fallback to 4 KiB `VirtualLock` (`TR-03`). |
| `--save` / `--load` | *(unset)* | Binary snapshot export / deterministic resume (exact state hash preserved across worker counts). |
| `--no-lock` / `--allow-pageable` | off | **Diagnostics only** -- strictly disallowed in any reported benchmark or suite run (`AGENTS.md`). |

## 3. CMake Build Options (`CMakeLists.txt`)

- `-DAZ_ONLY="ALL"` (or semicolon-separated subset of the 22 `core/az_*.c` modules)
- `-DAZ_TWINS="ALL"` (or `NONE` to disable NASM assembly twins)
- `-DAZ_COMPONENTS="ALL"` (`view;gpu`, or `NONE`)
- `-DAZ_MARCH="x86-64-v3"` (target microarchitecture)
- `-DAZ_UBSAN=ON` (separate trap-mode undefined-behavior sanitizer build without `-fwrapv`)
