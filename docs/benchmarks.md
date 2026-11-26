# Bare-Metal Multi-Substrate Benchmarks (`docs/benchmarks.md`)

This document consolidates the verified bare-metal throughput measurements, bit-exact state hashes, performance budgets, and honest hardware tradeoffs for `Asmodensius-Native` v1.1.0. For the low-level `az_bench` column specification and validity flags (`ok`, `L`, `B`, `U`, `T`), see [docs/BENCH.md](BENCH.md).

## 1. Measurement Host and Invariant Conditions

- **CPU:** Intel Core Ultra 9 275HX (`24` logical / `24` physical cores: `8` P-cores + `16` E-cores, `0` SMT, AVX2 + AVX-VNNI, `cpu features 0x1f`, `twin mask 0xd`).
- **GPU:** NVIDIA GeForce RTX 5070 Ti (`12,199` MiB VRAM, compute capability `12.0`, CUDA driver API `13040`).
- **OS & Storage:** Windows 11 x86-64 on ReFS Dev Drive `Z:`.
- **Mandatory Invariants for Every Reported Row:** `locked=1` (no `--allow-pageable`, no `--no-lock`), exact matter conservation (`==`), exact energy conservation (`==`), `field_underflow=0`, `dropped_events=0`, `audit_failures=0`, and bit-identical 64-bit `az_state_hash` against scalar freestanding C23 (`--kernels 0`).

## 2. Multi-Substrate Placement Table (`--lab --seed 42`)

| Scale | Live Organisms | Arena Locked | State Hash | Execution Substrate | Tick Time (ms/tick) | Speedup vs 8P C-ref | Status / Source |
|---|---:|---:|---|---|---:|---:|---|
| 1k (10t smoke) | 1,559 | 56.7 MB | `7bd9823448dd38b1` | 8P C-ref (`--kernels 0`) | 5.396 | 1.00x | Verified 2026-11-25 |
| 1k (10t smoke) | 1,559 | 56.7 MB | `7bd9823448dd38b1` | 8P AVX-VNNI+Philox (`--kernels 1`) | 5.631 | 0.96x | Verified 2026-11-25 |
| 1k (10t smoke) | 1,559 | 56.7 MB | `7bd9823448dd38b1` | RTX 5070 Ti GPU (`--gpu 1`) | 5.288 | 1.02x | Verified 2026-11-25 |
| 10k (32t, TR-04) | 15,394 | locked=1 | `21e4cfcdf0b08066` | C-ref / VNNI / GPU parity | 11.765 | parity | Closed `TR-04` / `TR-13` |
| 100k (32t, TR-07) | 153,084 | locked=1 | `5b8110b324d2603f` | 8P C-ref (`--kernels 0`) | 113.162 | 1.00x | Closed `TR-07` |
| 100k (32t, TR-07) | 153,084 | locked=1 | `5b8110b324d2603f` | 8P AVX-VNNI + Philox + PF | 109.323 | 1.04x | Closed `TR-01`/`TR-13` |
| 100k (32t, TR-07) | 153,084 | locked=1 | `5b8110b324d2603f` | RTX 5070 Ti GPU + 24C | 92.801 | 1.22x | Closed `TR-04`/`TR-07` |
| 100k (32t, TR-07) | 153,084 | locked=1 | `5b8110b324d2603f` | 24C (8P+16E) VNNI+Philox+PF | 72.199 | **1.57x (+56.7%)** | Best 100k placement |
| 1M (TR-01/04/07) | 1,004,019 | 9,260.6 MB | `31e5d00c106ce679` | 8P C-ref (`--kernels 0`) | 1,025.332 | 1.00x | Closed `TR-01`/`TR-07` |
| 1M (TR-01/04/07) | 1,004,019 | 9,260.6 MB | `31e5d00c106ce679` | 8P AVX-VNNI + Philox + PF | 976.322 | 1.05x | Closed `TR-01`/`TR-13` |
| 1M (TR-01/04/07) | 1,004,019 | 9,260.6 MB | `31e5d00c106ce679` | 24C (8P+16E) VNNI+Philox+PF | 757.988 | **1.35x (+35.3%)** | Best 1M placement |
| 1M (TR-01/04/07) | 1,004,019 | 9,260.6 MB | `31e5d00c106ce679` | RTX 5070 Ti GPU + 24C (4,999,522 brains, 0 fallbacks) | 1,122.495 | 0.91x | Slower (PCIe tradeoff) |

Additional verified parity hashes:
- **5k parity (`TR-13`):** `0f5867c70e9bda5b` (`--kernels 0` == `--kernels 1`).
- **HyperBrain 96-neuron parity (`TR-04`):** `73a8d1f51b9556da` (CPU == RTX 5070 Ti GPU).
- **Standalone GPU kernel micro-verification (`test_gpu.exe`):** `164/164` checks PASS, 1,000,000 variable-size sparse `AzBrain` steps (`0..48` hidden neurons, `128` templates, `hid=0`, `motor=0`), 1,000,000 kinematics lanes x 3 shifts (`0` diffs).

## 3. Performance Budgets (`A37`)

| Metric | Target Budget | Current Measured Status |
|---|---|---|
| Core C23 stack frame (`libaz_core.a`) | `<= 2,048` bytes per function (`-Werror=stack-usage=2048`) | PASS (enforced at compile time) |
| Hot entity record traffic | `<= 270` bytes/creature/tick (`128 B` fixed record + scratch) | PASS (`AZ_CHUNK = 128`) |
| 100k locked tick latency (24C VNNI+PF) | `< 100 ms/tick` | PASS (`72.199 ms/tick`) |
| 1M locked RAM footprint | `<= 50%` of 48 GB physical RAM (`<= 24 GB`, `TR-02`) | PASS (`9,260.6 MB` locked) |
| 1M locked tick latency (24C VNNI+PF) | `< 1,000 ms/tick` | PASS (`757.988 ms/tick`) |

## 4. Honest Hardware Tradeoffs & Deferred Optimizations

1. **GPU PCIe & Compaction Tradeoff at 1M:** On 1M scattered variable-size store blocks, dirty-block host-to-device synchronization after two-half store compaction pushes RTX 5070 Ti offload to `1,122.495 ms/tick`, slower than 24C AVX-VNNI CPU (`757.988 ms/tick`) despite `0` kernel fallbacks and bit-exact hash parity. Full GPU-resident world state is tracked as open item `TR-05`.
2. **Windows 2 MB Large Pages (`TR-03`):** `--large-pages` and `AZ_WIN_LARGE_PAGES` are wired cleanly on `az_run` and `az_live`, but non-elevated Windows tokens lack `SeLockMemoryPrivilege`, so runs fall back cleanly to 4 KiB `VirtualLock` (`large_pages=0`).
3. **Sample Size Disclosure:** Small 1k smoke timings above are single-run parity confirmations; formal speed claims below 10% require five interleaved repeats via `az_bench` per [docs/BENCH.md](BENCH.md).
