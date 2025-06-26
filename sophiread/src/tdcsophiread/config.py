"""
Configuration management for TDCSophiread

Provides Pydantic models for type-safe configuration validation
and JSON schema generation.
"""

from .cli.config_validator import (
    TDCConfig,
    DetectorConfig as PydanticDetectorConfig,
    TimingConfig,
    ProcessingConfig,
    MetadataConfig,
    ChipTransform as PydanticChipTransform,
    load_and_validate_config,
    generate_warnings,
    create_example_config,
    generate_json_schema,
)

__all__ = [
    "TDCConfig",
    "PydanticDetectorConfig",
    "TimingConfig",
    "ProcessingConfig",
    "MetadataConfig",
    "PydanticChipTransform",
    "load_and_validate_config",
    "generate_warnings",
    "create_example_config",
    "generate_json_schema",
]