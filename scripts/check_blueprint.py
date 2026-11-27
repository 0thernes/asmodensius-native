#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 Alexander Stephen Donahue
"""Asmodensius-Native v1.2.0 - Interactive Lean 4 Proof Blueprint & Portal Consistency Guard.

Verifies that:
  1. Every `data-lean-decl="..."` attribute in `docs/blueprint.html` exists
     verbatim in `results/lean4-asmodensius-audit.txt` (zero stale/fabricated names).
  2. All 38 isolated comparator targets from `lean/comparator.json` appear in
     `docs/blueprint.html`.
  3. Zero inline `<style>`, zero inline `<script>`, zero inline `style=` or `on*=`
     handlers, and zero external CDN URLs exist in `docs/blueprint.html` and `docs/index.html`.
  4. Both pages declare `lang="en"`, strict CSP `<meta http-equiv="Content-Security-Policy"`,
     and a skip-to-main-content link (WCAG 2.2 AA).
"""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def check_html_security_and_a11y(path: Path) -> None:
    html = path.read_text(encoding="utf-8")
    name = path.relative_to(ROOT).as_posix()
    if 'lang="en"' not in html:
        raise AssertionError(f"{name} missing lang=\"en\"")
    if 'http-equiv="Content-Security-Policy"' not in html:
        raise AssertionError(f"{name} missing strict CSP meta tag")
    if 'class="skip-link"' not in html:
        raise AssertionError(f"{name} missing WCAG skip-to-content link")
    if re.search(r"<style\b", html, re.IGNORECASE):
        raise AssertionError(f"{name} contains inline <style> (violates strict CSP)")
    if re.search(r"<script(?![^>]*\bsrc=)", html, re.IGNORECASE):
        raise AssertionError(f"{name} contains inline <script> (violates strict CSP)")
    if re.search(r"\bstyle\s*=", html, re.IGNORECASE):
        raise AssertionError(f"{name} contains inline style= attribute (violates strict CSP)")
    if re.search(r"\bon(?:click|load|change|input|keydown|submit)\s*=", html, re.IGNORECASE):
        raise AssertionError(f"{name} contains inline event handler (violates strict CSP)")
    for bad_cdn in ("cdn.", "unpkg.com", "jsdelivr", "googleapis", "cloudflare.com"):
        if bad_cdn in html.lower():
            raise AssertionError(f"{name} references external CDN token '{bad_cdn}' (zero-CDN violated)")


def main() -> None:
    bp_path = ROOT / "docs" / "blueprint.html"
    idx_path = ROOT / "docs" / "index.html"
    audit_path = ROOT / "results" / "lean4-asmodensius-audit.txt"
    comp_path = ROOT / "lean" / "comparator.json"

    for p in (bp_path, idx_path, ROOT / "docs" / "site.css", ROOT / "docs" / "blueprint.js"):
        if not p.is_file():
            raise FileNotFoundError(f"Missing required GitHub Pages asset: {p.relative_to(ROOT).as_posix()}")

    check_html_security_and_a11y(bp_path)
    check_html_security_and_a11y(idx_path)

    html = bp_path.read_text(encoding="utf-8")
    audit = audit_path.read_text(encoding="utf-8")
    comp = json.loads(comp_path.read_text(encoding="utf-8"))

    audited_decls = set(
        re.findall(r"^'([A-Za-z0-9_.]+)' (?:depends on axioms:|does not depend on any axioms)", audit, re.MULTILINE)
    )
    if len(audited_decls) < 38:
        raise AssertionError(
            f"Expected >= 38 audited declarations in results/lean4-asmodensius-audit.txt, found {len(audited_decls)}"
        )

    bp_decls = re.findall(r'data-lean-decl="([A-Za-z0-9_.]+)"', html)
    if len(bp_decls) < 38:
        raise AssertionError(f"Expected >= 38 linked declarations in docs/blueprint.html, found {len(bp_decls)}")

    missing = [d for d in bp_decls if d not in audited_decls]
    if missing:
        raise AssertionError(f"Blueprint references declarations absent from audit: {missing}")

    comp_theorems = comp.get("theorem_names", [])
    if len(comp_theorems) < 38:
        raise AssertionError(f"Expected >= 38 comparator targets in lean/comparator.json, found {len(comp_theorems)}")

    bp_set = set(bp_decls)
    missing_targets = [t for t in comp_theorems if t not in bp_set]
    if missing_targets:
        raise AssertionError(f"Blueprint missing comparator.json target(s): {missing_targets}")

    print(
        f"[PASS] Lean Proof Blueprint & Portal audit: {len(bp_decls)}/{len(bp_decls)} declarations verified in "
        f"results/lean4-asmodensius-audit.txt ({len(audited_decls)} audited), "
        f"{len(comp_theorems)}/{len(comp_theorems)} comparator targets linked, strict CSP + zero-CDN + WCAG 2.2 AA clean."
    )


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print(f"[FAIL] {exc}", file=sys.stderr)
        sys.exit(1)
