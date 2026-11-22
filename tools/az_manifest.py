"""Record or verify the source inventory used for a native evaluation.

Usage: python -I tools/az_manifest.py [--root ROOT] [--output FILE | --check FILE]
"""
import argparse
import hashlib
import os
from datetime import datetime, timezone
from pathlib import Path
import sys
import tempfile

SOURCE_DIRECTORIES = ("core", "kernels", "platform", "tools", "gpu")


def source_names(root):
    names = ["CMakeLists.txt"]
    for directory in SOURCE_DIRECTORIES:
        base = root / directory
        if not base.is_dir():
            raise ValueError("Missing source directory " + str(base))
        for parent, dirs, filenames in os.walk(base, onerror=raise_walk_error):
            for name in filenames:
                names.append((Path(parent) / name).relative_to(root).as_posix())
    return sorted(names)


def raise_walk_error(error):
    raise error


def inventory(root):
    names = source_names(root)
    # A worker may finish rebuilding a file while the inventory is being read.
    # Reject a mixed revision even if every relative filename stayed the same.
    def fingerprint(name):
        entry = (root / name).stat()
        return entry.st_dev, entry.st_ino, entry.st_size, entry.st_mtime_ns
    before = {name: fingerprint(name) for name in names}
    def digest_pass():
        lines = []
        for name in names:
            if "\n" in name or "\r" in name:
                raise ValueError("A source filename contains a line break")
            h = hashlib.sha256()
            with (root / name).open("rb") as source:
                for block in iter(lambda: source.read(1048576), b""):
                    h.update(block)
            lines.append(h.hexdigest() + "  " + name + "\n")
        return lines

    # Same-length rewrites can preserve a file's modification time.
    lines = digest_pass()
    if (lines != digest_pass() or names != source_names(root) or
            any(before[name] != fingerprint(name) for name in names)):
        raise ValueError("The source inventory changed during hashing")
    return "".join(lines).encode("utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    group = parser.add_mutually_exclusive_group()
    group.add_argument("--output", type=Path, help="Write a manifest at this path")
    group.add_argument("--check", type=Path, help="Verify all source files against this manifest")
    args = parser.parse_args()
    root = args.root.resolve()
    date = datetime.now(timezone.utc).date().isoformat()
    target = (args.check or args.output or (root / "docs/eval" / ("ENGINE-MANIFEST-" + date + ".txt"))).resolve()
    if target == root / "CMakeLists.txt" or any((root / name) in target.parents for name in SOURCE_DIRECTORIES):
        parser.error("The manifest must be outside the source inventory")
    data = inventory(root)
    digest = hashlib.sha256(data).hexdigest()
    if args.check:
        if target.read_bytes() != data:
            raise ValueError("Source inventory differs from " + str(target))
    elif target.is_file() and target.read_bytes() != data:
        raise ValueError("This manifest records a different revision. Choose a new --output path.")
    elif not target.is_file():
        target.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.NamedTemporaryFile(dir=target.parent, delete=False) as temp:
            temp.write(data)
            temp_path = Path(temp.name)
        try:
            # Windows rename refuses replacement, preserving independently frozen revisions.
            try:
                temp_path.rename(target)
            except FileExistsError:
                if target.read_bytes() != data:
                    raise ValueError("Concurrent write recorded a different source inventory")
        finally:
            temp_path.unlink(missing_ok=True)
    file_count = len(data.splitlines())
    print(f"{digest}  {target} ({file_count} files)")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError) as error:
        print(error, file=sys.stderr)
        sys.exit(1)

