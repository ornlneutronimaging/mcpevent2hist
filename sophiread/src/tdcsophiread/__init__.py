"""
TDCSophiread: High-performance TDC-only TPX3 neutron imaging data processor

A modern Python package for processing TPX3 detector data with TDC timing,
providing 150x performance improvement over pure Python implementations
while maintaining scientific accuracy.
"""

# Import version from _version.py (updated by build system)
from ._version import __version__
__author__ = "ORNL Neutron Imaging Team"
__email__ = "neutronimaging@ornl.gov"

# Import core functionality
try:
    # Try to import the compiled extension
    from ._core import *  # noqa: F403, F401
except ImportError as e:
    raise ImportError(
        "Failed to import TDCSophiread C++ extension. "
        "Make sure the package was installed correctly. "
        f"Error: {e}"
    ) from e

# Import Python modules
from . import config
from . import cli
from . import analysis

__all__ = [
    # Version info
    "__version__",
    "__author__",
    "__email__",
    # Core classes (from C++ extension)
    "DetectorConfig",
    "TDCProcessor",
    "TDCStreamProcessor",
    "TDCHit",
    "ChipTransform",
    # Exception classes
    "TDCProcessingError",
    "TDCFileError",
    "TDCConfigError",
    # Utility functions
    "process_tpx3",
    "process_tpx3_stream",
    # Python modules
    "config",
    "cli",
    "analysis",
]
