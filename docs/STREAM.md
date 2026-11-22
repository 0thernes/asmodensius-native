# AZST binary observation stream

`az_live --stream --dir DIR` writes version 1 frames on stdout. Diagnostic messages go to stderr. The default mode is wild. Use `--lab --seed 42` for repeatable measurements. Other options include `--sps 1..60`, `--tps T` (zero runs freely), `--max-ticks N`, and `--observer-ms M` (zero checks each snapshot). Without `--stream` the existing PNG and JSON-line interface applies.

## Framing

Each frame starts with the four ASCII bytes AZST, a little-endian u16 type at offset 4, u16 version at 6, u32 payload byte length at 8 and u32 sequence at 12. The payload follows at offset 16. Sequence numbering starts at zero independently for each type. All multi-byte payload values are little-endian. Validate lengths before decoding. Integers that represent a hash or UID retain all 64 bits.

| Type | Payload |
|---|---|
| 1 JSON | A single UTF-8 JSON object. Events include start, stats, inspect, picture, error and end. |
| 2 CREATURES | A 64-byte header, then the 24-byte record of each living creature in ascending UID order. |
| 3 STATIC | Eight u32 values (gx, gy, gz, number of cells, number of vents, two sun periods in ticks, field stride), u32 vent indices, one rock-occupancy byte per cell and u32 ground heights per x/y column. |
| 4 FIELDS | u64 tick, u32 gx/gy/gz, u32 parts mask (3), eight reserved zero bytes, then one u32 water height per column followed by four bytes per cell. |
| 5 SHAPES | u32 count, then 4634 bytes per clade. Each record contains u32 clade slot, 16 body bytes, three u16 values (elongation, spike amplitude, maximum gradient in Q8), then two 48 by 48 byte radius maps, base followed by spike. Replace the cache entry when the same slot reappears. |
| 6 PICTURE | An engine-rendered PNG. The preceding picture event provides matching tick, camera, target, pixel dimensions, and before/after capture hashes. |

Cell index is x + gx * (y + gy * z). Water and ground heights use unsigned 32-bit units of 1/256 cell. This covers the engine's supported 4096-cell vertical dimension. A fully solid layer ends at its upper face. The occupancy of the cell just above adds a fractional layer. Water height is the upper face of the highest liquid solvent cell with a positive solvent amount. Zero denotes an empty water column. Field bytes are packed phases, light, scaled temperature and scaled organic abundance. Sun phase and brightness are in stats.

## Creature payload

| Offset | Type | Field |
|---:|---|---|
| 0, 8, 16 | u64 each | Simulation tick and state hashes before and after capture. A skipped hash check writes two zeroes. |
| 24, 28 | u32 each | Live record count and stride (24). |
| 32 | u8 x 4 | Three grid dimension exponents and mode (0 lab, 1 wild). |
| 36, 40 | u32 each | Ticks since the previous snapshot and mean elapsed microseconds per tick. |
| 44 | i8 x 4 | Light vector from the top cell of column zero. |
| 48, 52 | u32 each | Clade slot count and cumulative observer failures. |
| 56 | u32 x 2 | Reserved zero. |

| Record offset | Type | Field |
|---:|---|---|
| 0 | u64 | Unique creature ID, low word first. |
| 8, 10, 12 | u16 each | High 16 bits of the engine x/y/z positions. Multiply each by its grid dimension and divide by 65536 for cells. Use the shortest wrapped path on x and y. |
| 14, 15 | u8, i8 | Yaw in 1/256 turn and pitch with 127 representing a right angle. |
| 16..19 | u8 each | Mass-derived radius code, hue, pattern and engine flags. |
| 20 | u32 | Clade slot. |

## Capture and control

The tick thread reads simulation columns between ticks and hands the writer thread an owned observation buffer. The writer emits complete frames. It also renders proof pictures using a private copy of the scene. A busy writer skips creature snapshots. A relay using stdin as a pipe sends `ack N` after reading creature frame N. Exactly one creature frame can be outstanding. A file or NUL stdin disables credit requirements for finite batch runs. The relay distributes the most recent snapshot to each client separately.

Commands are ASCII lines on stdin. Use `pause`, `run`, `step K`, `sps S`, `tps T`, `pace free|picture`, `observer-ms M`, `size WxH`, `camera YAW PITCH DIST [TX TY TZ]`, `inspect UID` (16 hex digits), `picture`, `save FILE`, or `quit`. A pause followed by a step yields a stable tick for comparing inspect coordinates and a proof picture. A UID survives storage reordering. The slot in an inspect result is valid only at its reported tick.

Stats report the stable tick, live count, matter, energy, field underflows, dropped events, skipped snapshot attempts and observer results. `ms_copy` measures creature extraction and UID ordering. `ms_hash` measures both full observer hashes. `ms_read` covers the full world read between ticks, including hashes, shape and field capture, and creature extraction. `tick_end_unix_ms` is the Windows wall-clock timestamp, in Unix milliseconds, at completion of the captured tick. Match stats by tick before calculating observation age. Tick throughput measures simulation progress. Display FPS requires a browser measurement.

For a full check, workers read disjoint entity and cell ranges of the stable world. The individual canonical hash terms are combined by unsigned 64-bit addition, and the canonical clade, colony and global tail is added once. The tick thread joins every reader before starting capture, and again before advancing the world. One worker uses the serial reference directly. The start event reports the actual locked-arena flag and configured observer worker count. The end event records the final hash, imbalance count and observer failure count. Compare with az_run using identical world configuration and lab seed.

Each snapshot emitted on pause or completion of a paused step gets a full observer check regardless of the configured sampling interval. A paused world waits for a command after refreshing once.
