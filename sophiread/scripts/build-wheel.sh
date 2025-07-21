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

    # Repair to manylinux_2_34 which matches our build environment
    echo "Repairing to manylinux_2_34_x86_64..."
    auditwheel repair dist/*.whl -w dist --plat manylinux_2_34_x86_64

    # Remove the original unrepaired wheel
    rm -f dist/*-linux_x86_64.whl
fi

echo "Wheel build complete!"