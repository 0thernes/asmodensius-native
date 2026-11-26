#!/usr/bin/env python3
"""Asmodensius-Native v1.1.0 - Verbatim README `#print axioms` Consistency Guard.

Ensures every `#print axioms` output line embedded inside README.md between
`<!-- LEAN_AXIOMS_START -->` and `<!-- LEAN_AXIOMS_END -->` matches
`results/lean4-asmodensius-audit.txt` character-for-character.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
AXIOM_LINE_RE = re.compile(
    r"^'[A-Za-z0-9_.]+' (depends on axioms: \[.*\]|does not depend on any axioms)$"
)
FORBIDDEN_TOKENS = ("sorryAx", "ofReduceBool", "trustCompiler")


def main() -> None:
    readme = (ROOT / "README.md").read_text(encoding="utf-8")
    audit = (ROOT / "results" / "lean4-asmodensius-audit.txt").read_text(encoding="utf-8")

    audit_lines = [line.strip() for line in audit.splitlines() if AXIOM_LINE_RE.match(line.strip())]
    if len(audit_lines) < 30:
        print(
            f"[FAIL] Expected >= 30 audited theorem lines in results/lean4-asmodensius-audit.txt, found {len(audit_lines)}",
            file=sys.stderr,
        )
        sys.exit(1)

    for line in audit_lines:
        for bad in FORBIDDEN_TOKENS:
            if bad in line:
                print(f"[FAIL] Forbidden axiom '{bad}' detected in audit: {line}", file=sys.stderr)
                sys.exit(1)

    m = re.search(
        r"<!-- LEAN_AXIOMS_START -->(.*?)<!-- LEAN_AXIOMS_END -->",
        readme,
        flags=re.DOTALL,
    )
    if not m:
        print("[FAIL] Missing <!-- LEAN_AXIOMS_START --> ... <!-- LEAN_AXIOMS_END --> block in README.md", file=sys.stderr)
        sys.exit(1)

    readme_lines = [line.strip() for line in m.group(1).splitlines() if AXIOM_LINE_RE.match(line.strip())]
    if len(readme_lines) != len(audit_lines):
        print(
            f"[FAIL] Count mismatch: README.md has {len(readme_lines)} axiom lines, "
            f"results/lean4-asmodensius-audit.txt has {len(audit_lines)}",
            file=sys.stderr,
        )
        sys.exit(1)

    if readme_lines != audit_lines:
        print("[FAIL] README.md `#print axioms` lines do NOT match results/lean4-asmodensius-audit.txt verbatim:", file=sys.stderr)
        for idx, (r_line, a_line) in enumerate(zip(readme_lines, audit_lines), 1):
            if r_line != a_line:
                print(f"  Line {idx} README: {r_line}", file=sys.stderr)
                print(f"  Line {idx} AUDIT:  {a_line}", file=sys.stderr)
        sys.exit(1)

    print(
        f"[PASS] Verbatim README `#print axioms` check: {len(readme_lines)}/{len(audit_lines)} lines match "
        "results/lean4-asmodensius-audit.txt identically with 0 forbidden axioms."
    )


if __name__ == "__main__":
    main()
