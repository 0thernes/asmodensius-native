#!/usr/bin/env python3
"""gen_fx_tables.py - generate the constant tables of core/az_fx.c.

Development tooling only (never part of the simulation). The tables are
printed as C text; the block between the BEGIN/END marker lines is pasted
into core/az_fx.c.

  python gen_fx_tables.py                 print the generated block
  python gen_fx_tables.py --check FILE    exit 0 if FILE contains exactly this block

Tables
  AZ_SIN_Q14[257]    round(16384 * sin(pi/2 * i/256)),        i = 0..256
  AZ_LOG2_Q15[257]   round(32768 * log2(1 + i/256)),          i = 0..256
  AZ_EXP2_Q31[256]   round(2^31 * 2^(i/256)),                 i = 0..255
  AZ_SQRT_SEED[192]  round(4096 * sqrt(i + 0.5)),             i = 64..255

Every value is computed with 60-digit decimal arithmetic, so the rounding
does not depend on the platform's libm.
"""
import sys
from decimal import Decimal, getcontext, ROUND_HALF_EVEN

getcontext().prec = 60

BEGIN = "/* BEGIN GENERATED TABLES (tools/gen/gen_fx_tables.py) - do not edit by hand */"
END = "/* END GENERATED TABLES */"


def dec_pi():
    # Machin: pi = 16 atan(1/5) - 4 atan(1/239)
    def atan_inv(n):
        x = Decimal(1) / n
        x2 = x * x
        term, total, k = x, x, 1
        while abs(term) > Decimal(10) ** -58:
            term = -term * x2
            k += 2
            total += term / k
        return total
    return 16 * atan_inv(5) - 4 * atan_inv(239)


def dec_sin(x):
    term, total, k = x, x, 1
    x2 = x * x
    while abs(term) > Decimal(10) ** -58:
        term = -term * x2 / ((k + 1) * (k + 2))
        k += 2
        total += term
    return total


LN2 = Decimal(2).ln()


def rnd(d):
    """Round to nearest integer; refuse a value that is too close to a tie."""
    r = d.to_integral_value(rounding=ROUND_HALF_EVEN)
    frac = abs(d - d.to_integral_value(rounding="ROUND_FLOOR") - Decimal("0.5"))
    assert frac > Decimal(10) ** -30, "value too close to a rounding tie"
    return int(r)


def tables():
    pi = dec_pi()
    sin_t = [rnd(16384 * dec_sin(pi / 2 * i / 256)) for i in range(257)]
    log_t = [rnd(32768 * (Decimal(256 + i) / 256).ln() / LN2) for i in range(257)]
    exp_t = [rnd(Decimal(2) ** 31 * (LN2 * i / 256).exp()) for i in range(256)]
    seed_t = [rnd(4096 * (Decimal(i) + Decimal("0.5")).sqrt()) for i in range(64, 256)]
    assert sin_t[0] == 0 and sin_t[256] == 16384
    assert all(a < b for a, b in zip(sin_t, sin_t[1:-1])) and sin_t[255] <= sin_t[256]
    assert log_t[0] == 0 and log_t[256] == 32768
    assert all(a < b for a, b in zip(log_t, log_t[1:]))
    assert exp_t[0] == 2 ** 31 and exp_t[255] < 2 ** 32
    assert all(a < b for a, b in zip(exp_t, exp_t[1:]))
    assert 32768 <= seed_t[0] and seed_t[-1] <= 65535
    return sin_t, log_t, exp_t, seed_t


def c_array(ctype, name, values, per_line, suffix=""):
    lines = ["static const %s %s[%d] = {" % (ctype, name, len(values))]
    for i in range(0, len(values), per_line):
        chunk = values[i:i + per_line]
        lines.append("    " + ", ".join("%d%s" % (v, suffix) for v in chunk) + ",")
    lines[-1] = lines[-1].rstrip(",")
    lines.append("};")
    return lines


def block():
    sin_t, log_t, exp_t, seed_t = tables()
    out = [BEGIN]
    out.append("/* round(16384 * sin(pi/2 * i/256)), i = 0..256 */")
    out += c_array("i16", "AZ_SIN_Q14", sin_t, 12)
    out.append("/* round(32768 * log2(1 + i/256)), i = 0..256 */")
    out += c_array("u16", "AZ_LOG2_Q15", log_t, 12)
    out.append("/* round(2^31 * 2^(i/256)), i = 0..255 */")
    out += c_array("u32", "AZ_EXP2_Q31", exp_t, 6, "u")
    out.append("/* round(4096 * sqrt(i + 0.5)), i = 64..255 */")
    out += c_array("u16", "AZ_SQRT_SEED", seed_t, 12)
    out.append(END)
    return "\n".join(out) + "\n"


def main():
    text = block()
    if len(sys.argv) == 3 and sys.argv[1] == "--check":
        data = open(sys.argv[2], "rb").read().decode("ascii")
        a, b = data.find(BEGIN), data.find(END)
        if a < 0 or b < 0:
            print("gen_fx_tables: markers not found in", sys.argv[2])
            return 1
        found = data[a:b + len(END)] + "\n"
        if found != text:
            fl, tl = found.split("\n"), text.split("\n")
            for n, (x, y) in enumerate(zip(fl, tl), 1):
                if x != y:
                    print("gen_fx_tables: block line %d differs\n  file: %s\n  gen : %s" % (n, x, y))
                    break
            else:
                print("gen_fx_tables: block length differs (%d vs %d lines)" % (len(fl), len(tl)))
            return 1
        print("gen_fx_tables: %s matches the generated tables (%d lines)" % (sys.argv[2], text.count("\n")))
        return 0
    if len(sys.argv) != 1:
        print(__doc__)
        return 2
    sys.stdout.write(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
