"""Verify live-tick bit-exact hash parity across C-ref, AVX2/VNNI+Philox,
DDR5 streaming prefetch, all 24 P+E cores, and CUDA PTX GPU brain offload
(TR-13, TR-18, TR-04, TR-01, TR-07), including recurrence/brain lesions."""
import re
import subprocess
import sys

HASH_RE = re.compile(r"final hash\s+([0-9a-f]{16})")
TRACE_RE = re.compile(r"tick\s+(\d+)\s+live\s+\d+\s+hash\s+([0-9a-f]{16})")
GPU_RE = re.compile(r"gpu_tick:\s+gpu_ticks=(\d+)\s+fallbacks=(\d+)\s+brains=(\d+)")


def run(exe, args):
    p = subprocess.run([exe] + args, stdin=subprocess.DEVNULL,
                       capture_output=True, text=True, timeout=180)
    return p.returncode, p.stdout, p.stderr


def extract(stdout):
    m = HASH_RE.search(stdout)
    final_h = m.group(1) if m else None
    trace = TRACE_RE.findall(stdout)
    exact = ("(exact)" in stdout) and ("VIOLATED" not in stdout)
    return final_h, trace, exact


def main():
    if len(sys.argv) < 2:
        print("usage: gpu_tick_parity.py <az_run>", file=sys.stderr)
        return 2
    exe = sys.argv[1]
    base = ["--lab", "--seed", "42", "--n", "4000", "--ticks", "32",
            "--k1", "8", "--reorder", "8", "--store-period", "8", "--hash-every", "8"]

    # 1. CPU reference baseline
    rc0, out0, err0 = run(exe, base + ["--kernels", "0", "--prefetch", "0"])
    if rc0 != 0:
        print("FAIL cpu baseline:", rc0, err0, file=sys.stderr)
        return 1
    h0, tr0, ex0 = extract(out0)
    if not h0 or not ex0 or len(tr0) < 4:
        print("FAIL cpu baseline parse:", h0, ex0, len(tr0), file=sys.stderr)
        return 1

    # 2. CPU AVX2/VNNI + Bulk Philox + DDR5 Prefetch + 24-core P+E
    for name, extra in [
        ("avx2_vnni_philox_pf", ["--kernels", "1", "--prefetch", "1"]),
        ("ecores_24c_pf", ["--kernels", "1", "--prefetch", "1", "--ecores"]),
    ]:
        rc, out, err = run(exe, base + extra)
        if rc != 0:
            print("FAIL", name, rc, err, file=sys.stderr)
            return 1
        h, tr, ex = extract(out)
        if h != h0 or tr != tr0 or not ex:
            print("FAIL parity", name, h, "!=", h0, file=sys.stderr)
            return 1
        print("PASS %s: hash=%s (%d checkpoints exact)" % (name, h, len(tr)))

    # 3. Live RTX GPU PTX brain offload (skip gracefully if no NVIDIA driver)
    rcg, outg, errg = run(exe, base + ["--kernels", "1", "--prefetch", "1", "--gpu", "1"])
    if rcg == 2 and ("no NVIDIA driver" in errg or "no CUDA device" in errg or "without the GPU" in errg):
        print("SKIPPED gpu parity (no CUDA GPU present):", errg.strip())
        return 0
    if rcg != 0:
        print("FAIL gpu run:", rcg, errg, file=sys.stderr)
        return 1
    hg, trg, exg = extract(outg)
    mg = GPU_RE.search(outg)
    if hg != h0 or trg != tr0 or not exg or not mg:
        print("FAIL gpu parity:", hg, "!=", h0, "exact=", exg, "meta=", bool(mg), file=sys.stderr)
        return 1
    gticks, gfb, gbrains = int(mg.group(1)), int(mg.group(2)), int(mg.group(3))
    if gticks != 32 or gfb != 0 or gbrains == 0:
        print("FAIL gpu counters:", gticks, gfb, gbrains, file=sys.stderr)
        return 1
    print("PASS gpu_live_tick: hash=%s gpu_ticks=%d fallbacks=%d brains=%d" % (hg, gticks, gfb, gbrains))

    # 4. Lesion parity on GPU vs CPU (recurrence lesion 0x2 and brain lesion 0x1)
    for lesion in ("0x1", "0x2"):
        lbase = ["--lab", "--seed", "7", "--n", "2000", "--ticks", "16", "--lesion", lesion]
        rc_c, out_c, err_c = run(exe, lbase + ["--kernels", "1", "--gpu", "0"])
        rc_g, out_g, err_g = run(exe, lbase + ["--kernels", "1", "--gpu", "1"])
        if rc_c != 0 or rc_g != 0:
            print("FAIL lesion", lesion, rc_c, rc_g, err_c, err_g, file=sys.stderr)
            return 1
        hc, _, exc = extract(out_c)
        hg2, _, exg2 = extract(out_g)
        if hc != hg2 or not exc or not exg2:
            print("FAIL lesion parity", lesion, hc, "!=", hg2, file=sys.stderr)
            return 1
        print("PASS gpu_lesion_%s: hash=%s" % (lesion, hg2))

    return 0


if __name__ == "__main__":
    sys.exit(main())
