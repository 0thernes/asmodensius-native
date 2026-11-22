"""Check seed 19's reported native nest cues from genesis and after format-9 restore."""
import json
from pathlib import Path
import subprocess
import sys


def invoke(*args):
    return subprocess.run(list(map(str, args)), stdin=subprocess.DEVNULL, capture_output=True,
                          text=True, check=True, timeout=60).stdout.splitlines()


def census(*args):
    return json.loads(next(line[7:] for line in invoke(*args) if line.startswith("census ")))


def verify(run, live, work):
    work.mkdir(exist_ok=True, parents=True)
    genesis = ["--lab", "--seed", 19, "--n", 768, "--cap", 4096, "--grid", "4,4,3",
               "--species", 8, "--threads", 1, "--reorder", 1, "--k1", 8, "--k2", 16, "--stride", 4]
    end = census(run, *genesis, "--ticks", 150)
    first = end["builder"]
    assert first["credited_occasions"] == "171" and first["marginal_mark_units"] == "2689"
    assert first["lifetime_covered_live"] == end["live"] and first["partial_live"] == 0
    assert first["living_observed_builders"] == 18 and first["window_complete"]
    save = work / "75.azs"
    census(run, *genesis, "--ticks", 75, "--save", save)
    restored = census(run, "--lab", "--load", save, "--threads", 3, "--ticks", 75)["builder"]
    assert restored["credited_occasions"] == "121" and restored["marginal_mark_units"] == "1945"
    assert restored["partial_live"] == 1524 and restored["lifetime_covered_live"] == 176
    frames = invoke(live, "--load", save, "--threads", 3, "--dir", work, "--every", 75,
                    "--max-ticks", 75, "--size", "320x180", "--fps", 60)
    end = [e for line in frames if "file" in (e := json.loads(line))][-1]
    assert end["census"]["builder"] == restored and end["hash"] == end["hash_after_view"] == "9bb101f573d8f234"
    assert (work / end["file"]).read_bytes()[:8] == b"\x89PNG\r\n\x1a\n"


if __name__ == "__main__":
    verify(sys.argv[1], sys.argv[2], Path(sys.argv[3]))
