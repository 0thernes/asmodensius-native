"""Exercise the native census by UID, reordering, working thread counts, restore, and PNG."""
import json
from pathlib import Path
import subprocess
import sys


def artifact(argv, path):
    r = subprocess.run(list(map(str, argv)), stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                       stderr=subprocess.STDOUT, check=True, timeout=90)
    path.write_bytes(r.stdout)
    return r.stdout.decode("utf8").splitlines()


def census(argv, path):
    lines = artifact(argv, path)
    data = json.loads(next(line[7:] for line in lines if line.startswith("census ")))
    state = next(line for line in lines if line.startswith("final hash "))
    assert "field_underflow 0 dropped_events 0 audit_failures 0" in "\n".join(lines)
    return data, state


def verify(data):
    p = data["partners"]
    assert p["available"] and p["window_complete"] and p["celibacy"] is None
    assert p["zero"] + p["one_mating"] + p["repeated_single_partner"] + p["multiple_distinct_partners"] == p["lifetime_covered_live"]
    assert p["partial_live"] + p["lifetime_covered_live"] == data["live"] == sum(data["archetype"])
    assert p["repeated_exclusive_reciprocal"] <= p["repeated_single_partner"]
    assert p["simultaneous_incoming_bonds"] <= p["multiple_distinct_partners"]
    assert data["archetype"][16] <= p["repeated_exclusive_reciprocal"]
    assert data["archetype"][17] <= p["simultaneous_incoming_bonds"]
    assert data["archetype"][19] == 0


def run_tests(run, live, work):
    work.mkdir(exist_ok=True, parents=True)
    base = ["--lab", "--seed", 19, "--n", 768, "--cap", 4096, "--grid", "4,4,3",
            "--species", 8, "--k1", 8, "--k2", 16, "--stride", 4]
    saved = work/"75.azs"
    reference = None
    for workers, reorder in [(1, 1), (3, 1), (8, 9)]:
        c, state = census([run, *base, "--ticks", 150, "--threads", workers, "--reorder", reorder,
                           "--audit-every", 1], work/f"fresh-{workers}-{reorder}.log")
        verify(c)
        p = c["partners"]
        assert p["lifetime_covered_live"] == c["live"] and p["partial_live"] == 0
        assert p["one_mating"] > 0 and p["multiple_distinct_partners"] > 0
        assert p["from_tick"] == "0" and p["to_tick_exclusive"] == "150"
        if reference:
            assert (p, state, c["archetype"]) == reference
        else:
            reference = p, state, c["archetype"]
    c0, _ = census([run, *base, "--threads", 1, "--reorder", 1, "--ticks", 75,
                    "--save", saved, "--audit-every", 1], work/"save.log")
    verify(c0)
    load = ["--lab", "--load", saved, "--threads", 3]
    c_start, _ = census([run, *load, "--ticks", 0], work/"restored-start.log")
    verify(c_start)
    assert c_start["partners"]["lifetime_covered_live"] == 0
    assert c_start["partners"]["partial_live"] == c_start["live"] == c0["live"]
    assert c_start["partners"]["zero"] == 0
    c1, state = census([run, *load, "--ticks", 75, "--audit-every", 1], work/"restored-end.log")
    verify(c1)
    assert state == reference[1]
    p = c1["partners"]
    assert p["from_tick"] == "75" and p["to_tick_exclusive"] == "150"
    assert p["partial_live"] > 0 and p["lifetime_covered_live"] > 0 and p["partial_observed_mating_live"] > 0
    frames = artifact([live, "--load", saved, "--threads", 3, "--dir", work, "--every", 75,
                       "--max-ticks", 75, "--size", "320x180", "--fps", 60], work/"png.log")
    seen = [d for line in frames if "file" in (d := json.loads(line))]
    assert len(seen) >= 2
    for frame in (seen[0], seen[-1]):
        assert frame["hash"] == frame["hash_after_view"]
        assert (work/frame["file"]).read_bytes()[:8] == b"\x89PNG\r\n\x1a\n"
    assert seen[0]["census"] == c_start and seen[-1]["census"] == c1
    assert seen[-1]["hash"] == state.split()[-1]
    print("fresh", json.dumps(reference[0], sort_keys=True), reference[1], "relationship winners", reference[2][16:18])
    print("restored", json.dumps(p, sort_keys=True), state)


if __name__ == "__main__":
    run_tests(sys.argv[1], sys.argv[2], Path(sys.argv[3]))
