#!/usr/bin/env python3
"""Verify repository compliance records that can be checked mechanically."""

from __future__ import annotations

import argparse
import collections
import hashlib
import re
import subprocess
import sys
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parent.parent

REQUIRED_TRACKED_PATHS = {
    ".githooks/pre-commit",
    "DEPENDENCIES.spdx",
    "LICENSE",
    "THIRD_PARTY_NOTICES.md",
    "docs/almanac-data-provenance.md",
    "docs/compliance-status.md",
    "docs/licensing.md",
    "docs/privacy.md",
    "docs/visual-asset-provenance.md",
    "tools/check_compliance.py",
    "tools/check_public_distribution.py",
    "tools/write_release_evidence.py",
}

REQUIRED_ROOT_DOCUMENTS = {"LICENSE", "THIRD_PARTY_NOTICES.md", "DEPENDENCIES.spdx"}
REQUIRED_GUIDE_DOCUMENTS = {
    "docs/almanac-data-provenance.md",
    "docs/compliance-status.md",
    "docs/licensing.md",
    "docs/privacy.md",
    "docs/visual-asset-provenance.md",
}

REQUIRED_SPDX_PACKAGES = {
    "SPDXRef-Package-MARS",
    "SPDXRef-Package-SQLCipher",
    "SPDXRef-Package-SQLite",
    "SPDXRef-Package-TZDB",
    "SPDXRef-Package-Unicode-CLDR",
    "SPDXRef-Package-WeatherAPI",
    "SPDXRef-Package-DE440",
    "SPDXRef-Package-DE440s",
    "SPDXRef-Package-NAIF-Auxiliary-Kernels",
}

CHECKSUM_ROW = re.compile(r"^\|\s*`([^`]+)`\s*\|\s*`([0-9a-f]{64})`\s*\|\s*$")
SPDX_ID = re.compile(r"^SPDXID:\s*(\S+)\s*$")
SPDX_RELATIONSHIP = re.compile(r"^Relationship:\s*(\S+)\s+\S+\s+(\S+)\s*$")


def read_text(path: str) -> str:
    """Read a UTF-8 repository file."""
    return (REPOSITORY_ROOT / path).read_text(encoding="utf-8")


def tracked_paths() -> set[str]:
    """Return paths currently present in the Git index."""
    completed = subprocess.run(
        ["git", "ls-files", "-z"], cwd=REPOSITORY_ROOT, check=True, capture_output=True
    )
    return set(completed.stdout.decode("utf-8", errors="surrogateescape").rstrip("\0").split("\0"))


def make_variable_paths(makefile: str, variable: str) -> set[str]:
    """Read a simple whitespace-separated path variable from the Makefile."""
    match = re.search(rf"^{re.escape(variable)}\s*:=\s*(.+)$", makefile, flags=re.MULTILINE)
    return set(match.group(1).split()) if match else set()


def sha256(path: Path) -> str:
    """Return the SHA-256 digest of a file."""
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def check_required_paths(errors: list[str], allow_untracked: bool) -> None:
    """Check that the compliance records and controls are present and, normally, tracked."""
    if allow_untracked:
        missing = {path for path in REQUIRED_TRACKED_PATHS if not (REPOSITORY_ROOT / path).is_file()}
    else:
        missing = REQUIRED_TRACKED_PATHS - tracked_paths()
    for path in sorted(missing):
        requirement = "present" if allow_untracked else "tracked"
        errors.append(f"required compliance path is not {requirement}: {path}")


def check_installed_documents(errors: list[str]) -> None:
    """Check that installation retains every required legal document."""
    makefile = read_text("Makefile")
    root_documents = make_variable_paths(makefile, "LEGAL_ROOT_DOCUMENTS")
    guide_documents = make_variable_paths(makefile, "LEGAL_GUIDE_DOCUMENTS")

    for path in sorted(REQUIRED_ROOT_DOCUMENTS - root_documents):
        errors.append(f"legal root document is not installed: {path}")
    for path in sorted(REQUIRED_GUIDE_DOCUMENTS - guide_documents):
        errors.append(f"legal guide document is not installed: {path}")


def check_almanac_checksums(errors: list[str]) -> None:
    """Check every distributed-file digest recorded by the almanac provenance."""
    rows = [CHECKSUM_ROW.match(line) for line in read_text("docs/almanac-data-provenance.md").splitlines()]
    checksums = [(match.group(1), match.group(2)) for match in rows if match]
    if not checksums:
        errors.append("almanac provenance contains no machine-checkable SHA-256 rows")
        return

    for relative_path, expected in checksums:
        path = REPOSITORY_ROOT / relative_path
        if not path.is_file():
            errors.append(f"provenance file is missing: {relative_path}")
            continue
        actual = sha256(path)
        if actual != expected:
            errors.append(f"provenance checksum mismatch for {relative_path}: expected {expected}, found {actual}")


def check_spdx(errors: list[str]) -> None:
    """Check basic SPDX structure and internal relationship references."""
    lines = read_text("DEPENDENCIES.spdx").splitlines()
    if "SPDXVersion: SPDX-2.3" not in lines:
        errors.append("dependency inventory is not declared as SPDX 2.3")
    if "DataLicense: CC0-1.0" not in lines:
        errors.append("dependency inventory does not use the required CC0 SPDX data licence")

    identifier_list = [match.group(1) for line in lines if (match := SPDX_ID.match(line))]
    identifiers = set(identifier_list)
    for identifier, count in sorted(collections.Counter(identifier_list).items()):
        if count > 1:
            errors.append(f"SPDX identifier is declared {count} times: {identifier}")
    missing_packages = REQUIRED_SPDX_PACKAGES - identifiers
    for identifier in sorted(missing_packages):
        errors.append(f"required SPDX package is missing: {identifier}")

    for line in lines:
        relationship = SPDX_RELATIONSHIP.match(line)
        if not relationship:
            continue
        for identifier in relationship.groups():
            if identifier not in identifiers:
                errors.append(f"SPDX relationship references an unknown identifier: {identifier}")


def check_notices(errors: list[str]) -> None:
    """Check that externally governed data and services retain their notices."""
    notices = read_text("THIRD_PARTY_NOTICES.md")
    required_markers = {
        "SQLCipher Community Edition": "SQLCipher notice",
        "Unicode CLDR week data": "Unicode CLDR notice",
        "Astronomical data and generation tools": "astronomical-data notice",
        "WeatherAPI.com": "WeatherAPI notice",
        "https://www.weatherapi.com/terms.aspx": "WeatherAPI terms link",
        "https://naif.jpl.nasa.gov/naif/rules.html": "NAIF use-rules link",
    }
    for marker, description in required_markers.items():
        if marker not in notices:
            errors.append(f"third-party notices are missing the {description}")


def check_weather_integration(errors: list[str]) -> None:
    """Check that the optional weather integration retains its public safeguards."""
    mars_lab = read_text("tools/mars_lab.py")
    required_markers = {
        "MARS_WEATHER_API_KEY": "user-supplied API-key setting",
        "WEATHER_SAFETY_NOTICE": "end-user weather safety notice",
        "https://www.weatherapi.com/": "WeatherAPI attribution link",
        "https://www.weatherapi.com/privacy.aspx": "WeatherAPI privacy link",
        "https://www.weatherapi.com/terms.aspx": "WeatherAPI terms link",
    }
    for marker, description in required_markers.items():
        if marker not in mars_lab:
            errors.append(f"weather integration is missing the {description}")
    if "WeatherAPI" not in read_text("docs/privacy.md"):
        errors.append("privacy notice does not describe the optional WeatherAPI integration")


def main() -> int:
    """Run the mechanical compliance checks."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--allow-untracked",
        action="store_true",
        help="accept compliance files present in the working tree before their first commit",
    )
    arguments = parser.parse_args()

    errors: list[str] = []
    try:
        check_required_paths(errors, arguments.allow_untracked)
        check_installed_documents(errors)
        check_almanac_checksums(errors)
        check_spdx(errors)
        check_notices(errors)
        check_weather_integration(errors)
    except (OSError, subprocess.CalledProcessError) as error:
        print(f"compliance check could not inspect the repository: {error}", file=sys.stderr)
        return 2

    if errors:
        print("compliance checks failed:", file=sys.stderr)
        for error in errors:
            print(f"  {error}", file=sys.stderr)
        return 1

    print("compliance records, installed notices, SPDX references and provenance checksums are consistent")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
