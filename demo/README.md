# Live demo

A local web page for one living Asmodensius-Native world. You can watch it, steer it, save it and test it. The page runs on this computer only (`127.0.0.1`). It uses no internet and no extra software beyond what the build already needs (Python 3.9 or newer and a build of the engine).

## Start it

Double-click `Start-Demo.cmd` in this folder, or run:

```
python -I "Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native\demo\live_server.py" --open
```

Then open <http://127.0.0.1:8765/>. Close the window, or press Ctrl+C, to stop.

The server looks for `az_live.exe` and `az_run.exe` in `build\z` (then `build\z-full`, then the newest build folder that has `az_live.exe`). You can name another folder with `--build`.

## What the page does

| Tab | What it shows |
|---|---|
| Live world | One world made by `az_live` (`tools/az_live.c`). The engine's own renderer (`view/az_view.c`) draws each picture between ticks. You can start, pause, step, change the ticks per picture and the picture size, drag to orbit, use the wheel to zoom, save the world and load it again. Founders, species, slots (memory), lesions and every world parameter of `core/az_params.def` can be set before a start. |
| Proof checks | Runs `az_run` and reads its report: lab repeatability (three runs, one of them on a single thread, give the same hash), wild never-twice (two wild runs differ, and both keep matter and energy exact), a lesion A/B contrast, and the recorded reference worlds of `tests/golden_hashes.txt`. |
| Test suite | Runs `ctest` in the build folder (every test program and gate, several minutes). |
| About | What is real, what is still a stub, and how to run each part without the page. |

Every picture line from `az_live` carries the state hash taken before and after drawing. The page shows a green badge only while the two are equal and matter and energy equal their starting totals after every tick.

## Files

| Path | What it is |
|---|---|
| `demo/live_server.py` | the server (standard library only; programs are started with argument lists, never through a shell) |
| `demo/live.html` | the page (no internet, no libraries) |
| `demo/Start-Demo.cmd` | double-click launcher |
| `tools/az_live.c` | the engine program behind the page; `az_live --help` lists its options and stdin commands |
| `tests/cli/live_smoke.py` | the ctest gate `cli_live_smoke` (21 checks: modes, books, observer, lab hash = `az_run`'s, pipe commands, save and load) |
| `build/demo/` | pictures, saved worlds (`worlds/*.azsnap`) and logs; git ignores `build/` |

## Run the engine without the page

```
"Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native\build\z\az_live.exe" --dir "%TEMP%\az_frames" --n 5000 --max-ticks 200 --every 10
```

This runs a wild world: fresh entropy from the operating system every tick, no seed, never the same twice. Add `--lab --seed 42` for the repeatable test bench. The same world without pictures:

```
"Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native\build\z\az_run.exe" --n 5000 --ticks 200
```
