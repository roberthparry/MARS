#!/usr/bin/env python3
"""Reject repository paths that contain private ESAA reference material."""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parent.parent
FORBIDDEN_PREFIXES = ("docs/books/esaa/",)
FORBIDDEN_PATHS = {
    "docs/explanatory supplement to the astronomical almanac.md",
    "src/almanac/explanatory supplement to the astronomical almanac.pdf",
}


def normalise_repository_path(path: str) -> str:
    """Return a case-insensitive, forward-slash repository path."""
    normalised = path.replace("\\", "/")
    while normalised.startswith("./"):
        normalised = normalised[2:]
    return normalised.casefold()


def is_forbidden(path: str) -> bool:
    """Return whether a repository path is reserved for private reference material."""
    normalised = normalise_repository_path(path)
    return normalised in FORBIDDEN_PATHS or any(normalised.startswith(prefix) for prefix in FORBIDDEN_PREFIXES)


def git_paths(staged_only: bool) -> list[str]:
    """Read tracked paths, or newly staged paths, from the repository index."""
    if staged_only:
        command = ["git", "diff", "--cached", "--name-only", "--diff-filter=ACMR", "-z"]
    else:
        command = ["git", "ls-files", "-z"]

    completed = subprocess.run(command, cwd=REPOSITORY_ROOT, check=True, capture_output=True)
    return completed.stdout.decode("utf-8", errors="surrogateescape").rstrip("\0").split("\0")


def main() -> int:
    """Check the index and report any private paths included in public distribution."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--staged",
        action="store_true",
        help="check only added, copied, modified or renamed paths staged for the next commit",
    )
    arguments = parser.parse_args()

    try:
        forbidden = sorted(path for path in git_paths(arguments.staged) if path and is_forbidden(path))
    except subprocess.CalledProcessError as error:
        detail = error.stderr.decode("utf-8", errors="replace").strip()
        print(f"public-distribution check could not inspect the Git index: {detail}", file=sys.stderr)
        return 2

    if not forbidden:
        return 0

    print("private ESAA reference material must not be included in the public repository:", file=sys.stderr)
    for path in forbidden:
        print(f"  {path}", file=sys.stderr)
    print("keep the local files in their ignored locations and remove them from the Git index", file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
