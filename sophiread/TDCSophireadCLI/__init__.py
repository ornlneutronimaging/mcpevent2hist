"""
TDCSophiread CLI Applications

Python-based command line tools for high-performance TPX3 data processing.
Provides improved usability and maintainability over C++ equivalents.
"""

__version__ = "2.0.0"
__author__ = "ORNL Neutron Imaging Team"

# Make CLI tools importable
from . import tdcsophiread_cli
from . import venus_auto_reducer
from . import config_validator

__all__ = ["tdcsophiread_cli", "venus_auto_reducer", "config_validator"]
