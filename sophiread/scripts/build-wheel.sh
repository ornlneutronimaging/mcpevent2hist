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
    auditwheel repair dist/*.whl -w dist
    # Remove the original unrepaired wheel
    rm -f dist/*-linux_x86_64.whl
fi

echo "Wheel build complete!"