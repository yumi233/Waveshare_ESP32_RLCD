#!/usr/bin/env python3
# Copyright (c) 2026 玉米. MIT License; upstream notices retained.
"""Archive a committed source tree and bind its checksum to the commit."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import subprocess
import tempfile
import zipfile


ROOT = Path(__file__).resolve().parents[1]


def git(*args):
    return subprocess.check_output(["git", *args], cwd=ROOT)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ref", default="HEAD", help="Committed revision or tag")
    parser.add_argument("--version", required=True, help="Archive version, e.g. 1.0.0-rc.3")
    parser.add_argument("--output", type=Path, default=ROOT / "release/artifacts")
    args = parser.parse_args()
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._-]*", args.version):
        parser.error("Version must contain only letters, digits, dots, underscores and hyphens")
    commit = git("rev-parse", "--verify", "--end-of-options", args.ref + "^{commit}").decode().strip()
    paths = git("ls-tree", "-rz", "--name-only", commit).decode().rstrip("\0").split("\0")
    forbidden = {"reporter.json", "electricity-history.json", "electricity-history.tmp", "private.pem", "full-flash.bin", "after-flash.bin"}
    for name in paths:
        path = PurePosixPath(name)
        if (path.name in forbidden or path.name == ".env" or path.name.startswith(".env.")
                or path.suffix in {".key", ".log"} or path.name.startswith("flash-")
                or any(part in {".venv", ".venv-macos", "__pycache__", "backups"} for part in path.parts)):
            raise SystemExit("Refusing private/runtime path: " + name)
    args.output.mkdir(parents=True, exist_ok=True)
    filename = "XiaNing-source-" + args.version + ".zip"
    archive = args.output / filename
    manifest_path = args.output / "SHA256SUMS.json"
    if archive.exists() or manifest_path.exists():
        raise SystemExit("Output already exists; choose a new output directory")
    with tempfile.TemporaryDirectory(prefix="xianing-source-") as temporary:
        candidate = Path(temporary) / filename
        subprocess.run(["git", "archive", "--format=zip", "--prefix=" + archive.stem + "/",
                        "--output=" + str(candidate), commit], cwd=ROOT, check=True)
        with zipfile.ZipFile(candidate) as source:
            corrupt = source.testzip()
            if corrupt:
                raise SystemExit("Invalid ZIP entry: " + corrupt)
            file_count = sum(not entry.is_dir() for entry in source.infolist())
        digest = hashlib.sha256(candidate.read_bytes()).hexdigest()
        manifest = {"version": args.version, "source_commit": commit,
                    "scope": "source-only", "file_count": file_count,
                    "files": [{"name": filename, "size_bytes": candidate.stat().st_size, "sha256": digest}]}
        archive.write_bytes(candidate.read_bytes())
    manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(manifest, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
