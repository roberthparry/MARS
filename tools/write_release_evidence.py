#!/usr/bin/env python3
"""Write machine-specific dependency evidence for a MARS binary release."""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import os
import platform
import re
import subprocess
import sys
from pathlib import Path
from typing import Any


REPOSITORY_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_LIBRARY = REPOSITORY_ROOT / "build/release/libmars.so"
DEFAULT_OUTPUT = REPOSITORY_ROOT / "build/compliance/release-evidence.json"

LDD_LIBRARY = re.compile(r"^\s*(\S+)\s+=>\s+(\S+)\s+\(0x[0-9a-fA-F]+\)\s*$")
LDD_MISSING = re.compile(r"^\s*(\S+)\s+=>\s+not found\s*$")
LDD_LOADER = re.compile(r"^\s*(/\S+)\s+\(0x[0-9a-fA-F]+\)\s*$")


def run(command: list[str]) -> subprocess.CompletedProcess[str]:
    """Run a command without raising when an optional probe is unavailable."""
    try:
        return subprocess.run(command, cwd=REPOSITORY_ROOT, text=True, capture_output=True, check=False)
    except OSError as error:
        return subprocess.CompletedProcess(command, 127, stdout="", stderr=str(error))


def first_line(command: list[str]) -> str | None:
    """Return the first non-empty output line from a version probe."""
    completed = run(command)
    if completed.returncode != 0:
        return None
    for line in (completed.stdout + completed.stderr).splitlines():
        if line.strip():
            return line.strip()
    return None


def sha256(path: Path) -> str:
    """Return the SHA-256 digest of a file."""
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def git_evidence() -> dict[str, Any]:
    """Return the source revision and worktree state."""
    commit = run(["git", "rev-parse", "HEAD"])
    status = run(["git", "status", "--porcelain"])
    return {
        "commit": commit.stdout.strip() if commit.returncode == 0 else None,
        "worktree_dirty": bool(status.stdout.strip()) if status.returncode == 0 else None,
    }


def tool_versions() -> dict[str, str | None]:
    """Return versions for release-relevant build and runtime tools."""
    probes = {
        "cc": [os.environ.get("CC", "gcc"), "--version"],
        "dvisvgm": ["dvisvgm", "--version"],
        "latex": ["latex", "--version"],
        "pkg-config": ["pkg-config", "--version"],
        "python3": ["python3", "--version"],
        "sqlcipher": ["sqlcipher", "--version"],
    }
    return {name: first_line(command) for name, command in probes.items()}


def pkg_config_versions() -> dict[str, str | None]:
    """Return versions reported by available pkg-config metadata."""
    modules = ("gmp", "mpfr", "mpc", "libunistring", "sqlcipher")
    return {module: first_line(["pkg-config", "--modversion", module]) for module in modules}


def compliance_record_hashes() -> dict[str, str]:
    """Hash the legal and dependency records accompanying the source revision."""
    paths = ("LICENSE", "THIRD_PARTY_NOTICES.md", "DEPENDENCIES.spdx", "docs/compliance-status.md")
    return {path: sha256(REPOSITORY_ROOT / path) for path in paths}


def package_owner(path: Path) -> dict[str, str] | None:
    """Return the Debian package owning a resolved library path, when available."""
    owner = run(["dpkg-query", "-S", str(path)])
    if owner.returncode != 0 or not owner.stdout.strip():
        return None

    package_spec = owner.stdout.split(": ", 1)[0].strip()
    details = run(["dpkg-query", "-W", "-f=${binary:Package}\t${Version}\t${Architecture}\n", package_spec])
    if details.returncode != 0 or not details.stdout.strip():
        return {"package": package_spec}

    package, version, architecture = details.stdout.strip().split("\t", 2)
    base_package = package.split(":", 1)[0]
    copyright_file = Path("/usr/share/doc") / base_package / "copyright"
    result = {"package": package, "version": version, "architecture": architecture}
    if copyright_file.is_file():
        result["copyright_file"] = str(copyright_file)
        result["copyright_sha256"] = sha256(copyright_file)
    return result


def dynamic_libraries(artifact: Path) -> list[dict[str, Any]]:
    """Return the resolved dynamic-library closure reported by ldd."""
    completed = run(["ldd", str(artifact)])
    if completed.returncode != 0:
        raise RuntimeError(completed.stderr.strip() or f"ldd could not inspect {artifact}")

    libraries: list[dict[str, Any]] = []
    for line in completed.stdout.splitlines():
        match = LDD_LIBRARY.match(line)
        soname: str | None = None
        resolved: str | None = None
        if match:
            soname, resolved = match.groups()
        else:
            missing = LDD_MISSING.match(line)
            loader = LDD_LOADER.match(line)
            if missing:
                soname = missing.group(1)
            elif loader:
                resolved = loader.group(1)
                soname = Path(resolved).name
        if not soname:
            continue

        entry: dict[str, Any] = {"soname": soname, "resolved_path": resolved}
        if resolved:
            path = Path(resolved).resolve()
            if path.is_file():
                entry["resolved_path"] = str(path)
                entry["sha256"] = sha256(path)
                entry["system_package"] = package_owner(path)
        libraries.append(entry)
    return libraries


def write_evidence(artifact: Path, output: Path, allow_dirty: bool = False) -> None:
    """Write the release evidence as deterministic, readable JSON."""
    if not artifact.is_file():
        raise FileNotFoundError(f"release artefact does not exist: {artifact}")

    source = git_evidence()
    if source["worktree_dirty"] and not allow_dirty:
        raise RuntimeError("release evidence requires a clean worktree; use --allow-dirty only for a local trial")

    libraries = dynamic_libraries(artifact)
    missing_libraries = [entry["soname"] for entry in libraries if not entry["resolved_path"]]
    if missing_libraries:
        raise RuntimeError(f"release artefact has unresolved dynamic libraries: {', '.join(missing_libraries)}")

    evidence = {
        "schema": "MARS release dependency evidence 1",
        "generated_at_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "source": source,
        "host": {
            "platform": platform.platform(),
            "machine": platform.machine(),
            "python": platform.python_version(),
        },
        "tools": tool_versions(),
        "pkg_config_modules": pkg_config_versions(),
        "compliance_records": compliance_record_hashes(),
        "artefact": {
            "path": str(artifact),
            "sha256": sha256(artifact),
            "dynamic_libraries": libraries,
        },
        "distribution_note": (
            "This supplements DEPENDENCIES.spdx for one built artefact. A distributor bundling any listed library "
            "must include the exact licence and source-availability material required by that library."
        ),
    }
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(evidence, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def main() -> int:
    """Parse arguments and write release evidence."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--library", type=Path, default=DEFAULT_LIBRARY, help="release library to inspect")
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT, help="JSON evidence file to write")
    parser.add_argument(
        "--allow-dirty",
        action="store_true",
        help="permit a dirty worktree for a local trial; never use this for published release evidence",
    )
    arguments = parser.parse_args()

    artifact = arguments.library if arguments.library.is_absolute() else REPOSITORY_ROOT / arguments.library
    output = arguments.output if arguments.output.is_absolute() else REPOSITORY_ROOT / arguments.output
    try:
        write_evidence(artifact.resolve(), output.resolve(), arguments.allow_dirty)
    except (OSError, RuntimeError, ValueError) as error:
        print(f"could not write release evidence: {error}", file=sys.stderr)
        return 1

    print(output.resolve())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
