#!/usr/bin/env python3
"""
Configuration Validator for TDCSophiread using Pydantic v2

Validates JSON configuration files with type safety and detailed error reporting.
Provides schema generation and comprehensive validation.
"""

import argparse
import json
import sys
from pathlib import Path
from typing import List, Optional, Dict, Any

from pydantic import BaseModel, Field, field_validator, model_validator
from pydantic.json_schema import GenerateJsonSchema

# Import TDCSophiread
try:
    import tdcsophiread
except ImportError:
    # For development, try relative import
    try:
        from .. import tdcsophiread
    except ImportError:
        # Add build directory to path for development
        build_dir = Path(__file__).parent.parent.parent.parent / 'build' / 'TDCSophiread' / 'python'
        sys.path.insert(0, str(build_dir))
        import tdcsophiread


class ChipTransform(BaseModel):
    """2x3 affine transformation matrix for chip coordinate mapping"""

    chip_id: int = Field(..., ge=0, le=3, description="Chip identifier (0-3)")
    transform: List[List[float]] = Field(
        ...,
        description="2x3 affine transformation matrix [[a,b,tx], [c,d,ty]]"
    )

    @field_validator('transform')
    @classmethod
    def validate_transform_matrix(cls, v):
        if len(v) != 2:
            raise ValueError("Transform matrix must have exactly 2 rows")
        for i, row in enumerate(v):
            if len(row) != 3:
                raise ValueError(f"Transform matrix row {i} must have exactly 3 elements")
            if not all(isinstance(x, (int, float)) for x in row):
                raise ValueError(f"Transform matrix row {i} must contain only numbers")
        return v


class DetectorConfig(BaseModel):
    """Detector configuration section"""

    name: str = Field(default="VENUS_TPX3", description="Detector name")
    chip_size_x: int = Field(default=256, ge=1, le=1024, description="Chip width in pixels")
    chip_size_y: int = Field(default=256, ge=1, le=1024, description="Chip height in pixels")
    super_resolution_factor: int = Field(
        default=4,
        ge=1,
        le=16,
        description="Super-resolution factor (sub-pixels per pixel)"
    )
    chips: List[ChipTransform] = Field(
        default_factory=list,
        description="Chip transformation configurations"
    )

    @field_validator('super_resolution_factor')
    @classmethod
    def validate_super_resolution(cls, v):
        if v not in [1, 2, 4, 8, 16]:
            raise ValueError(f"Super-resolution factor {v} not recommended (use 1, 2, 4, 8, or 16)")
        return v

    @model_validator(mode='after')
    def validate_chip_consistency(self):
        if self.chips:
            chip_ids = [chip.chip_id for chip in self.chips]
            if len(set(chip_ids)) != len(chip_ids):
                raise ValueError("Duplicate chip IDs found")
            if not all(0 <= chip_id <= 3 for chip_id in chip_ids):
                raise ValueError("Chip IDs must be between 0 and 3")
        return self


class TimingConfig(BaseModel):
    """Timing configuration section"""

    tdc_frequency: float = Field(
        default=60.0,
        gt=0,
        le=1000,
        description="TDC frequency in Hz"
    )
    enable_missing_tdc_correction: bool = Field(
        default=True,
        description="Enable missing TDC correction algorithm"
    )

    @field_validator('tdc_frequency')
    @classmethod
    def validate_tdc_frequency(cls, v):
        if v < 10 or v > 200:
            # This is a warning, not an error, so we'll just return the value
            # Warnings will be handled separately
            pass
        return v


class ProcessingConfig(BaseModel):
    """Processing configuration section"""

    parallel_enabled: bool = Field(default=True, description="Enable parallel processing")
    default_threads: int = Field(default=0, ge=0, description="Default thread count (0=auto)")
    chunk_size_mb: int = Field(default=1024, ge=1, description="Processing chunk size in MB")


class MetadataConfig(BaseModel):
    """Metadata configuration section"""

    description: Optional[str] = Field(default=None, description="Configuration description")
    facility: Optional[str] = Field(default=None, description="Facility name")
    instrument: Optional[str] = Field(default=None, description="Instrument name")
    created_by: Optional[str] = Field(default=None, description="Configuration creator")
    version: Optional[str] = Field(default=None, description="Configuration version")


class TDCConfig(BaseModel):
    """Complete TDCSophiread configuration"""

    detector: DetectorConfig = Field(default_factory=DetectorConfig)
    timing: TimingConfig = Field(default_factory=TimingConfig)
    processing: ProcessingConfig = Field(default_factory=ProcessingConfig)
    metadata: Optional[MetadataConfig] = Field(default=None)

    @model_validator(mode='after')
    def validate_complete_config(self):
        # Cross-section validations can go here
        return self


def generate_warnings(config: TDCConfig) -> List[str]:
    """Generate warnings for unusual but valid configurations"""
    warnings = []

    # TDC frequency warnings
    if config.timing.tdc_frequency < 10 or config.timing.tdc_frequency > 200:
        warnings.append(
            f"TDC frequency {config.timing.tdc_frequency} Hz is unusual (typical: 60 Hz)"
        )

    # Missing TDC correction warning
    if not config.timing.enable_missing_tdc_correction:
        warnings.append("Missing TDC correction is disabled - may affect data quality")

    # Chip size warnings
    if config.detector.chip_size_x != 256 or config.detector.chip_size_y != 256:
        warnings.append(
            f"Non-standard chip size: {config.detector.chip_size_x}x{config.detector.chip_size_y} "
            f"(standard: 256x256)"
        )

    # Super-resolution warnings
    if config.detector.super_resolution_factor not in [4, 8]:
        warnings.append(
            f"Super-resolution factor {config.detector.super_resolution_factor} "
            f"is unusual (typical: 4 or 8)"
        )

    # Missing chips warning
    if not config.detector.chips:
        warnings.append("No chip transformations defined - will use default identity transforms")
    elif len(config.detector.chips) < 4:
        missing_chips = set(range(4)) - {chip.chip_id for chip in config.detector.chips}
        warnings.append(f"Missing chip transformations for chips: {sorted(missing_chips)}")

    return warnings


def load_and_validate_config(config_path: str) -> tuple[bool, TDCConfig | None, List[str], List[str]]:
    """
    Load and validate configuration file using Pydantic

    Returns:
        (success, config, errors, warnings)
    """
    try:
        with open(config_path, 'r') as f:
            raw_config = json.load(f)
    except FileNotFoundError:
        return False, None, [f"Configuration file not found: {config_path}"], []
    except json.JSONDecodeError as e:
        return False, None, [f"Invalid JSON: {e}"], []
    except Exception as e:
        return False, None, [f"Error loading file: {e}"], []

    try:
        # Validate with Pydantic
        config = TDCConfig.model_validate(raw_config)

        # Generate warnings
        warnings = generate_warnings(config)

        return True, config, [], warnings

    except Exception as e:
        # Format Pydantic validation errors nicely
        if hasattr(e, 'errors'):
            errors = []
            for error in e.errors():
                loc = " -> ".join(str(x) for x in error['loc'])
                errors.append(f"{loc}: {error['msg']}")
        else:
            errors = [str(e)]

        return False, None, errors, []


def test_config_loading(config: TDCConfig) -> bool:
    """
    Test loading configuration with TDCSophiread

    Returns:
        True if successful
    """
    try:
        # Convert to dict and then to TDCSophiread config
        config_dict = config.model_dump()
        tdc_config = tdcsophiread.DetectorConfig.from_json(config_dict)

        print(f"✅ Configuration loaded successfully with TDCSophiread")
        print(f"   TDC frequency: {tdc_config.get_tdc_frequency()} Hz")
        print(f"   Missing TDC correction: {tdc_config.is_missing_tdc_correction_enabled()}")
        print(f"   Chip size: {tdc_config.get_chip_size_x()}x{tdc_config.get_chip_size_y()}")

        # Test coordinate mapping for each chip
        for chip_id in range(4):
            try:
                global_coords = tdc_config.map_chip_to_global(chip_id, 100, 100)
                print(f"   Chip {chip_id} (100,100) -> Global {global_coords}")
            except Exception as e:
                print(f"   ⚠️ Chip {chip_id} mapping error: {e}")
                return False

        return True

    except Exception as e:
        print(f"❌ Failed to load with TDCSophiread: {e}")
        return False


def create_example_config(output_path: str) -> None:
    """Create an example configuration file using Pydantic"""

    # Create configuration with explicit values
    config = TDCConfig(
        detector=DetectorConfig(
            name="VENUS_TPX3",
            chip_size_x=256,
            chip_size_y=256,
            super_resolution_factor=4,
            chips=[
                ChipTransform(chip_id=0, transform=[[1.0, 0.0, 258.0], [0.0, 1.0, 0.0]]),
                ChipTransform(chip_id=1, transform=[[-1.0, 0.0, 513.0], [0.0, -1.0, 513.0]]),
                ChipTransform(chip_id=2, transform=[[-1.0, 0.0, 255.0], [0.0, -1.0, 513.0]]),
                ChipTransform(chip_id=3, transform=[[1.0, 0.0, 0.0], [0.0, 1.0, 0.0]])
            ]
        ),
        timing=TimingConfig(
            tdc_frequency=60.0,
            enable_missing_tdc_correction=True
        ),
        processing=ProcessingConfig(
            parallel_enabled=True,
            default_threads=0,
            chunk_size_mb=1024
        ),
        metadata=MetadataConfig(
            description="VENUS detector configuration for TPX3 neutron imaging",
            facility="SNS",
            instrument="VENUS",
            created_by="TDCSophiread config_validator",
            version="1.0"
        )
    )

    # Export to JSON
    with open(output_path, 'w') as f:
        json.dump(config.model_dump(), f, indent=2)

    print(f"✅ Example configuration created: {output_path}")


def generate_json_schema(output_path: str) -> None:
    """Generate JSON schema for configuration validation"""
    schema = TDCConfig.model_json_schema()

    with open(output_path, 'w') as f:
        json.dump(schema, f, indent=2)

    print(f"✅ JSON schema generated: {output_path}")


def main():
    """Main CLI function"""
    parser = argparse.ArgumentParser(
        description="Configuration validator for TDCSophiread (Pydantic v2)",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Validate existing configuration
  %(prog)s config.json

  # Create example configuration
  %(prog)s --create-example venus_config.json

  # Generate JSON schema
  %(prog)s --generate-schema config_schema.json

  # Validate and test loading
  %(prog)s config.json --test-load

  # Validate with detailed output
  %(prog)s config.json --verbose
        """
    )

    parser.add_argument('config_file', nargs='?',
                       help='Configuration file to validate')
    parser.add_argument('--create-example', metavar='OUTPUT_FILE',
                       help='Create example configuration file')
    parser.add_argument('--generate-schema', metavar='SCHEMA_FILE',
                       help='Generate JSON schema file')
    parser.add_argument('--test-load', action='store_true',
                       help='Test loading configuration with TDCSophiread')
    parser.add_argument('-v', '--verbose', action='store_true',
                       help='Verbose output')
    parser.add_argument('--version', action='version',
                       version=f'TDCSophiread Config Validator {tdcsophiread.__version__}')

    args = parser.parse_args()

    # Generate JSON schema
    if args.generate_schema:
        generate_json_schema(args.generate_schema)
        return

    # Create example configuration
    if args.create_example:
        create_example_config(args.create_example)
        return

    # Validate configuration file
    if not args.config_file:
        parser.error("Configuration file required (or use --create-example/--generate-schema)")

    if not Path(args.config_file).exists():
        print(f"❌ Configuration file not found: {args.config_file}")
        sys.exit(1)

    print(f"🔍 Validating configuration: {args.config_file}")

    # Load and validate with Pydantic
    success, config, errors, warnings = load_and_validate_config(args.config_file)

    # Report results
    if errors:
        print(f"\n❌ Validation failed with {len(errors)} error(s):")
        for error in errors:
            print(f"   • {error}")
    else:
        print(f"✅ Configuration structure is valid")

    if warnings:
        print(f"\n⚠️ {len(warnings)} warning(s):")
        for warning in warnings:
            print(f"   • {warning}")

    # Show configuration details if verbose
    if args.verbose and config:
        print(f"\n📋 Configuration Details:")
        print(f"   Detector: {config.detector.name}")
        print(f"   Chip size: {config.detector.chip_size_x}x{config.detector.chip_size_y}")
        print(f"   Super-resolution: {config.detector.super_resolution_factor}x")
        print(f"   TDC frequency: {config.timing.tdc_frequency} Hz")
        print(f"   TDC correction: {config.timing.enable_missing_tdc_correction}")
        print(f"   Parallel processing: {config.processing.parallel_enabled}")
        print(f"   Configured chips: {len(config.detector.chips)}")

    # Test loading if requested and validation passed
    if args.test_load and success and config:
        print(f"\n🧪 Testing configuration loading...")
        load_success = test_config_loading(config)

        if load_success:
            print(f"✅ Configuration loads and functions correctly")
        else:
            success = False

    # Exit with appropriate code
    if success:
        print(f"\n🎉 Configuration validation completed successfully!")
        sys.exit(0)
    else:
        print(f"\n💥 Configuration validation failed!")
        sys.exit(1)


if __name__ == '__main__':
    main()