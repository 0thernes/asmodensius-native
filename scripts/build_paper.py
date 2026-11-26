#!/usr/bin/env python3
"""Asmodensius-Native - Release PDF & Standalone arXiv Tarball Packager + Verifier.

1. Compiles paper/main.tex via Tectonic (--keep-intermediates --keep-logs).
2. Fails on ANY Overfull \\hbox, LaTeX Warning: Reference/Citation, Undefined, or Missing character.
3. Copies the frozen release PDF to dist/Donahue_2026_asmodensius_native.pdf.
4. Packages a deterministic dist/arxiv_submission.tar.gz (mtime=0) containing main.tex,
   main.bbl, refs.bib, and any paper/figures/*.pdf files.
5. Extracts dist/arxiv_submission.tar.gz into a fresh temporary directory and
   verifies clean-room standalone compilation via Tectonic with zero errors.
"""

from __future__ import annotations

import gzip
import hashlib
import io
import os
import shutil
import subprocess
import sys
import tarfile
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PAPER = ROOT / "paper"
BUILD = PAPER / "build"
DIST = ROOT / "dist"


def find_tectonic() -> str:
    found = shutil.which("tectonic")
    if found:
        return found
    win_default = Path.home() / ".local" / "bin" / "tectonic.exe"
    if win_default.exists():
        return str(win_default)
    raise FileNotFoundError("tectonic executable not found on PATH or ~/.local/bin/tectonic.exe")


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def check_log_clean(log_path: Path, label: str) -> None:
    log_text = log_path.read_text(encoding="utf-8", errors="replace")
    bad_tokens = (
        "Overfull \\hbox",
        "LaTeX Warning: Reference",
        "LaTeX Warning: Citation",
        "LaTeX Warning: There were undefined references",
        "Missing character:",
        "Undefined control sequence",
    )
    bad = [line for line in log_text.splitlines() if any(tok in line for tok in bad_tokens)]
    if bad:
        raise RuntimeError(f"LaTeX quality gate failed for {label}:\n" + "\n".join(bad))
    print(f"[PASS] Zero Overfull \\hbox / undefined refs / undefined citations / missing chars in {label}.")


def make_deterministic_tar_gz(tar_out: Path, files: list[tuple[Path, str]]) -> None:
    buf = io.BytesIO()
    with tarfile.open(fileobj=buf, mode="w") as tf:
        for src, arcname in files:
            data = src.read_bytes()
            info = tarfile.TarInfo(name=arcname)
            info.size = len(data)
            info.mtime = 0
            info.mode = 0o644
            info.uid = 0
            info.gid = 0
            info.uname = ""
            info.gname = ""
            tf.addfile(info, io.BytesIO(data))
    with tar_out.open("wb") as raw_f:
        with gzip.GzipFile(filename="", mode="wb", fileobj=raw_f, mtime=0) as gz:
            gz.write(buf.getvalue())


def count_pdf_pages(pdf_path: Path, log_path: Path) -> int:
    import re

    if log_path.exists():
        log_text = log_path.read_text(encoding="utf-8", errors="replace")
        m = re.search(r"Output written on [^\n(]+\((\d+)\s+pages?", log_text)
        if m:
            return int(m.group(1))
    raw = pdf_path.read_bytes()
    counts = [int(m) for m in re.findall(rb"/Type\s*/Pages\b[^>]*?/Count\s+(\d+)", raw)]
    if counts:
        return max(counts)
    return len(re.findall(rb"/Type\s*/Page\b(?!s)", raw))


def main() -> None:
    tectonic = find_tectonic()
    BUILD.mkdir(parents=True, exist_ok=True)
    DIST.mkdir(parents=True, exist_ok=True)

    env = os.environ.copy()
    tmp_root = ROOT / "build" / "tmp"
    tmp_root.mkdir(parents=True, exist_ok=True)
    env["TMP"] = str(tmp_root)
    env["TEMP"] = str(tmp_root)

    res = subprocess.run(
        [tectonic, "-X", "compile", "main.tex", "--outdir", "build", "--keep-intermediates", "--keep-logs"],
        cwd=PAPER,
        env=env,
        capture_output=True,
        text=True,
        check=False,
    )
    if res.returncode != 0:
        print(res.stdout)
        print(res.stderr, file=sys.stderr)
        raise RuntimeError("Tectonic failed compiling paper/main.tex")

    check_log_clean(BUILD / "main.log", "paper/main.tex")

    pdf_out = DIST / "Donahue_2026_asmodensius_native.pdf"
    shutil.copy2(BUILD / "main.pdf", pdf_out)

    tar_files: list[tuple[Path, str]] = [
        (PAPER / "main.tex", "main.tex"),
        (BUILD / "main.bbl", "main.bbl"),
        (PAPER / "refs.bib", "refs.bib"),
    ]
    fig_dir = PAPER / "figures"
    if fig_dir.is_dir():
        for fig_pdf in sorted(fig_dir.glob("*.pdf")):
            tar_files.append((fig_pdf, f"figures/{fig_pdf.name}"))

    tar_out = DIST / "arxiv_submission.tar.gz"
    make_deterministic_tar_gz(tar_out, tar_files)

    with tempfile.TemporaryDirectory(prefix="az_arxiv_check_", dir=tmp_root) as tmpdir:
        tmp = Path(tmpdir)
        with tarfile.open(tar_out, "r:gz") as tf:
            tf.extractall(tmp, filter="data")
        check = subprocess.run(
            [tectonic, "-X", "compile", "main.tex", "--keep-logs"],
            cwd=tmp,
            env=env,
            capture_output=True,
            text=True,
            check=False,
        )
        if check.returncode != 0 or not (tmp / "main.pdf").exists():
            print(check.stdout)
            print(check.stderr, file=sys.stderr)
            raise RuntimeError("Clean-room arXiv tarball failed compilation!")
        check_log_clean(tmp / "main.log", "clean-room arxiv_submission.tar.gz")

    pages = count_pdf_pages(pdf_out, BUILD / "main.log")
    print(f"[PASS] Standalone arXiv tarball verified in fresh temp dir: {tar_out.name}")
    print(f"PDF PAGES:     {pages}")
    print(f"PDF SHA-256:   {sha256_file(pdf_out)}  dist/{pdf_out.name} ({pdf_out.stat().st_size:,} bytes)")
    print(f"ARXIV SHA-256: {sha256_file(tar_out)}  dist/{tar_out.name} ({tar_out.stat().st_size:,} bytes)")


if __name__ == "__main__":
    main()
