"""Credit only positive contest matter received by the attacking UID."""
import json
import subprocess
import sys
from pathlib import Path


def lines(command):
    result = subprocess.run(list(map(str, command)), check=True, stdin=subprocess.DEVNULL,
                            capture_output=True, text=True, timeout=60)
    return result.stdout.splitlines()


def census(command):
    return json.loads(next(line[7:] for line in lines(command) if line.startswith("census ")))


def check(run, live, work):
    work.mkdir(parents=True, exist_ok=True)
    base = ["--lab", "--seed", 442, "--n", 384, "--threads", 2]
    snap = work / "eight.azs"
    eight = census([run, *base, "--ticks", 8, "--save", snap])
    resume = ["--lab", "--load", snap, "--threads", 3]
    loaded = census([run, *resume, "--ticks", 0])
    later = census([run, *resume, "--ticks", 8])
    frames = [r for line in lines([live, *base, "--dir", work, "--max-ticks", 8,
              "--every", 8, "--size", "320x180", "--fps", 60]) if "file" in (r := json.loads(line))]
    last = frames[-1]
    assert last["tick"] == 8 and last["hash"] == last["hash_after_view"]
    assert (work / last["file"]).is_file() and last["census"]["predation"] == eight["predation"]
    # Independent before/after free-pool deltas for surviving attacker UIDs.
    keys = ("complete_positive", "complete_zero", "partial_positive", "partial_zero")
    for result, coverage, occasions, mu in ((eight, (24, 494, 0, 0), 31, 628),
                                           (loaded, (0, 0, 0, 518), 0, 0),
                                           (later, (1, 291, 13, 496), 15, 286)):
        obs = result["predation"]
        assert obs["available"] and obs["amount_scope"] == "observed_interval_living_uids"
        assert tuple(obs[k] for k in keys) == coverage and sum(coverage) == result["live"]
        assert int(obs["occasions"]) == occasions and int(obs["assimilated_mu"]) == mu
    predator_id = 5
    assert eight["archetype"][predator_id] == last["census"]["archetype"][predator_id] == 1
    assert loaded["archetype"][predator_id] == 0
    print("funded predation: positive CLI and PNG classification, measured transfer and restore coverage")


if __name__ == "__main__":
    check(sys.argv[1], sys.argv[2], Path(sys.argv[3]))
