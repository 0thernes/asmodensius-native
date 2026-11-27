#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 Alexander Stephen Donahue
"""Asmodensius-Native v1.2.0 - Live Citation, DOI, Title/Author Alignment & Retraction Auditor.

Verifies every BibTeX entry in `paper/refs.bib`:
  1. 1-to-1 coverage with `paper/main.tex` (zero uncited orphans, zero missing keys).
  2. Strict DOI regex validation.
  3. With `--live`:
     - Crossref REST API lookup for all publisher DOIs: HTTP 200 reachability,
       title token alignment against `paper/refs.bib`, author surname alignment,
       publication year check, and retraction metadata check (`update-to`/`updated-by`).
     - arXiv Atom API lookup for `10.48550/arXiv.*` DOIs: HTTP 200 reachability,
       title alignment, and author surname alignment.
     - Live HTTP reachability for vendor manual URLs (`url = {...}`).
     - Explicit transparent status for published book ISBN / ISO standards.
     - Live reachability for repository and ORCID public endpoints.
     - Writes structured audit JSON to `results/citation-audit.json` (or `--out`).

Usage:
  python3 -I scripts/check_citations.py             # Offline syntax + 1-to-1 coverage check
  python3 -I scripts/check_citations.py --live      # Live Crossref/arXiv/HTTP + title/author check
"""

from __future__ import annotations

import argparse
import datetime as dt
import json
import re
import unicodedata
import urllib.error
import urllib.request
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BIB_PATH = ROOT / "paper" / "refs.bib"
TEX_PATH = ROOT / "paper" / "main.tex"
DEFAULT_OUT = ROOT / "results" / "citation-audit.json"

PUBLIC_URLS = [
    "https://github.com/0thernes/asmodensius-native",
    "https://orcid.org/0009-0006-8168-9648",
]

DOI_RE = re.compile(r"^10\.\d{4,9}/[-._;()/:A-Za-z0-9]+$")


def strip_braces(s: str) -> str:
    return re.sub(r"[{}]", "", s).strip()


def norm_tokens(s: str) -> set[str]:
    clean = unicodedata.normalize("NFKD", strip_braces(s)).encode("ascii", "ignore").decode("ascii").lower()
    clean = re.sub(r"[^a-z0-9]+", " ", clean)
    stop = {"a", "an", "the", "of", "in", "on", "for", "and", "to", "with", "as", "by", "from"}
    return {t for t in clean.split() if len(t) >= 2 and t not in stop}


def bib_surnames(author_field: str) -> set[str]:
    surnames: set[str] = set()
    for person in re.split(r"\s+and\s+", author_field):
        p = strip_braces(person)
        if not p:
            continue
        if "," in p:
            fam = p.split(",", 1)[0].strip()
        else:
            fam = p.split()[-1].strip()
        toks = norm_tokens(fam)
        surnames.update(toks)
    return surnames


def parse_bib(text: str) -> list[dict[str, str]]:
    """Balanced-brace BibTeX parser."""
    entries: list[dict[str, str]] = []
    i = 0
    n = len(text)
    while i < n:
        at = text.find("@", i)
        if at < 0:
            break
        m = re.match(r"@(\w+)\s*\{\s*([^,\s]+)\s*,", text[at:])
        if not m:
            i = at + 1
            continue
        etype, key = m.group(1), m.group(2).strip()
        pos = at + m.end()
        depth = 1
        start_body = pos
        while pos < n and depth > 0:
            ch = text[pos]
            if ch == "{":
                depth += 1
            elif ch == "}":
                depth -= 1
            pos += 1
        body = text[start_body : pos - 1]
        entry: dict[str, str] = {"type": etype, "key": key}
        # Parse field = { ... } or field = "..."
        j = 0
        bn = len(body)
        while j < bn:
            fm = re.search(r"([A-Za-z][A-Za-z0-9_-]*)\s*=\s*", body[j:])
            if not fm:
                break
            fname = fm.group(1).lower()
            vstart = j + fm.end()
            while vstart < bn and body[vstart].isspace():
                vstart += 1
            if vstart >= bn:
                break
            if body[vstart] == "{":
                d = 1
                k = vstart + 1
                while k < bn and d > 0:
                    if body[k] == "{":
                        d += 1
                    elif body[k] == "}":
                        d -= 1
                    k += 1
                val = body[vstart + 1 : k - 1]
                j = k
            elif body[vstart] == '"':
                k = vstart + 1
                while k < bn and body[k] != '"':
                    k += 1
                val = body[vstart + 1 : k]
                j = k + 1
            else:
                k = vstart
                while k < bn and body[k] not in ",\n}":
                    k += 1
                val = body[vstart:k]
                j = k
            entry[fname] = re.sub(r"\s+", " ", val.strip())
        entries.append(entry)
        i = pos
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


def title_overlap(bib_title: str, remote_title: str) -> float:
    bt = norm_tokens(bib_title)
    rt = norm_tokens(remote_title)
    if not bt or not rt:
        return 0.0
    inter = len(bt & rt)
    return inter / min(len(bt), len(rt))


def check_live_crossref(entries: list[dict[str, str]], out_path: Path | None) -> None:
    headers = {"User-Agent": "asmodensius-native-citation-audit/1.2 (mailto:0_0@0thernes.art)"}
    doi_ok = 0
    bib_url_ok = 0
    book_std = 0
    results: list[dict[str, object]] = []

    for e in entries:
        key = e["key"]
        doi = e.get("doi")
        bib_title = e.get("title", "")
        bib_auth = e.get("author", "")
        b_surnames = bib_surnames(bib_auth)

        if doi:
            assert DOI_RE.match(doi), f"Malformed DOI syntax for {key}: {doi}"
            if doi.lower().startswith("10.48550/arxiv."):
                arxiv_id = doi.split("arXiv.", 1)[-1]
                abs_url = f"https://arxiv.org/abs/{arxiv_id}"
                code = check_http_url(abs_url, headers)
                assert 200 <= code < 400, f"arXiv URL check failed ({code}): {doi}"
                api_url = f"https://export.arxiv.org/api/query?id_list={arxiv_id}"
                req = urllib.request.Request(api_url, headers=headers)
                with urllib.request.urlopen(req, timeout=20) as resp:
                    xml_bytes = resp.read()
                root_el = ET.fromstring(xml_bytes)
                ns = {"a": "http://www.w3.org/2005/Atom"}
                ent_el = root_el.find("a:entry", ns)
                assert ent_el is not None, f"arXiv API returned no entry for {arxiv_id}"
                arx_title = re.sub(r"\s+", " ", (ent_el.findtext("a:title", default="", namespaces=ns) or "").strip())
                arx_authors = [
                    (a.findtext("a:name", default="", namespaces=ns) or "").strip()
                    for a in ent_el.findall("a:author", ns)
                ]
                arx_surnames = set()
                for nm in arx_authors:
                    if nm:
                        arx_surnames.update(norm_tokens(nm.split()[-1]))
                t_score = title_overlap(bib_title, arx_title)
                auth_overlap = bool(b_surnames & arx_surnames)
                assert t_score >= 0.75, f"arXiv title mismatch for {key}: bib='{bib_title}' vs arxiv='{arx_title}' ({t_score:.2f})"
                assert auth_overlap, f"arXiv author mismatch for {key}: bib={sorted(b_surnames)} vs arxiv={sorted(arx_surnames)}"
                safe_title = arx_title[:52].encode("ascii", "replace").decode("ascii")
                print(f"  [ARXIV OK] {key:24s} HTTP={code} title={t_score:.2f} auth=OK ret=False | {doi} | {safe_title}")
                results.append({
                    "key": key, "kind": "arxiv-doi", "doi": doi, "http_status": code,
                    "title_overlap": round(t_score, 3), "author_match": True, "retracted": False,
                    "remote_title": arx_title.encode("ascii", "replace").decode("ascii"),
                    "status": "PASS",
                })
                doi_ok += 1
                continue

            url = f"https://api.crossref.org/works/{doi}"
            req = urllib.request.Request(url, headers=headers)
            with urllib.request.urlopen(req, timeout=20) as resp:
                code = int(resp.status)
                payload = json.loads(resp.read().decode("utf-8"))["message"]
            cr_title = re.sub(r"\s+", " ", (payload.get("title") or [""])[0].strip())
            retracted = False
            for upd in payload.get("update-to", []) + payload.get("updated-by", []):
                if "retract" in str(upd.get("type", "")).lower():
                    retracted = True
            assert not retracted, f"RETRACTED WORK DETECTED: {key} ({doi})"

            cr_surnames: set[str] = set()
            for a in payload.get("author", []):
                fam = a.get("family") or a.get("name") or ""
                cr_surnames.update(norm_tokens(fam))
            t_score = title_overlap(bib_title, cr_title)
            auth_overlap = (not cr_surnames) or bool(b_surnames & cr_surnames)
            assert t_score >= 0.75, f"Crossref title mismatch for {key} ({doi}): bib='{bib_title}' vs crossref='{cr_title}' ({t_score:.2f})"
            assert auth_overlap, f"Crossref author mismatch for {key} ({doi}): bib={sorted(b_surnames)} vs crossref={sorted(cr_surnames)}"

            cr_years: set[str] = set()
            for k in ("published-print", "published-online", "issued", "created"):
                parts = payload.get(k, {}).get("date-parts", [[]])
                if parts and parts[0]:
                    cr_years.add(str(parts[0][0]))
            bib_year = e.get("year", "")
            year_ok = (not cr_years) or (bib_year in cr_years)
            y_tag = "OK" if year_ok else f"YEAR_NOTE(bib={bib_year},cr={sorted(cr_years)})"
            safe_title = cr_title[:52].encode("ascii", "replace").decode("ascii")
            print(f"  [DOI OK]   {key:24s} HTTP={code} title={t_score:.2f} auth=OK {y_tag} ret=False | {doi} | {safe_title}")
            results.append({
                "key": key, "kind": "crossref-doi", "doi": doi, "http_status": code,
                "title_overlap": round(t_score, 3), "author_match": auth_overlap,
                "year_match": year_ok, "bib_year": bib_year, "crossref_years": sorted(cr_years),
                "retracted": False,
                "remote_title": cr_title.encode("ascii", "replace").decode("ascii"),
                "status": "PASS",
            })
            doi_ok += 1
        elif e.get("url"):
            code = check_http_url(e["url"], headers)
            assert 200 <= code < 400, f"Bib URL failed ({code}): {key} -> {e['url']}"
            print(f"  [URL OK]   {key:24s} HTTP={code} | {e['url']}")
            results.append({
                "key": key, "kind": "vendor-url", "url": e["url"], "http_status": code,
                "status": "PASS",
            })
            bib_url_ok += 1
        else:
            print(f"  [BOOK/STD] {key:24s} year={e.get('year', '?')} isbn={e.get('isbn', e.get('number', 'n/a'))}")
            results.append({
                "key": key, "kind": "book-or-standard", "year": e.get("year", ""),
                "identifier": e.get("isbn") or e.get("number") or "n/a",
                "status": "CATALOGED",
            })
            book_std += 1

    print(
        f"[PASS] Live Bibliography Audit: {doi_ok} DOIs resolved with title+author match (0 retractions), "
        f"{bib_url_ok} vendor URLs verified HTTP 200, {book_std} book/ISO standards cataloged."
    )

    public_results: list[dict[str, object]] = []
    url_ok = 0
    for u in PUBLIC_URLS:
        code = check_http_url(u, headers)
        assert 200 <= code < 400, f"Public URL check failed ({code}): {u}"
        print(f"  [HTTP {code}] {u}")
        public_results.append({"url": u, "http_status": code, "status": "PASS"})
        url_ok += 1
    print(f"[PASS] Live Public URL Check: {url_ok}/{url_ok} endpoints healthy.")

    if out_path is not None:
        out_path.parent.mkdir(parents=True, exist_ok=True)
        report = {
            "schema_version": 1,
            "audited_utc": dt.datetime.now(dt.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
            "bib_file": "paper/refs.bib",
            "tex_file": "paper/main.tex",
            "total_entries": len(entries),
            "dois_verified_title_and_author": doi_ok,
            "vendor_urls_verified": bib_url_ok,
            "books_and_standards_cataloged": book_std,
            "retractions_detected": 0,
            "entries": results,
            "public_urls": public_results,
        }
        out_path.write_text(json.dumps(report, indent=2, ensure_ascii=True) + "\n", encoding="utf-8", newline="\n")
        print(f"[PASS] Saved structured citation audit to {out_path.relative_to(ROOT).as_posix()}.")


def main() -> None:
    parser = argparse.ArgumentParser(description="Asmodensius-Native BibTeX, DOI, title/author, retraction & URL auditor.")
    parser.add_argument("--live", action="store_true", help="Query live Crossref/arXiv APIs & public URLs")
    parser.add_argument("--out", default="", help="Optional path to write citation-audit JSON (defaults to results/citation-audit.json when --write-report is set)")
    parser.add_argument("--write-report", action="store_true", help="Write results/citation-audit.json during --live check")
    args = parser.parse_args()

    entries = parse_bib(BIB_PATH.read_text(encoding="utf-8"))
    assert len(entries) >= 12, f"Expected >= 12 BibTeX entries, got {len(entries)}"
    for e in entries:
        if "doi" in e:
            assert DOI_RE.match(e["doi"]), f"Malformed DOI for {e['key']}: {e['doi']}"
    check_tex_citation_coverage(entries)

    if args.live:
        out_path = Path(args.out) if args.out else (DEFAULT_OUT if args.write_report else None)
        check_live_crossref(entries, out_path)
    else:
        print(f"[PASS] Offline BibTeX syntax & DOI check ({len(entries)} entries). Pass --live for Crossref/arXiv audit.")


if __name__ == "__main__":
    main()
