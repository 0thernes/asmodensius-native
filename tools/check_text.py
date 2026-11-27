#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 Alexander Stephen Donahue
"""Tracked Law-14 Text Hygiene Checker (`tools/check_text.py`).

Enforces Project Law 14 across active repository files:
  - Pure 7-bit printable ASCII (bytes 0x20..0x7E) plus LF (0x0A)
  - Zero CR (0x0D) bytes
  - Zero literal tabs (0x09), except `.tsv` data tables and explicitly listed POSIX shell TSV parsers
  - Trailing final newline on every non-empty text file

Explicit, transparent scope separation (closing ERRATA.md #5 honestly):
  - Active scope (strict gate; exits 1 on ANY violation):
      `core/`, `kernels/`, `gpu/`, `platform/`, `tools/`, `view/`, `tests/` (excluding
      vendored `tests/upstream/`), `demo/`, `examples/`, `lean/`, `paper/`, `scripts/`,
      `.github/`, root metadata/docs, and active `docs/` (all `docs/` outside the
      explicit archival/exempt allowlists below).
  - Exempt verbatim owner prompts (`EXEMPT`):
      `docs/handoff/PROMPTS.md`, `docs/handoff/PROMPTS.xml` (Law 12 verbatim owner words).
  - Archival historical records (`ARCHIVAL_PREFIXES`):
      `docs/requirements/`, `docs/research/`, `tests/upstream/` (preserved verbatim from
      upstream mirrors / early requirement & microbenchmark dumps). Violations inside
      archival prefixes are counted and reported honestly rather than hidden.

Usage:
  python3 -I tools/check_text.py                  # Strict active-scope gate + archival summary
  python3 -I tools/check_text.py --report-archival # Also print every archival file detail
  python3 -I tools/check_text.py --self-test       # Run built-in planted-fault self-test
"""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

TEXT_EXTS = {
    ".c", ".h", ".def", ".asm", ".inc", ".py", ".md", ".txt", ".cmake", ".xml",
    ".json", ".ps1", ".cu", ".ptx", ".js", ".mjs", ".html", ".css", ".cmd",
    ".csv", ".jsonl", ".lean", ".tex", ".bib", ".yaml", ".yml", ".toml",
    ".sh", ".cff", ".mdc", ".svg", ".tsv", ".sha256",
}

EXACT_NAMES = {
    "CMakeLists.txt", "VERSION", "LICENSE", "LICENSE-DOCS", "NOTICE",
    "justfile", ".gitignore", ".gitattributes", ".editorconfig", ".env.example",
}

SKIP_DIRS = {".git", ".lake", "build", "__pycache__"}

EXEMPT = {
    "docs/handoff/PROMPTS.md": "Law 12 verbatim owner words",
    "docs/handoff/PROMPTS.xml": "Law 12 verbatim owner words",
}

TAB_OK = {
    "scripts/audit-repo.sh": "POSIX shell IFS tab delimiter for reading scripts/audit-checks.tsv",
}

ARCHIVAL_PREFIXES = (
    "docs/requirements/",
    "docs/research/",
    "tests/upstream/",
)


def is_text_target(rel: str) -> bool:
    name = os.path.basename(rel)
    ext = os.path.splitext(name)[1].lower()
    return ext in TEXT_EXTS or name in EXACT_NAMES


def check_bytes(raw: bytes, allow_tab: bool = False) -> list[str]:
    reasons: list[str] = []
    if b"\r" in raw:
        reasons.append("CR")
    if b"\t" in raw and not allow_tab:
        reasons.append("tab")
    bad_bytes = [i for i, b in enumerate(raw) if b > 126 or (b < 32 and b not in (10, 13, 9))]
    if bad_bytes:
        first_idx = bad_bytes[0]
        line_no = raw.count(b"\n", 0, first_idx) + 1
        reasons.append(f"non-ASCII byte 0x{raw[first_idx]:02x} at line {line_no} ({len(bad_bytes)} in all)")
    if raw and not raw.endswith(b"\n"):
        reasons.append("no final newline")
    return reasons


def iter_repo_files(root: Path) -> list[str]:
    try:
        out = subprocess.check_output(
            ["git", "ls-files", "-co", "--exclude-standard"],
            cwd=str(root),
            text=True,
            stderr=subprocess.DEVNULL,
        )
        rels = sorted({line.strip().replace("\\", "/") for line in out.splitlines() if line.strip()})
        return [r for r in rels if not any(part in SKIP_DIRS for part in r.split("/")) and (root / r).is_file()]
    except Exception:
        found: list[str] = []
        for dirpath, dirnames, filenames in os.walk(root):
            dirnames[:] = sorted(d for d in dirnames if d not in SKIP_DIRS)
            for fname in sorted(filenames):
                full = Path(dirpath) / fname
                found.append(full.relative_to(root).as_posix())
        return found


def run_self_test() -> int:
    cases = [
        ("clean.c", b"int x = 0;\n", False, []),
        ("cr.c", b"int x = 0;\r\n", False, ["CR"]),
        ("tab.c", b"int\tx = 0;\n", False, ["tab"]),
        ("tab_ok.tsv", b"col1\tcol2\n", True, []),
        ("nonascii.md", b"caf\xc3\xa9\n", False, ["non-ASCII"]),
        ("nofinal.py", b"x = 1", False, ["no final newline"]),
    ]
    failures: list[str] = []
    with tempfile.TemporaryDirectory(prefix="az_check_text_selftest_") as tmp:
        for name, data, allow_tab, expected_tokens in cases:
            p = Path(tmp) / name
            p.write_bytes(data)
            got = check_bytes(p.read_bytes(), allow_tab=allow_tab)
            joined = ", ".join(got)
            if not expected_tokens:
                if got:
                    failures.append(f"{name}: expected clean, got {joined}")
            else:
                for tok in expected_tokens:
                    if tok not in joined:
                        failures.append(f"{name}: expected '{tok}', got '{joined or 'clean'}'")
    if failures:
        for msg in failures:
            print(f"[SELF-TEST FAIL] {msg}", file=sys.stderr)
        return 1
    print(f"[PASS] tools/check_text.py self-test: {len(cases)}/{len(cases)} cases passed.")
    return 0


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description="Tracked Law-14 text hygiene checker.")
    parser.add_argument("root", nargs="?", default=str(ROOT), help="Repository root directory")
    parser.add_argument("--report-archival", action="store_true", help="Print every archival-prefix violation")
    parser.add_argument("--self-test", action="store_true", help="Run built-in planted-fault self-test")
    args = parser.parse_args(argv)

    if args.self_test:
        return run_self_test()

    root = Path(args.root).resolve()
    rels = [r for r in iter_repo_files(root) if is_text_target(r)]

    active_checked = 0
    active_violations: list[tuple[str, list[str]]] = []
    exempt_checked = 0
    archival_checked = 0
    archival_violations: list[tuple[str, list[str]]] = []

    for rel in rels:
        raw = (root / rel).read_bytes()
        allow_tab = rel.endswith(".tsv") or rel in TAB_OK
        why = check_bytes(raw, allow_tab=allow_tab)

        if rel in EXEMPT:
            exempt_checked += 1
            continue
        if any(rel.startswith(pref) for pref in ARCHIVAL_PREFIXES):
            archival_checked += 1
            if why:
                archival_violations.append((rel, why))
                if args.report_archival:
                    print(f"[ARCHIVAL] {rel}: {', '.join(why)}")
            continue

        active_checked += 1
        if why:
            active_violations.append((rel, why))
            print(f"[VIOLATION] {rel}: {', '.join(why)}", file=sys.stderr)

    print(
        f"Law-14 Text Hygiene Audit: active={active_checked} checked ({len(active_violations)} violations) | "
        f"exempt={exempt_checked} (verbatim owner prompts) | "
        f"archival={archival_checked} checked ({len(archival_violations)} historical files with UTF-8/CR/tab bytes) | "
        f"tab_allowlisted={len(TAB_OK)}"
    )
    if active_violations:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
