# Measuring native tick throughput

`az_bench` times the native tick pipeline on fresh, seeded lab worlds. A repeat builds and locks its own arena, generates the world, warms it up, then times individual ticks with the Windows host clock. The result concerns that binary, CPU topology, world configuration and measured population.

## Run a repeatable measurement

From the repository root in PowerShell, this example compares the C and available assembly kernels on the same seed and collects every repeat.

```powershell
$repo = (Get-Location).Path
$out = Join-Path $repo 'build\work\perf\bench-example'
[void][IO.Directory]::CreateDirectory($out)
$bench = Join-Path $repo 'build\z\az_bench.exe'
Get-FileHash -LiteralPath $bench
& $bench --n 10000 --threads 1,P --kernels 0,1 --seed 42 --warmup 10 --ticks 30 --repeats 3 --max-s 0 --audit-every 1 --csv (Join-Path $out 'repeat.csv') |
    Set-Content -LiteralPath (Join-Path $out 'stdout.txt')
$LASTEXITCODE
```

Record the executable hash, command, CPU topology, OS/build, UTC, power source and other CPU load alongside the output. The default early cutoff is 30 seconds per phase. Use `--max-s 0` for a fixed tick count. `P` selects the P cores, `PE` includes P and E cores and a number requests a worker count. Check the actual `workers` and the requested ideal-processor policy in `pinned`. With `--kernels 1`, the installed mask records which compiled twins the CPU can execute. Mask zero is the C reference. A loaded twin can contribute to speed only if this workload calls it.

The benchmark uses lab seed 42 by default, 100 species, a grid selected for roughly 1 to 2 founders per cell, and a requested capacity of `n + n / 2 + 4096`. The reported capacity includes engine rounding. Fixed-capacity and fixed-store reference runs use `az_run` as described below.

## Read a row

| Output | Meaning |
| --- | --- |
| `n`, grid bits, `cells`, `per_cell`, `cap` | Founders, the base-two logarithm of each grid dimension, allocated cell count, founders per cell, and entity capacity. |
| `warmup_ticks`, `ticks`, `total_s`, `ticks_per_s` | Completed warmup, completed measured ticks, sum of the individually timed tick intervals, and measured ticks divided by that sum. Setup, warmup and optional inter-tick audits are outside the tick timer. |
| `ns_per_creature_tick` | Total measured tick nanoseconds divided by the sum of the engine's `w->n_live` immediately after each measured tick. This accounts for changing population. |
| `ms_tick_mean`, `ms_tick_median`, `ms_tick_p95`, `ms_tick_max` | Statistics across the measured ticks of one repeat. Median and p95 use linear interpolation at `(n - 1) * q` (Hyndman-Fan type 7). |
| `ms_reorder`, `ms_grid`, `ms_perceive`, `ms_match`, `ms_act`, `ms_field`, `ms_life`, `ms_tier1`, `ms_tier2`, `ms_ledger` | Differences of accumulated stage clocks across the measured ticks, divided by the measured tick count. `ms_stage_sum` adds the ten stage means. The outer tick timer also includes work outside the named clocks. |
| `arena_mb`, `locked`, `large_pages` | Arena size in MiB, actual page lock, and whether large pages were obtained. Reported runs require a locked arena. |
| `live_start`, `live_end`, `births`, `deaths`, `cap_denied` | Live count after warmup and at the end, plus cumulative world counters including warmup. Refused births can change how much work the run performs. |
| `matter_ok`, `energy_ok`, `field_underflow`, `audits`, `audit_failures`, `state_hash` | Compare initial and final conserved inventories, check recorded field underflow, and report the number and failures of inventory audits. The program audits after warmup and at the end. `--audit-every K` adds one check every K measured ticks. The final hash covers the resulting world. |
| `other_load_cores`, `on_mains`, `valid` | Machine busy CPU time minus this process's CPU time over the measured wall-clock interval, expressed in fully occupied cores. Power codes are 1 for mains, 0 for battery and -1 for unknown. |

Within each population, repeats interleave the kernel and worker settings. CSV `rep1`, `rep2` and subsequent rows are individual runs. The `median` row represents the complete set of columns from the repeat whose ticks/s is closest to the interpolated median, choosing the slower repeat on an exact tie. `tps_min`, `tps_max`, and `spread_pct` on that row summarize throughput across usable timed repeats. Spread is `100 * (max - min) / median`. Check the validity flags on every individual repeat because the selection can include a truncated or loaded run.

## Decide whether to use a number

| Flag | Trigger and interpretation |
| --- | --- |
| `ok` | None of the four conditions below occurred. Check invariants, seed, length, twins and final hashes separately. Unknown power produces no battery flag. |
| `L` | Other work averaged at least 0.5 CPU cores. Label the timing noisy and record the competing work. |
| `B` | Windows reported battery power when the benchmark queried system power. |
| `U` | The actual arena was unlocked. Pageable diagnostics cannot supply project benchmark evidence. |
| `T` | Warmup or measured ticks ended early under `--max-s`. The repeat is omitted from the complete-run hash comparison. |

The console considers a configuration valid only when all repeats are valid. The median CSV row carries the selected repeat's flag. Inspect each repeat for combined causes. Conservation failure, field underflow or differing hashes across completed runs invalidates the result. Exit 1 reports an invariant/hash failure. Exit 2 covers bad input, setup or output failures. A run can exit zero with timing flags. The current CSV has no page-fault or dropped-event column. Keep those claims tied to a tool that actually reports them.

## Compare against the engine reference

The ES-0 baseline uses the five exact `az_run` configurations in section 8.1 of the performance program, three interleaved rounds. They cover 100,000 founders for 60 ticks at eight workers with C and assembly, 24 mixed-core workers with assembly and OS placement, and one worker with C. The fifth covers 1,000,000 founders for 20 ticks with assembly and 52,000,000 store units. Keep the reference capacity, grid, seed, lock, placement and tick count for each. Record the complete stage line, final live population, hash and inventory checks for every run. Reference hashes are `73f1e9e013f8db50` and `5d9c02d2c7dd5fb1`.

`az_run` computes its printed ns per live entity per tick using the final live population. `az_bench` accumulates live counts at each measured tick. Keep the tool and denominator beside a quoted number. For a claimed change below 10 percent, section 8.1 calls for five interleaved repeats per condition. Use three for larger changes, report their medians and name the host load. Fewer than three is a sample.

The implementation and protocol are in [az_bench.c](../tools/az_bench.c), [az_run.c](../tools/az_run.c), [the stage names](../tools/az_tools.h), and [the engine speed measurement program](handoff/next-agent/08-PERFORMANCE-AND-REALTIME-VIEWER.md#81-how-every-speed-change-is-measured).
