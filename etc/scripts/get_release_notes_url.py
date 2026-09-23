#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0
"""Extract the release notes URL for a given version from the AppStream metainfo.

Usage:
    python3 get_release_notes_url.py VERSION

Reads ``etc/de.desy.constellation.metainfo.xml`` and looks for the ``<release>``
entry with the matching ``version`` attribute. If found, the ``<url type="details">``
of that release is printed to stdout. If no matching release (or no URL) exists,
nothing is printed, which callers can use to omit release notes entirely.
"""

import sys
import xml.etree.ElementTree as ET
from pathlib import Path


def find_release_notes_url(version: str, metainfo_path: Path) -> str | None:
    """Return the ``details`` URL for ``version`` or ``None`` if there is none."""
    tree = ET.parse(metainfo_path)
    root = tree.getroot()
    for release in root.findall("./releases/release"):
        if release.get("version") == version:
            url = release.find("url[@type='details']")
            if url is not None and url.text:
                return url.text.strip()
            return None
    return None


def main() -> int:
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} VERSION", file=sys.stderr)
        return 1
    version = sys.argv[1]

    # The metainfo file lives next to this script's directory (etc/scripts -> etc).
    metainfo_path = Path(__file__).resolve().parent.parent / "de.desy.constellation.metainfo.xml"
    if not metainfo_path.is_file():
        print(f"Metainfo file not found: {metainfo_path}", file=sys.stderr)
        return 1

    url = find_release_notes_url(version, metainfo_path)
    if url:
        print(url)
    return 0


if __name__ == "__main__":
    sys.exit(main())
