"""Public lifetime and interval counts on a locked colony world."""
import json
from pathlib import Path
import subprocess
import sys


def observe(command):
    return subprocess.run(list(map(str, command)), stdin=subprocess.DEVNULL, capture_output=True, text=True, check=True,
                          timeout=120).stdout.splitlines()


def census(command):
    return json.loads(next(line[7:] for line in observe(command) if line.startswith("census ")))


def verify(run, live, directory):
    directory.mkdir(parents=True, exist_ok=True)
    common = ["--lab", "--seed", 19, "--n", 768, "--cap", 4096, "--grid", "4,4,3",
              "--species", 8, "--threads", 1, "--k1", 8, "--k2", 16, "--stride", 4, "--reorder", 1]
    full = census([run, *common, "--ticks", 150])["commons"]
    assert full["available"] and full["lifetime_covered_live"] == 1700 and full["partial_live"] == 0
    assert full["receipt_pulses"] == "20" and full["contribution_pulses"] == "668"
    assert full["matched_withholdings"] == "0" and full["qualifying_live"] == 0
    saved = directory / "tick75.azs"
    census([run, *common, "--ticks", 75, "--save", saved])
    resumed = census([run, "--lab", "--load", saved, "--threads", 3, "--ticks", 75])["commons"]
    assert resumed["from_tick"] == "75" and resumed["to_tick_exclusive"] == "150"
    assert resumed["lifetime_covered_live"] == 176 and resumed["partial_live"] == 1524
    assert resumed["receipt_pulses"] == "20" and resumed["contribution_pulses"] == "499"
    lines = observe([live, "--load", saved, "--threads", 3, "--dir", directory,
                     "--max-ticks", 75, "--every", 75, "--size", "320x180", "--fps", 60])
    frame = [row for line in lines if "file" in (row := json.loads(line))][-1]
    assert frame["census"]["commons"] == resumed
    assert frame["hash"] == frame["hash_after_view"] == "9bb101f573d8f234"
    assert (directory / frame["file"]).read_bytes().startswith(b"\x89PNG\r\n\x1a\n")


if __name__ == "__main__":
    verify(sys.argv[1], sys.argv[2], Path(sys.argv[3]))
