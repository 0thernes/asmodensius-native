"""Normal CLI and PNG reports expose eligible refusals and restored lifetime coverage."""
import json
from pathlib import Path
import subprocess
import sys


def output(argv):
    return subprocess.run(list(map(str, argv)), stdin=subprocess.DEVNULL, capture_output=True,
                          check=True, text=True, timeout=60).stdout.splitlines()


def census(argv):
    return json.loads(next(line[7:] for line in output(argv) if line.startswith("census ")))


def verify(run, live, work):
    work.mkdir(exist_ok=True, parents=True)
    genesis = ["--lab", "--seed", 19, "--n", 768, "--cap", 4096, "--grid", "4,4,3",
               "--species", 8, "--threads", 1, "--reorder", 1, "--k1", 8, "--k2", 16, "--stride", 4]
    end = census([run, *genesis, "--ticks", 150])
    first = end["hermit"]
    assert first["eligible_refusals"] == "3" and first["lifetime_covered_live"] == end["live"]
    assert first["window_complete"] and first["partial_live"] == 0
    assert first["example"] == {"tick": "145", "recipient_uid": "165"}
    save = work / "75.azs"
    census([run, *genesis, "--ticks", 75, "--save", save])
    resumed = census([run, "--lab", "--load", save, "--threads", 3, "--ticks", 75])
    h = resumed["hermit"]
    assert h["eligible_refusals"] == "2" and h["from_tick"] == "75" and h["to_tick_exclusive"] == "150"
    assert h["partial_live"] == 1524 and h["lifetime_covered_live"] == 176 and h["window_complete"]
    lines = output([live, "--load", save, "--threads", 3, "--dir", work, "--every", 75,
                    "--max-ticks", 75, "--size", "320x180", "--fps", 60])
    frame = [entry for line in lines if "file" in (entry := json.loads(line))][-1]
    assert frame["census"]["hermit"] == h and frame["hash"] == frame["hash_after_view"] == "9bb101f573d8f234"
    assert (work / frame["file"]).read_bytes()[:8] == b"\x89PNG\r\n\x1a\n"
    print("3 eligible refusals from genesis, 2 after restore, exact CLI and PNG coverage")


if __name__ == "__main__":
    verify(sys.argv[1], sys.argv[2], Path(sys.argv[3]))
