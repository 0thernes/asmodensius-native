# Asmodensius-Native Examples (`examples/`)

Runnable, headless verification scripts demonstrating multi-substrate bit-exact
parity, explicit refusal boundaries, and checksum generation.

## `parity_smoke.py`

Runs `build/z/az_run.exe` across scalar C23 (`--kernels 0`), AVX-VNNI/AVX2 twins
(`--kernels 1`), and optional RTX 5070 Ti CUDA PTX offload (`--gpu 1`) on
`--lab --seed 42 --n 1000 --ticks 10`, verifying identical `az_state_hash`
(`7bd9823448dd38b1`), exact matter and energy conservation, `field_underflow 0`,
`dropped_events 0`, and explicit refusal of `--species 101` (exit code 2).

```powershell
python -I examples/parity_smoke.py
python -I examples/parity_smoke.py --no-gpu
```
