#!/usr/bin/env python3
"""
VENUS Auto-Reducer for TDCSophiread

Python equivalent of the C++ venus_auto_reducer with improved functionality:
- Automatic directory monitoring and processing
- Batch processing of multiple TPX3 files
- TIFF output generation for TOF imaging
- Configurable processing intervals
- Better error handling and logging
"""

import argparse
import json
import os
import sys
import time
import glob
from pathlib import Path
from typing import List, Dict, Any, Optional
import threading
import queue

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

# Optional TIFF support
try:
    from PIL import Image
    TIFF_AVAILABLE = True
except ImportError:
    TIFF_AVAILABLE = False


class VenusAutoReducer:
    """Auto-reducer for VENUS instrument data processing"""

    def __init__(self, input_dir: str, output_dir: str, config_path: Optional[str] = None,
                 parallel: bool = True, interval: int = 30, verbose: bool = False):
        self.input_dir = Path(input_dir)
        self.output_dir = Path(output_dir)
        self.config_path = config_path
        self.parallel = parallel
        self.interval = interval
        self.verbose = verbose

        # Create output directory
        self.output_dir.mkdir(parents=True, exist_ok=True)

        # Load configuration
        if config_path:
            self.config = self._load_config(config_path)
        else:
            self.config = tdcsophiread.DetectorConfig.venus_defaults()

        # Create processor
        self.processor = tdcsophiread.TDCProcessor(self.config)

        # Tracking
        self.processed_files = set()
        self.processing_stats = []

        if self.verbose:
            print(f"🔧 VENUS Auto-Reducer initialized")
            print(f"   Input directory: {self.input_dir}")
            print(f"   Output directory: {self.output_dir}")
            print(f"   Parallel processing: {self.parallel}")
            print(f"   Monitoring interval: {self.interval}s")

    def _load_config(self, config_path: str) -> 'tdcsophiread.DetectorConfig':
        """Load configuration from file"""
        try:
            with open(config_path, 'r') as f:
                config_dict = json.load(f)
            return tdcsophiread.DetectorConfig.from_json(config_dict)
        except Exception as e:
            print(f"❌ Error loading config {config_path}: {e}")
            print("   Using VENUS defaults instead")
            return tdcsophiread.DetectorConfig.venus_defaults()

    def find_new_files(self) -> List[Path]:
        """Find new TPX3 files to process"""
        pattern = str(self.input_dir / "*.tpx3")
        all_files = [Path(f) for f in glob.glob(pattern)]

        # Filter out already processed files
        new_files = [f for f in all_files if f not in self.processed_files]

        # Sort by modification time (oldest first)
        new_files.sort(key=lambda f: f.stat().st_mtime)

        return new_files

    def process_file(self, input_file: Path) -> Dict[str, Any]:
        """Process a single TPX3 file"""
        if self.verbose:
            print(f"🚀 Processing {input_file.name}")

        start_time = time.time()

        try:
            # Process with TDCSophiread
            if self.parallel:
                hits_vec = self.processor.process_file_parallel(str(input_file))
            else:
                hits_vec = self.processor.process_file(str(input_file))

            # Convert to numpy
            hits = tdcsophiread.hits_to_numpy(hits_vec)
            processing_time = time.time() - start_time

            # Generate output paths
            base_name = input_file.stem
            hits_file = self.output_dir / f"{base_name}_hits.h5"

            # Save hits
            self._save_hits_hdf5(hits, hits_file, input_file, processing_time)

            # Generate TOF image if requested and TIFF available
            tiff_file = None
            if TIFF_AVAILABLE:
                tiff_file = self.output_dir / f"{base_name}_tof.tiff"
                self._save_tof_image(hits, tiff_file)

            # Create processing summary
            result = {
                'input_file': input_file,
                'output_hits': hits_file,
                'output_tiff': tiff_file,
                'num_hits': len(hits['x']),
                'processing_time': processing_time,
                'hits_per_sec': len(hits['x']) / processing_time if processing_time > 0 else 0,
                'success': True,
                'timestamp': time.time()
            }

            if self.verbose:
                print(f"   ✅ {len(hits['x']):,} hits in {processing_time:.2f}s "
                      f"({result['hits_per_sec']/1e6:.1f} M hits/sec)")

            return result

        except Exception as e:
            error_result = {
                'input_file': input_file,
                'error': str(e),
                'success': False,
                'timestamp': time.time()
            }

            if self.verbose:
                print(f"   ❌ Error: {e}")

            return error_result

    def _save_hits_hdf5(self, hits: Dict[str, np.ndarray], output_file: Path,
                       input_file: Path, processing_time: float) -> None:
        """Save hits to HDF5 format"""
        with h5py.File(output_file, 'w') as f:
            # Create hits group
            hits_group = f.create_group('hits')
            for field, data in hits.items():
                hits_group.create_dataset(field, data=data, compression='gzip')

            # Add metadata
            attrs = f.attrs
            attrs['input_file'] = str(input_file)
            attrs['processing_time_seconds'] = processing_time
            attrs['processed_timestamp'] = time.time()
            attrs['processor'] = 'TDCSophiread_AutoReducer'
            attrs['version'] = tdcsophiread.__version__
            attrs['parallel_processing'] = self.parallel

    def _save_tof_image(self, hits: Dict[str, np.ndarray], output_file: Path,
                       tof_range_ms: tuple = (0, 20), image_size: tuple = (512, 512)) -> None:
        """Save TOF image as TIFF"""
        if not TIFF_AVAILABLE or len(hits['x']) == 0:
            return

        try:
            # Convert TOF to milliseconds
            tof_ms = hits['tof'] * 25 / 1e6

            # Filter hits within TOF range
            mask = (tof_ms >= tof_range_ms[0]) & (tof_ms <= tof_range_ms[1])
            if not np.any(mask):
                if self.verbose:
                    print(f"   ⚠️ No hits in TOF range {tof_range_ms} ms")
                return

            x_filtered = hits['x'][mask]
            y_filtered = hits['y'][mask]

            # Create 2D histogram (TOF-integrated image)
            x_max = max(512, x_filtered.max() + 1) if len(x_filtered) > 0 else 512
            y_max = max(512, y_filtered.max() + 1) if len(y_filtered) > 0 else 512

            hist, _, _ = np.histogram2d(x_filtered, y_filtered,
                                     bins=[x_max, y_max],
                                     range=[[0, x_max], [0, y_max]])

            # Normalize to 16-bit range
            if hist.max() > 0:
                hist_norm = (hist / hist.max() * 65535).astype(np.uint16)
            else:
                hist_norm = hist.astype(np.uint16)

            # Resize to requested image size
            if hist_norm.shape != image_size:
                from PIL import Image
                img = Image.fromarray(hist_norm)
                img = img.resize(image_size, Image.Resampling.LANCZOS)
                hist_norm = np.array(img)

            # Save as TIFF
            Image.fromarray(hist_norm).save(output_file)

        except Exception as e:
            if self.verbose:
                print(f"   ⚠️ TIFF generation failed: {e}")

    def run_once(self) -> List[Dict[str, Any]]:
        """Run one processing cycle"""
        new_files = self.find_new_files()

        if not new_files:
            return []

        print(f"📁 Found {len(new_files)} new files to process")

        results = []
        for file_path in new_files:
            result = self.process_file(file_path)
            results.append(result)

            # Mark as processed (even if failed, to avoid reprocessing)
            self.processed_files.add(file_path)

            # Add to stats
            if result['success']:
                self.processing_stats.append(result)

        return results

    def run_continuous(self) -> None:
        """Run continuous monitoring and processing"""
        print(f"🔄 Starting continuous monitoring of {self.input_dir}")
        print(f"   Checking every {self.interval} seconds...")
        print(f"   Press Ctrl+C to stop")

        try:
            while True:
                results = self.run_once()

                if results:
                    successful = sum(1 for r in results if r['success'])
                    print(f"✅ Processed {successful}/{len(results)} files successfully")

                # Sleep until next check
                time.sleep(self.interval)

        except KeyboardInterrupt:
            print(f"\n🛑 Stopped by user")
            self.print_summary()

    def print_summary(self) -> None:
        """Print processing summary"""
        if not self.processing_stats:
            print("📊 No files processed successfully")
            return

        total_files = len(self.processing_stats)
        total_hits = sum(s['num_hits'] for s in self.processing_stats)
        total_time = sum(s['processing_time'] for s in self.processing_stats)
        avg_rate = total_hits / total_time if total_time > 0 else 0

        print(f"\n📊 Processing Summary")
        print(f"{'='*40}")
        print(f"Files processed: {total_files}")
        print(f"Total hits: {total_hits:,}")
        print(f"Total processing time: {total_time:.2f} seconds")
        print(f"Average rate: {avg_rate/1e6:.1f} M hits/sec")
        print(f"Files per hour: {3600 * total_files / total_time:.1f}")


def main():
    """Main CLI function"""
    parser = argparse.ArgumentParser(
        description="VENUS Auto-Reducer: Automated TPX3 data processing",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Process all files once
  %(prog)s -i /data/venus -o /data/processed

  # Continuous monitoring with custom interval
  %(prog)s -i /data/venus -o /data/processed --continuous --interval 60

  # Custom configuration with TIFF output
  %(prog)s -i /data/venus -o /data/processed -c config.json --tiff

  # Single-threaded processing
  %(prog)s -i /data/venus -o /data/processed --no-parallel
        """
    )

    # Required arguments
    parser.add_argument('-i', '--input-dir', required=True,
                       help='Input directory containing TPX3 files')
    parser.add_argument('-o', '--output-dir', required=True,
                       help='Output directory for processed data')

    # Configuration
    parser.add_argument('-c', '--config',
                       help='JSON configuration file (default: VENUS)')

    # Processing options
    parser.add_argument('--parallel', action='store_true', default=True,
                       help='Enable parallel processing (default)')
    parser.add_argument('--no-parallel', dest='parallel', action='store_false',
                       help='Disable parallel processing')

    # Operation modes
    parser.add_argument('--continuous', action='store_true',
                       help='Continuous monitoring mode')
    parser.add_argument('--interval', type=int, default=30,
                       help='Monitoring interval in seconds (default: 30)')

    # Output options
    parser.add_argument('--tiff', action='store_true',
                       help='Generate TIFF images (requires Pillow)')

    # Utility
    parser.add_argument('-v', '--verbose', action='store_true',
                       help='Verbose output')
    parser.add_argument('--version', action='version',
                       version=f'VENUS Auto-Reducer (TDCSophiread {tdcsophiread.__version__})')

    args = parser.parse_args()

    # Validate directories
    if not os.path.exists(args.input_dir):
        print(f"❌ Input directory not found: {args.input_dir}")
        sys.exit(1)

    # Check TIFF support
    if args.tiff and not TIFF_AVAILABLE:
        print("⚠️ TIFF output requested but Pillow not available")
        print("   Install with: pip install Pillow")
        args.tiff = False

    # Create auto-reducer
    reducer = VenusAutoReducer(
        input_dir=args.input_dir,
        output_dir=args.output_dir,
        config_path=args.config,
        parallel=args.parallel,
        interval=args.interval,
        verbose=args.verbose
    )

    # Run processing
    if args.continuous:
        reducer.run_continuous()
    else:
        results = reducer.run_once()
        reducer.print_summary()

        if results:
            successful = sum(1 for r in results if r['success'])
            print(f"🎉 Completed: {successful}/{len(results)} files processed successfully")
        else:
            print("📭 No new files found to process")


if __name__ == '__main__':
    main()