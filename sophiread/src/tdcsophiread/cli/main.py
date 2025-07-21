#!/usr/bin/env python3
"""
TDCSophiread Command Line Interface

High-performance TDC-only TPX3 data processing with Python interface.
Replaces C++ CLI applications with more maintainable Python implementation.
"""

import argparse
import json
import os
import sys
import time
from pathlib import Path
from typing import Optional, Dict, Any

import numpy as np
import h5py

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


def validate_config_file(config_path: str) -> Dict[str, Any]:
    """
    Validate and load configuration file

    Args:
        config_path: Path to JSON configuration file

    Returns:
        Loaded configuration dictionary

    Raises:
        SystemExit: If configuration is invalid
    """
    if not os.path.exists(config_path):
        print(f"❌ Configuration file not found: {config_path}")
        sys.exit(1)

    try:
        with open(config_path, 'r') as f:
            config_dict = json.load(f)
        print(f"✅ Loaded configuration from {config_path}")
        return config_dict
    except json.JSONDecodeError as e:
        print(f"❌ Invalid JSON in configuration file: {e}")
        sys.exit(1)
    except Exception as e:
        print(f"❌ Error loading configuration: {e}")
        sys.exit(1)


def save_hits_to_hdf5(hits: Dict[str, np.ndarray], output_path: str,
                      metadata: Optional[Dict] = None) -> None:
    """
    Save hits to HDF5 file format

    Args:
        hits: Dictionary of numpy arrays with hit data
        output_path: Output HDF5 file path
        metadata: Optional metadata to include
    """
    print(f"💾 Saving {len(hits['x']):,} hits to {output_path}")

    with h5py.File(output_path, 'w') as f:
        # Create hits dataset
        hits_group = f.create_group('hits')

        for field, data in hits.items():
            hits_group.create_dataset(field, data=data, compression='gzip')

        # Add metadata
        if metadata:
            attrs = f.attrs
            for key, value in metadata.items():
                attrs[key] = value

        # Add processing timestamp
        attrs['processed_timestamp'] = time.time()
        attrs['processor'] = 'TDCSophiread'
        attrs['version'] = tdcsophiread.__version__

    print(f"✅ Saved to {output_path}")


def create_tof_spectrum(hits: Dict[str, np.ndarray], tof_range_ms: tuple = (0, 20),
                       num_bins: int = 1000) -> tuple:
    """
    Create time-of-flight spectrum

    Args:
        hits: Hit data dictionary
        tof_range_ms: TOF range in milliseconds
        num_bins: Number of histogram bins

    Returns:
        (bin_centers_ms, counts)
    """
    tof_ms = hits['tof'] * 25 / 1e6  # Convert to milliseconds

    # Create histogram
    counts, bin_edges = np.histogram(tof_ms, bins=num_bins,
                                   range=tof_range_ms)
    bin_centers = (bin_edges[:-1] + bin_edges[1:]) / 2

    return bin_centers, counts


def print_processing_summary(hits: Dict[str, np.ndarray],
                           processing_time: float,
                           input_file: str) -> None:
    """Print summary of processing results"""
    print(f"\n📊 Processing Summary")
    print(f"{'='*50}")
    print(f"Input file: {input_file}")
    print(f"File size: {os.path.getsize(input_file) / 1024 / 1024:.1f} MB")
    print(f"Total hits: {len(hits['x']):,}")
    print(f"Processing time: {processing_time:.3f} seconds")
    print(f"Processing rate: {len(hits['x']) / processing_time / 1e6:.1f} M hits/sec")

    if len(hits['x']) > 0:
        print(f"\n🎯 Hit Statistics:")
        print(f"X range: {hits['x'].min()} - {hits['x'].max()}")
        print(f"Y range: {hits['y'].min()} - {hits['y'].max()}")
        print(f"TOF range: {hits['tof'].min()} - {hits['tof'].max()} (25ns units)")
        print(f"TOT range: {hits['tot'].min()} - {hits['tot'].max()}")

        unique_chips, chip_counts = np.unique(hits['chip_id'], return_counts=True)
        print(f"Active chips: {list(unique_chips)}")
        for chip, count in zip(unique_chips, chip_counts):
            percentage = 100 * count / len(hits['chip_id'])
            print(f"  Chip {chip}: {count:,} hits ({percentage:.1f}%)")


def main():
    """Main CLI function"""
    parser = argparse.ArgumentParser(
        description="TDCSophiread: High-performance TDC-only TPX3 data processor",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Basic processing with VENUS defaults
  %(prog)s -i data.tpx3 -o hits.h5

  # Custom configuration with parallel processing
  %(prog)s -i data.tpx3 -o hits.h5 -c config.json --parallel --threads 8

  # Generate TOF spectrum
  %(prog)s -i data.tpx3 -o hits.h5 --tof-spectrum tof_spectrum.txt

  # Verbose output with performance metrics
  %(prog)s -i data.tpx3 -o hits.h5 --verbose
        """
    )

    # Input/Output arguments
    parser.add_argument('-i', '--input', required=True,
                       help='Input TPX3 file path')
    parser.add_argument('-o', '--output',
                       help='Output HDF5 file path (not required in benchmark mode)')

    # Configuration arguments
    parser.add_argument('-c', '--config',
                       help='JSON configuration file (default: VENUS detector)')
    parser.add_argument('--tdc-frequency', type=float, default=60.0,
                       help='TDC frequency in Hz (default: 60.0)')
    parser.add_argument('--disable-tdc-correction', action='store_true',
                       help='Disable missing TDC correction')

    # Processing arguments
    parser.add_argument('--parallel', action='store_true', default=True,
                       help='Enable parallel processing (default: enabled)')
    parser.add_argument('--no-parallel', dest='parallel', action='store_false',
                       help='Disable parallel processing')
    parser.add_argument('--threads', type=int, default=0,
                       help='Number of threads (0 = auto-detect)')

    # Output arguments
    parser.add_argument('--tof-spectrum',
                       help='Generate TOF spectrum and save to file')
    parser.add_argument('--tof-range', nargs=2, type=float, default=[0, 20],
                       help='TOF range for spectrum in ms (default: 0 20)')
    parser.add_argument('--tof-bins', type=int, default=1000,
                       help='Number of TOF bins (default: 1000)')

    # Utility arguments
    parser.add_argument('-v', '--verbose', action='store_true',
                       help='Verbose output')
    parser.add_argument('--benchmark', action='store_true',
                       help='Benchmark mode - skip file writing to focus on processing performance')
    parser.add_argument('--version', action='version',
                       version=f'TDCSophiread {tdcsophiread.__version__}')

    args = parser.parse_args()

    # Validate input file
    if not os.path.exists(args.input):
        print(f"❌ Input file not found: {args.input}")
        sys.exit(1)

    # Validate output argument
    if not args.benchmark and not args.output:
        print(f"❌ Output file required unless using --benchmark mode")
        sys.exit(1)

    # Create output directory if needed (skip in benchmark mode)
    if not args.benchmark:
        output_dir = os.path.dirname(args.output)
        if output_dir and not os.path.exists(output_dir):
            os.makedirs(output_dir)
            if args.verbose:
                print(f"📁 Created output directory: {output_dir}")

    # Load configuration
    if args.config:
        config_dict = validate_config_file(args.config)
        config = tdcsophiread.DetectorConfig.from_json(config_dict)
        if args.verbose:
            print(f"🔧 Using custom configuration from {args.config}")
    else:
        config = tdcsophiread.DetectorConfig.venus_defaults()
        if args.verbose:
            print("🔧 Using VENUS detector defaults")

    # Create processor
    processor = tdcsophiread.TDCProcessor(config)

    # Apply TDC correction setting
    if args.disable_tdc_correction:
        processor.set_missing_tdc_correction_enabled(False)
        if args.verbose:
            print("⚠️ Missing TDC correction disabled")

    if args.verbose:
        print(f"📁 Input: {args.input}")
        if not args.benchmark:
            print(f"💾 Output: {args.output}")
        print(f"⚡ Parallel processing: {args.parallel}")
        if args.parallel and args.threads > 0:
            print(f"🧵 Threads: {args.threads}")
        if args.benchmark:
            print(f"🏁 Benchmark mode: Skipping file I/O for pure processing performance")

    # Process file
    print("🚀 Processing TPX3 data...")
    start_time = time.time()

    try:
        if args.parallel:
            hits_vec = processor.process_file_parallel(args.input, args.threads)
        else:
            hits_vec = processor.process_file(args.input)

        # Convert to numpy arrays
        hits = tdcsophiread.hits_to_numpy(hits_vec)
        processing_time = time.time() - start_time

    except Exception as e:
        print(f"❌ Processing failed: {e}")
        sys.exit(1)

    # Print summary
    if args.verbose or len(hits['x']) == 0:
        print_processing_summary(hits, processing_time, args.input)
    else:
        print(f"✅ Processed {len(hits['x']):,} hits in {processing_time:.3f}s "
              f"({len(hits['x']) / processing_time / 1e6:.1f} M hits/sec)")

    if not args.benchmark:
        # Save to HDF5
        metadata = {
            'input_file': args.input,
            'processing_time_seconds': processing_time,
            'parallel_processing': args.parallel,
            'tdc_correction_enabled': not args.disable_tdc_correction,
            'tdc_frequency_hz': config.get_tdc_frequency()
        }

        if args.threads > 0:
            metadata['num_threads'] = args.threads

        save_hits_to_hdf5(hits, args.output, metadata)
    else:
        print(f"🏁 Benchmark complete - skipped file writing")

    # Generate TOF spectrum if requested (skip in benchmark mode)
    if args.tof_spectrum and len(hits['x']) > 0 and not args.benchmark:
        print(f"📈 Generating TOF spectrum...")
        bin_centers, counts = create_tof_spectrum(hits,
                                                 tuple(args.tof_range),
                                                 args.tof_bins)

        # Save spectrum
        np.savetxt(args.tof_spectrum,
                  np.column_stack([bin_centers, counts]),
                  header='TOF_ms Counts',
                  fmt='%.6f %d')
        print(f"✅ TOF spectrum saved to {args.tof_spectrum}")

    if args.benchmark:
        print("🎉 Benchmark completed successfully!")
    else:
        print("🎉 Processing completed successfully!")


if __name__ == '__main__':
    main()