"""The normal reports carry funded amounts for live UIDs and explicit history coverage."""
import json
import subprocess
import sys
from pathlib import Path


def output(argv):
    return subprocess.run(list(map(str, argv)), stdin=subprocess.DEVNULL, capture_output=True,
                          check=True, text=True, timeout=60).stdout.splitlines()


def census(argv):
    return json.loads(next(s[7:] for s in output(argv) if s.startswith("census ")))


def verify(run, live, work):
    work.mkdir(parents=True, exist_ok=True)
    common = ["--lab", "--seed", "442", "--n", "384", "--threads", "2"]
    snap = work / "eight.azs"
    start = census([run, *common, "--ticks", 0])
    four = census([run, *common, "--ticks", 4])
    census([run, *common, "--ticks", 8, "--save", snap])
    late = census([run, "--lab", "--load", snap, "--threads", 3, "--ticks", 8])
    frames = [x for line in output([live, *common, "--dir", work, "--max-ticks", 4,
              "--every", 4, "--size", "320x180", "--fps", 60]) if "file" in (x := json.loads(line))]
    assert frames[-1]["tick"] == 4 and frames[-1]["hash"] == frames[-1]["hash_after_view"]
    assert (work / frames[-1]["file"]).is_file() and frames[-1]["census"]["intake"] == four["intake"]
    assert frames[-1]["census"]["waste_conversion"] == four["waste_conversion"]
    assert not int(start["waste_conversion"]["occasions"])
    for state in (four, late):
        waste = state["waste_conversion"]
        assert waste["available"]
        assert waste["lifetime_covered_live"] + waste["partial_live"] == state["live"]
        assert 0 < int(waste["external_converted_mu_lower"]) <= int(waste["external_converted_mu_upper"])
        assert int(waste["external_converted_mu_upper"]) <= int(waste["biomass_formed_mu"])
        assert 0 < int(waste["external_occasions_lower"]) <= int(waste["external_occasions_upper"]) <= int(waste["occasions"])
    assert four["waste_conversion"]["partial_live"] == 0 < late["waste_conversion"]["partial_live"]
    assert start["intake"]["complete_zero"] == start["live"]
    # Independently replayed native settlement receipts, summed only for the surviving UIDs.
    channels = ["light_eu", "nut_x_mu", "org_y_mu", "wst_fixed_mu", "wst_carried_mu"]
    expected = [(385, 1517, 110971), (252, 818, 5331), (373, 1345, 14876), (315, 658, 3678), (359, 1038, 3035)]
    for s, counts in [(four, (392, 30, 0, 0)), (late, (27, 265, 407, 102))]:
        observed = s["intake"]
        assert observed["available"] and int(observed["locked_observer_bytes"]) > 0
        assert tuple(observed[k] for k in ("complete_positive", "complete_zero", "partial_positive", "partial_zero")) == counts
        assert s["live"] == sum(counts) == sum(s["diet"]) + s["diet_unknown"]
    for name, values in zip(channels, expected):
        a = four["intake"]["channels"][name]
        assert (a["uids"], int(a["occasions"]), int(a["amount"])) == values, (name, a, values)
    assert four["diet"] == [382, 3, 7, 0] and four["diet_unknown"] == 30
    assert late["diet"] == [20, 2, 5, 0] and late["diet_unknown"] == 774
    print("funded intake: five channels, CLI and PNG, restored partial history, exact counts")


if __name__ == "__main__":
    verify(sys.argv[1], sys.argv[2], Path(sys.argv[3]))
