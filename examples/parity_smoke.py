#!/usr/bin/env python3
"""Asmodensius-Native v1.1.0 runnable multi-substrate parity & refusal smoke example.

Runs build/z/az_run.exe on --lab --seed 42 --n 1000 --ticks 10 across:
  1. Scalar freestanding C23 reference (--kernels 0)
  2. NASM AVX-VNNI + bulk Philox AVX2 + kin AVX2 twins (--kernels 1)
  3. Live CUDA PTX sparse brain offload (--kernels 1 --gpu 1, unless --no-gpu)
and verifies identical final state hash, exact conservation, and --species 101 refusal.
"""

from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
AZ_RUN = ROOT / "build" / "z" / "az_run.exe"
TMP_DIR = ROOT / "build" / "tmp"


def run_az(args: list[str]) -> subprocess.CompletedProcess[str]:
    TMP_DIR.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env["TMP"] = str(TMP_DIR)
    env["TEMP"] = str(TMP_DIR)
    return subprocess.run(
        [str(AZ_RUN), *args],
        cwd=ROOT,
        env=env,
        capture_output=True,
        text=True,
        check=False,
    )


def parse_hash(stdout: str) -> str:
    m = re.search(r"final hash ([0-9a-f]{16})", stdout)
    if not m:
        raise RuntimeError(f"Could not find final hash in az_run output:\n{stdout[-400:]}")
    return m.group(1)


def main() -> None:
    parser = argparse.ArgumentParser(description="Run Asmodensius-Native multi-substrate parity smoke test.")
    parser.add_argument("--no-gpu", action="store_true", help="Skip the --gpu 1 check")
    args = parser.parse_args()

    if not AZ_RUN.exists():
        print(f"[SKIP] Native binary not built at {AZ_RUN}; build first with CMake + Ninja.", file=sys.stderr)
        sys.exit(2)

    modes = [
        ("C23-ref", ["--lab", "--seed", "42", "--n", "1000", "--ticks", "10", "--quiet", "--kernels", "0"]),
        ("AVX-VNNI+Philox", ["--lab", "--seed", "42", "--n", "1000", "--ticks", "10", "--quiet", "--kernels", "1"]),
    ]
    if not args.no_gpu:
        modes.append(
            ("RTX-PTX-GPU", ["--lab", "--seed", "42", "--n", "1000", "--ticks", "10", "--quiet", "--kernels", "1", "--gpu", "1"])
        )

    hashes: dict[str, str] = {}
    for label, cli in modes:
        res = run_az(cli)
        if res.returncode != 0:
            raise RuntimeError(f"{label} failed with exit {res.returncode}:\n{res.stderr}")
        if "field_underflow 0 dropped_events 0 audit_failures 0" not in res.stdout:
            raise RuntimeError(f"{label} invariant line missing or non-zero:\n{res.stdout[-400:]}")
        h = parse_hash(res.stdout)
        hashes[label] = h
        print(f"[PASS] {label:18s} final_hash={h} exact_conservation=OK")

    unique = set(hashes.values())
    if len(unique) != 1:
        raise RuntimeError(f"Hash mismatch across substrates: {hashes}")

    refusal = run_az(["--lab", "--seed", "42", "--n", "100", "--species", "101", "--ticks", "1", "--quiet"])
    if refusal.returncode != 2:
        raise RuntimeError(f"Expected exit code 2 for --species 101, got {refusal.returncode}")
    print("[PASS] WG_MAX_SPECIES=100 refusal check: --species 101 rejected with exit=2")
    print(f"[PASS] All {len(hashes)} substrates matched bit-exact hash {next(iter(unique))}.")


if __name__ == "__main__":
    main()
