#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 Alexander Stephen Donahue
"""Live negative-elaboration and sorry-tamper Lean 4 verification test.

Copies `lean/` into an isolated temporary directory and verifies two failure gates:
  1. Negative elaboration: compiling a false Lean claim against `AsmodensiusProofs.Spec`
     MUST fail elaboration with a non-zero exit code.
  2. Live sorry-tamper rejection: replacing a real proof in `AsmodensiusProofs/Core.lean`
     with `sorry` MUST cause:
       (a) `lake env lean VerifyAxioms.lean` to report `sorryAx`, and
       (b) `lake comparator --config comparator.json --paranoid` (when `--with-comparator`
           is passed on Ubuntu CI) to reject the tampered solution with non-zero exit code.
"""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LEAN_DIR = ROOT / "lean"


def find_lake() -> str:
    found = shutil.which("lake")
    if found:
        return found
    win = Path.home() / ".elan" / "bin" / "lake.exe"
    if win.exists():
        return str(win)
    unix = Path.home() / ".elan" / "bin" / "lake"
    if unix.exists():
        return str(unix)
    raise FileNotFoundError("lake executable not found on PATH or ~/.elan/bin")


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Run live Lean 4 negative-elaboration and sorry-tamper rejection tests."
    )
    parser.add_argument(
        "--with-comparator",
        action="store_true",
        help="Also assert `lake comparator --config comparator.json --paranoid` rejects tampered Solution",
    )
    args = parser.parse_args()
    lake = find_lake()

    tmp_base = ROOT / "build" / "tmp"
    tmp_base.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory(prefix="az_lean_neg_", dir=str(tmp_base)) as tmp:
        work = Path(tmp) / "lean"
        shutil.copytree(
            LEAN_DIR,
            work,
            ignore=shutil.ignore_patterns(".lake", "build"),
        )

        # Build clean Spec first so negative elaboration tests against compiled Spec
        build_spec = subprocess.run(
            [lake, "build", "AsmodensiusProofs.Spec"],
            cwd=str(work),
            capture_output=True,
            text=True,
            check=False,
        )
        if build_spec.returncode != 0:
            raise RuntimeError(
                f"Failed building Spec in isolated temp dir:\n{build_spec.stdout}\n{build_spec.stderr}"
            )

        # Gate 1: Negative elaboration of false claims MUST fail
        neg_file = work / "NegativeProbe.lean"
        neg_file.write_text(
            "module\n"
            "public import AsmodensiusProofs.Spec\n"
            "namespace AsmodensiusProofs\n"
            "public theorem false_species_claim : acceptSpecies 101 = true := by decide\n"
            "public theorem false_spread_claim : (chemSpreadSixFaces 100 30 30 30 30 30 30).1 = 180 := by decide\n"
            "end AsmodensiusProofs\n",
            encoding="utf-8",
            newline="\n",
        )
        neg_res = subprocess.run(
            [lake, "env", "lean", "NegativeProbe.lean"],
            cwd=str(work),
            capture_output=True,
            text=True,
            check=False,
        )
        if neg_res.returncode == 0:
            raise AssertionError("Negative elaboration gate FAILED: Lean accepted false claims!")
        neg_file.unlink(missing_ok=True)

        # Gate 2: Tamper A31_chem_spread_quarter_budget_safe with `sorry`
        import re
        core_path = work / "AsmodensiusProofs" / "Core.lean"
        core_src = core_path.read_text(encoding="utf-8")
        pattern = re.compile(
            r"(public theorem A31_chem_spread_quarter_budget_safe.*? := by\n)(?:  [^\n]*\n)+",
            re.DOTALL,
        )
        m = pattern.search(core_src)
        if not m:
            raise RuntimeError("Could not locate A31_chem_spread_quarter_budget_safe in Core.lean")
        tampered_src = core_src[: m.start()] + m.group(1) + "  sorry\n" + core_src[m.end() :]
        core_path.write_text(tampered_src, encoding="utf-8", newline="\n")

        build_tampered = subprocess.run(
            [lake, "build", "AsmodensiusProofs", "Challenge", "Solution"],
            cwd=str(work),
            capture_output=True,
            text=True,
            check=False,
        )
        if build_tampered.returncode != 0:
            raise RuntimeError(
                f"Expected sorry-tampered build to elaborate with warning:\n"
                f"{build_tampered.stdout}\n{build_tampered.stderr}"
            )

        audit = subprocess.run(
            [lake, "env", "lean", "VerifyAxioms.lean"],
            cwd=str(work),
            capture_output=True,
            text=True,
            check=False,
        )
        combined = (audit.stdout or "") + "\n" + (audit.stderr or "")
        if "sorryAx" not in combined:
            raise AssertionError("Tamper gate FAILED: VerifyAxioms.lean did NOT report sorryAx!")

        comp_status = "skipped (pass --with-comparator on Linux CI)"
        if args.with_comparator:
            comp = subprocess.run(
                [lake, "comparator", "--config", "comparator.json", "--paranoid"],
                cwd=str(work),
                capture_output=True,
                text=True,
                check=False,
            )
            if comp.returncode == 0:
                raise AssertionError(
                    "Tamper gate FAILED: lake comparator ACCEPTED a sorry-tampered Solution!"
                )
            comp_status = f"REJECTED tampered Solution cleanly (exit={comp.returncode})"

        print(
            f"[PASS] Live Lean negative suite: false-claim elaboration REJECTED (exit={neg_res.returncode}); "
            f"sorry-tamper caught via VerifyAxioms.lean (`sorryAx`); comparator={comp_status}."
        )


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print(f"[FAIL] {exc}", file=sys.stderr)
        sys.exit(1)
