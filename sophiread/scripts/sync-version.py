#!/usr/bin/env python3
"""
Synchronize version between include/version.h and pyproject.toml

This script reads the version from include/version.h and updates
pyproject.toml to match, ensuring consistency.
"""

import re
import sys
from pathlib import Path

def extract_version_from_header():
    """Extract version from include/version.h"""
    version_h = Path("include/version.h")
    if not version_h.exists():
        print("Error: include/version.h not found")
        sys.exit(1)

    content = version_h.read_text()

    # Extract version components
    major_match = re.search(r'#define\s+VERSION_MAJOR\s+(\d+)', content)
    minor_match = re.search(r'#define\s+VERSION_MINOR\s+(\d+)', content)
    patch_match = re.search(r'#define\s+VERSION_PATCH\s+(\d+)', content)

    if not (major_match and minor_match and patch_match):
        print("Error: Could not find version defines in include/version.h")
        sys.exit(1)

    major = major_match.group(1)
    minor = minor_match.group(1)
    patch = patch_match.group(1)

    return f"{major}.{minor}.{patch}"

def update_pyproject_version(version):
    """Update version in pyproject.toml"""
    pyproject_toml = Path("pyproject.toml")
    if not pyproject_toml.exists():
        print("Error: pyproject.toml not found")
        sys.exit(1)

    content = pyproject_toml.read_text()

    # Update version line
    updated_content = re.sub(
        r'version\s*=\s*"[^"]*"',
        f'version = "{version}"',
        content
    )

    # Also update _version.py if it exists
    version_py = Path("src/tdcsophiread/_version.py")
    if version_py.exists():
        version_py_content = f'# Version file generated from include/version.h\n# This is automatically updated by the build system\n\n__version__ = "{version}"\n'
        version_py.write_text(version_py_content)
        print(f"Updated src/tdcsophiread/_version.py to {version}")

    if content != updated_content:
        pyproject_toml.write_text(updated_content)
        print(f"Updated pyproject.toml version to {version}")
    else:
        print(f"pyproject.toml already has version {version}")

def main():
    """Main function"""
    print("Synchronizing version from include/version.h to pyproject.toml...")

    # Extract version from header
    version = extract_version_from_header()
    print(f"Found version {version} in include/version.h")

    # Update pyproject.toml
    update_pyproject_version(version)

    print("Version synchronization complete!")

if __name__ == "__main__":
    main()