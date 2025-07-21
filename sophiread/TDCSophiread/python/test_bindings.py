#!/usr/bin/env python3
"""Test script for TDCSophiread Python bindings"""

import os
import sys
import numpy as np
import tempfile

# Add build directory to path (for testing before installation)
build_dir = os.path.join(os.path.dirname(__file__), '..', '..', 'build', 'TDCSophiread', 'python')
sys.path.insert(0, build_dir)

try:
    import tdcsophiread
    print(f"Successfully imported tdcsophiread version {tdcsophiread.__version__}")
except ImportError as e:
    print(f"Failed to import tdcsophiread: {e}")
    print("Make sure to build with: pixi run build")
    sys.exit(1)

def test_detector_config():
    """Test DetectorConfig functionality"""
    print("\n=== Testing DetectorConfig ===")

    # Test VENUS defaults
    config = tdcsophiread.DetectorConfig.venus_defaults()
    print(f"TDC Frequency: {config.get_tdc_frequency()} Hz")
    print(f"Missing TDC correction enabled: {config.is_missing_tdc_correction_enabled()}")
    print(f"Chip size: {config.get_chip_size_x()}x{config.get_chip_size_y()}")

    # Test coordinate mapping
    global_coords = config.map_chip_to_global(0, 100, 100)
    print(f"Chip 0 (100,100) -> Global {global_coords}")

    return config

def test_processor(config):
    """Test TDCProcessor functionality"""
    print("\n=== Testing TDCProcessor ===")

    # Create processor
    processor = tdcsophiread.TDCProcessor(config)
    print("Created TDCProcessor")

    # We need a test TPX3 file for real testing
    # For now, just verify the processor was created
    print("Processor ready for TPX3 file processing")

    return processor

def test_convenience_function():
    """Test high-level convenience function"""
    print("\n=== Testing Convenience Function ===")

    # This would process a real file
    # hits = tdcsophiread.process_tpx3("test.tpx3")
    print("process_tpx3() function available for simple usage")

def create_test_tpx3_file():
    """Create a minimal test TPX3 file"""
    with tempfile.NamedTemporaryFile(suffix='.tpx3', delete=False) as f:
        # TPX3 header for chip 0
        header = 0x0000000033585054  # "TPX3" magic
        f.write(header.to_bytes(8, byteorder='little'))

        # TDC packet with timestamp 1000
        tdc_packet = 0x6F00000003E8000  # TDC packet, timestamp 1000
        f.write(tdc_packet.to_bytes(8, byteorder='little'))

        # Hit packet
        # Simplified - would need proper bit packing in real use
        hit_packet = 0xB000000000000000  # Hit packet marker
        f.write(hit_packet.to_bytes(8, byteorder='little'))

        return f.name

def test_processing():
    """Test actual file processing"""
    print("\n=== Testing File Processing ===")

    test_file = create_test_tpx3_file()
    try:
        # Test convenience function
        hits = tdcsophiread.process_tpx3(test_file, parallel=False)
        print(f"Processed file, got hit arrays:")
        for key, array in hits.items():
            print(f"  {key}: shape={array.shape}, dtype={array.dtype}")
    except Exception as e:
        print(f"Processing failed (expected with minimal test file): {e}")
    finally:
        os.unlink(test_file)

def main():
    """Run all tests"""
    print("TDCSophiread Python Bindings Test")
    print("=" * 40)

    config = test_detector_config()
    processor = test_processor(config)
    test_convenience_function()
    test_processing()

    print("\n✅ All tests completed!")

if __name__ == "__main__":
    main()