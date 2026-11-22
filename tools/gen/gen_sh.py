#!/usr/bin/env python3
"""gen_sh.py - development tooling for core/tk_sh.c (never part of the simulation).

  python gen_sh.py consts
      Print the fixed-point constants of tk_sh.h, computed with 60-digit
      decimal arithmetic from the closed forms in libirrep
      src/solid_harmonics.c (init_constants_).  tests/test_tk_sh.c checks the
      values in the header against libm, so this script is a convenience and
      a record of where the numbers come from.

  python gen_sh.py vendor <libirrep mirror> <dest>
      Copy the minimal libirrep files that tests/test_tk_sh.c uses as its
      oracle into <dest> (tests/upstream/libirrep).  The only change made to
      a file is the form of its `#include <irrep/...>` lines (the owner build
      has no include path for the vendored tree), every other byte is kept.
      Writes <dest>/PROVENANCE.txt with the sha256 of each original.
"""
import hashlib
import os
import re
import sys
from decimal import Decimal, getcontext

getcontext().prec = 60
PI = Decimal("3.14159265358979323846264338327950288419716939937510582097494")


def q(value, bits):
    """round-half-up of value * 2^bits (value > 0)"""
    return int((value * (1 << bits) + Decimal("0.5")).to_integral_value(rounding="ROUND_FLOOR"))


def consts():
    k = [
        ("TK_SH_K0_Q30", Decimal(1) / (4 * PI), "sqrt(1/(4 pi))    Y_0,0"),
        ("TK_SH_K1_Q30", Decimal(3) / (4 * PI), "sqrt(3/(4 pi))    Y_1,m   * {y, z, x}"),
        ("TK_SH_K2A_Q30", Decimal(15) / (4 * PI), "sqrt(15/(4 pi))   Y_2,-2 Y_2,-1 Y_2,1 * {xy, yz, xz}"),
        ("TK_SH_K20_Q30", Decimal(5) / (16 * PI), "sqrt(5/(16 pi))   Y_2,0   * (2zz - xx - yy)"),
        ("TK_SH_K22_Q30", Decimal(15) / (16 * PI), "sqrt(15/(16 pi))  Y_2,2   * (xx - yy)"),
        ("TK_SH_K33_Q30", Decimal(35) / (32 * PI), "sqrt(35/(32 pi))  Y_3,-3 Y_3,3"),
        ("TK_SH_K3A_Q30", Decimal(105) / (4 * PI), "sqrt(105/(4 pi))  Y_3,-2  * xyz"),
        ("TK_SH_K31_Q30", Decimal(21) / (32 * PI), "sqrt(21/(32 pi))  Y_3,-1 Y_3,1"),
        ("TK_SH_K30_Q30", Decimal(7) / (16 * PI), "sqrt(7/(16 pi))   Y_3,0"),
        ("TK_SH_K32_Q30", Decimal(105) / (16 * PI), "sqrt(105/(16 pi)) Y_3,2   * z(xx - yy)"),
    ]
    for name, sq, what in k:
        v = sq.sqrt()
        print(f"#define {name:14s} {q(v, 30)}ll  /* {what} = {v:.12f} */")
    y00 = q((Decimal(1) / (4 * PI)).sqrt(), 14)
    print(f"#define TK_SH_Y00_Q14  {y00}")
    # power scale: power_l = (sum_m a^2 [Q60] >> 30) * C_l >> 38, a = mean coefficient in Q30
    c0 = Decimal(1 << 54) / Decimal(4 * y00 * y00)
    print(f"#define TK_SH_PC0_Q24  {q(c0, 0)}u  /* 2^54 / (4 * Y00_Q14^2): calibrated to the rounded Y_0,0 */")
    for l in (1, 2, 3):
        c = 4 * PI / (2 * l + 1)
        print(f"#define TK_SH_PC{l}_Q24  {q(c, 24)}u  /* 4 pi / {2 * l + 1} = {c:.12f} */")
    print(f"/* exact 4 pi in Q24 would be {q(4 * PI, 24)} */")


VENDOR = [
    "LICENSE",
    "include/irrep/export.h",
    "include/irrep/types.h",
    "include/irrep/solid_harmonics.h",
    "include/irrep/spherical_harmonics.h",
    "src/internal/dispatch.h",
    "src/solid_harmonics.c",
    "src/spherical_harmonics.c",
]
INC = re.compile(rb"#include <irrep/([a-z0-9_]+\.h)>")


def vendor(src, dst):
    lines = []
    for rel in VENDOR:
        raw = open(os.path.join(src, rel), "rb").read()
        sha = hashlib.sha256(raw).hexdigest()
        if rel.startswith("include/irrep/"):
            out, n = INC.subn(rb'#include "\1"', raw)
        elif rel.startswith("src/") and rel.endswith(".c"):
            out, n = INC.subn(rb'#include "../include/irrep/\1"', raw)
        else:
            out, n = raw, 0
        path = os.path.join(dst, rel)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "wb") as f:
            f.write(out)
        lines.append(f"{sha}  {rel}  (include lines rewritten: {n})")
    head = open(os.path.join(src, "VERSION"), "r").read().strip()
    text = (
        "libirrep files vendored as a TEST ORACLE ONLY (never linked into the simulation).\n"
        "Upstream: libirrep by tsotchke, MIT License (see LICENSE in this folder).\n"
        f"Upstream version file: {head}; mirror HEAD 6c6acab (2026-05-19) per docs/research/tsotchke/libirrep.md.\n"
        "Copied by tools/gen/gen_sh.py vendor. The ONLY modification: lines of the form\n"
        "  #include <irrep/X.h>\n"
        'were rewritten to #include "X.h" (in include/irrep) or #include "../include/irrep/X.h" (in src),\n'
        "because the owner build has no include path for this folder. Every other byte is unchanged\n"
        "(the files keep their UTF-8 text and CRLF line ends).\n"
        "sha256 of each ORIGINAL upstream file:\n" + "\n".join(lines) + "\n"
    )
    with open(os.path.join(dst, "PROVENANCE.txt"), "w", newline="\n") as f:
        f.write(text)
    print(text)


def main():
    if len(sys.argv) >= 2 and sys.argv[1] == "consts":
        consts()
        return 0
    if len(sys.argv) == 4 and sys.argv[1] == "vendor":
        vendor(sys.argv[2], sys.argv[3])
        return 0
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main())
