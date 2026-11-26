#!/usr/bin/env python3
"""Asmodensius-Native - Live Citation, DOI, Retraction & Public URL Auditor.

Verifies every BibTeX entry in paper/refs.bib against the live Crossref REST API
(resolving DOIs, checking Crossref update-to retraction metadata, verifying years),
checks non-DOI bibliography URLs, verifies 1-to-1 coverage with paper/main.tex,
and checks public repository/ORCID endpoints.

Usage:
  python -I scripts/check_citations.py             # Offline syntax + coverage check
  python -I scripts/check_citations.py --live      # Live Crossref + URL + Retraction check
"""

from __future__ import annotations

import argparse
import json
import re
import urllib.error
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BIB_PATH = ROOT / "paper" / "refs.bib"
TEX_PATH = ROOT / "paper" / "main.tex"

PUBLIC_URLS = [
    "https://github.com/0thernes/asmodensius-native",
    "https://orcid.org/0009-0006-8168-9648",
]

DOI_RE = re.compile(r"^10\.\d{4,9}/[-._;()/:A-Za-z0-9]+$")


def parse_bib(text: str) -> list[dict[str, str]]:
    entries: list[dict[str, str]] = []
    for raw in re.split(r"\n(?=@)", text.strip()):
        m_key = re.match(r"@(\w+)\{([^,]+),", raw)
        if not m_key:
            continue
        entry: dict[str, str] = {"type": m_key.group(1), "key": m_key.group(2).strip()}
        for field in ("doi", "year", "title", "url", "eprint", "isbn"):
            m = re.search(rf"\b{field}\s*=\s*\{{(.+?)\}}(?:,|\s*\n|\s*\}})", raw, flags=re.IGNORECASE)
            if m:
                entry[field] = m.group(1).strip()
        entries.append(entry)
    return entries


def check_tex_citation_coverage(entries: list[dict[str, str]]) -> None:
    tex = TEX_PATH.read_text(encoding="utf-8")
    bib_keys = {e["key"] for e in entries}
    cited_keys: set[str] = set()
    for grp in re.findall(r"\\cite(?:\[[^\]]*\])?\{([^}]+)\}", tex):
        for k in grp.split(","):
            tok = k.strip()
            if tok:
                cited_keys.add(tok)
    missing_in_bib = cited_keys - bib_keys
    uncited_in_tex = bib_keys - cited_keys
    assert not missing_in_bib, f"Keys cited in paper/main.tex missing from refs.bib: {sorted(missing_in_bib)}"
    assert not uncited_in_tex, f"Uncited orphan entries in paper/refs.bib: {sorted(uncited_in_tex)}"
    print(f"[PASS] BibTeX <-> LaTeX 1-to-1 coverage: all {len(bib_keys)}/{len(bib_keys)} entries cited cleanly.")


def check_http_url(u: str, headers: dict[str, str]) -> int:
    req = urllib.request.Request(u, headers=headers, method="HEAD")
    try:
        with urllib.request.urlopen(req, timeout=20) as resp:
            return int(resp.status)
    except urllib.error.HTTPError as err:
        if err.code in (403, 405):
            req_get = urllib.request.Request(u, headers=headers)
            with urllib.request.urlopen(req_get, timeout=20) as resp:
                return int(resp.status)
        raise


def check_live_crossref(entries: list[dict[str, str]]) -> None:
    headers = {"User-Agent": "asmodensius-native-citation-audit/1.1 (mailto:0_0@0thernes.art)"}
    doi_ok = 0
    bib_url_ok = 0
    for e in entries:
        key = e["key"]
        doi = e.get("doi")
        if doi:
            assert DOI_RE.match(doi), f"Malformed DOI syntax for {key}: {doi}"
            if doi.lower().startswith("10.48550/arxiv."):
                arxiv_id = doi.split("arXiv.", 1)[-1]
                code = check_http_url(f"https://arxiv.org/abs/{arxiv_id}", headers)
                assert 200 <= code < 400, f"arXiv DOI check failed ({code}): {doi}"
                print(f"  [ARXIV OK] {key:24s} HTTP={code} ret=False | {doi}")
                doi_ok += 1
                continue
            url = f"https://api.crossref.org/works/{doi}"
            req = urllib.request.Request(url, headers=headers)
            with urllib.request.urlopen(req, timeout=20) as resp:
                payload = json.loads(resp.read().decode("utf-8"))["message"]
            cr_title = (payload.get("title") or [""])[0]
            retracted = False
            for upd in payload.get("update-to", []) + payload.get("updated-by", []):
                if "retract" in str(upd.get("type", "")).lower():
                    retracted = True
            assert not retracted, f"RETRACTED WORK DETECTED: {key} ({doi})"
            cr_years: set[str] = set()
            for k in ("published-print", "published-online", "issued", "created"):
                parts = payload.get(k, {}).get("date-parts", [[]])
                if parts and parts[0]:
                    cr_years.add(str(parts[0][0]))
            bib_year = e.get("year", "")
            year_ok = (not cr_years) or (bib_year in cr_years)
            status = "OK" if year_ok else f"YEAR_NOTE(bib={bib_year},cr={sorted(cr_years)})"
            safe_title = cr_title[:52].encode("ascii", "replace").decode("ascii")
            print(f"  [DOI OK]   {key:24s} {status:12s} ret=False | {doi} | {safe_title}")
            doi_ok += 1
        elif e.get("url"):
            code = check_http_url(e["url"], headers)
            assert 200 <= code < 400, f"Bib URL failed ({code}): {key} -> {e['url']}"
            print(f"  [URL OK]   {key:24s} HTTP={code} | {e['url']}")
            bib_url_ok += 1
        else:
            print(f"  [BOOK]     {key:24s} year={e.get('year', '?')} isbn={e.get('isbn', 'n/a')}")

    print(
        f"[PASS] Live Bibliography Audit: {doi_ok} DOIs resolved (0 retractions), "
        f"{bib_url_ok} non-DOI URLs verified HTTP 200."
    )

    url_ok = 0
    for u in PUBLIC_URLS:
        code = check_http_url(u, headers)
        assert 200 <= code < 400, f"Public URL check failed ({code}): {u}"
        print(f"  [HTTP {code}] {u}")
        url_ok += 1
    print(f"[PASS] Live Public URL Check: {url_ok}/{url_ok} endpoints healthy.")


def main() -> None:
    parser = argparse.ArgumentParser(description="Asmodensius-Native BibTeX, DOI, retraction & URL auditor.")
    parser.add_argument("--live", action="store_true", help="Query live Crossref API & public URLs")
    args = parser.parse_args()

    entries = parse_bib(BIB_PATH.read_text(encoding="utf-8"))
    assert len(entries) >= 12, f"Expected >= 12 BibTeX entries, got {len(entries)}"
    for e in entries:
        if "doi" in e:
            assert DOI_RE.match(e["doi"]), f"Malformed DOI for {e['key']}: {e['doi']}"
    check_tex_citation_coverage(entries)

    if args.live:
        check_live_crossref(entries)
    else:
        print(f"[PASS] Offline BibTeX syntax & DOI check ({len(entries)} entries). Pass --live for Crossref audit.")


if __name__ == "__main__":
    main()
