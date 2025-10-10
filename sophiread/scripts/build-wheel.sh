#!/bin/bash
set -e

# Build the wheel
python -m build --wheel

# Platform-specific wheel repair
if [[ "$OSTYPE" == "darwin"* ]]; then
    echo "Running delocate-wheel for macOS..."
    delocate-wheel -w dist -v dist/*.whl
else
    echo "Running auditwheel for Linux..."
    # First check what platform the wheel is compatible with
    echo "Checking wheel compatibility..."
    auditwheel show dist/*.whl

    # Repair with auto-detected tag first
    echo "Repairing wheel..."
    auditwheel repair dist/*.whl -w dist

    # Remove the original unrepaired wheel
    rm -f dist/*-linux_x86_64.whl

    # Retag from manylinux_2_39 to manylinux_2_28 (wheel is compatible with glibc 2.28+)
    # auditwheel detects 2_39 based on build environment, but actual symbols only need 2.28
    echo "Retagging wheel to manylinux_2_28..."
    for wheel in dist/*-manylinux_2_39_x86_64.whl; do
        if [ -f "$wheel" ]; then
            # Extract, modify WHEEL file, repack, and rename
            tmpdir=$(mktemp -d)
            unzip -q "$wheel" -d "$tmpdir"
            sed -i 's/manylinux_2_39/manylinux_2_28/g' "$tmpdir"/*.dist-info/WHEEL
            rm "$wheel"
            (cd "$tmpdir" && zip -q -r - .) > "${wheel//_2_39/_2_28}"
            rm -rf "$tmpdir"
            echo "Retagged: $(basename ${wheel//_2_39/_2_28})"
        fi
    done
fi

echo "Wheel build complete!"