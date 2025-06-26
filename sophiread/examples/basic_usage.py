#!/usr/bin/env python3
"""
TDCSophiread Basic Usage Examples

This script demonstrates the most common use cases for TDCSophiread
"""

import tdcsophiread
import numpy as np
import time

def example_1_simple_processing():
    """Example 1: Simple file processing"""
    print("=" * 60)
    print("Example 1: Simple Processing")
    print("=" * 60)

    # Process a TPX3 file with default settings
    data_file = "resources/data/tiny.tpx3"
    hits = tdcsophiread.process_tpx3(data_file)

    print(f"✅ Processed {len(hits['x']):,} hits")
    print(f"   Data fields: {list(hits.keys())}")
    print(f"   X range: {hits['x'].min()} - {hits['x'].max()}")
    print(f"   Y range: {hits['y'].min()} - {hits['y'].max()}")
    print(f"   TOF range: {hits['tof'].min()} - {hits['tof'].max()} (25ns units)")

    return hits


def example_2_progress_tracking():
    """Example 2: Processing with progress tracking"""
    print("\n" + "=" * 60)
    print("Example 2: Progress Tracking")
    print("=" * 60)

    def progress_callback(progress, message):
        # Simple progress bar
        bar_length = 30
        filled = int(bar_length * progress)
        bar = '█' * filled + '░' * (bar_length - filled)
        print(f"\r[{bar}] {progress:.1%} - {message}", end='', flush=True)

    data_file = "resources/data/tiny.tpx3"
    start_time = time.time()
    hits = tdcsophiread.process_tpx3(data_file, progress_callback=progress_callback)
    process_time = time.time() - start_time

    print(f"\n✅ Processed {len(hits['x']):,} hits in {process_time:.3f} seconds")
    print(f"   Rate: {len(hits['x']) / process_time / 1e6:.1f} M hits/sec")

    return hits


def example_3_data_analysis():
    """Example 3: Data analysis and statistics"""
    print("\n" + "=" * 60)
    print("Example 3: Data Analysis")
    print("=" * 60)

    data_file = "resources/data/tiny.tpx3"
    hits = tdcsophiread.process_tpx3(data_file)

    # Calculate comprehensive statistics
    stats = tdcsophiread.analysis.calculate_hit_statistics(hits)

    print("📊 Hit Statistics:")
    print(f"   Total hits: {stats['total_hits']:,}")
    print(f"   Coordinate ranges:")
    print(f"     X: {stats['coordinate_ranges']['x_range']}")
    print(f"     Y: {stats['coordinate_ranges']['y_range']}")
    print(f"   Timing:")
    print(f"     TOF: {stats['timing_stats']['tof_range_ms'][0]:.2f} - {stats['timing_stats']['tof_range_ms'][1]:.2f} ms")
    print(f"     TOF mean: {stats['timing_stats']['tof_mean_ms']:.2f} ms")
    print(f"   Active chips: {stats['chip_breakdown']['active_chips']}")

    # Create TOF spectrum
    bin_centers, counts = tdcsophiread.analysis.create_tof_spectrum(
        hits, tof_range_ms=(0, 20), num_bins=100
    )
    peak_tof = bin_centers[np.argmax(counts)]
    peak_count = np.max(counts)

    print(f"\n📈 TOF Spectrum:")
    print(f"   Bins: {len(bin_centers)}")
    print(f"   Peak: {peak_tof:.2f} ms ({peak_count} counts)")
    print(f"   Total counts: {np.sum(counts):,}")

    return hits, stats


def example_4_roi_selection():
    """Example 4: Region of Interest selection"""
    print("\n" + "=" * 60)
    print("Example 4: ROI Selection")
    print("=" * 60)

    data_file = "resources/data/tiny.tpx3"
    hits = tdcsophiread.process_tpx3(data_file)

    # Select center region
    roi_hits = tdcsophiread.analysis.select_roi(
        hits,
        x_range=(200, 300),
        y_range=(200, 300)
    )

    print(f"🎯 ROI Selection (200-300, 200-300):")
    print(f"   Original hits: {len(hits['x']):,}")
    print(f"   ROI hits: {len(roi_hits['x']):,}")
    print(f"   Fraction: {100 * len(roi_hits['x']) / len(hits['x']):.1f}%")

    # Time-based filtering
    tof_filtered = tdcsophiread.analysis.filter_hits_by_tof(
        hits, tof_range_ms=(5.0, 15.0)
    )

    print(f"\n⏱️ TOF Filtering (5-15 ms):")
    print(f"   Original hits: {len(hits['x']):,}")
    print(f"   Filtered hits: {len(tof_filtered['x']):,}")
    print(f"   Fraction: {100 * len(tof_filtered['x']) / len(hits['x']):.1f}%")

    return roi_hits, tof_filtered


def example_5_streaming():
    """Example 5: Streaming large files"""
    print("\n" + "=" * 60)
    print("Example 5: Streaming Processing")
    print("=" * 60)

    def streaming_progress(progress, message):
        print(f"  📦 {progress:.1%} - {message}")

    data_file = "resources/data/tiny.tpx3"

    # Process in small chunks for demonstration
    chunks = tdcsophiread.process_tpx3_stream(
        data_file,
        chunk_size_mb=1,  # Small chunks for demo
        progress_callback=streaming_progress
    )

    print(f"\n✅ Received {len(chunks)} chunks")

    total_hits = 0
    for i, chunk in enumerate(chunks):
        chunk_hits = len(chunk['x'])
        total_hits += chunk_hits
        print(f"   Chunk {i}: {chunk_hits:,} hits")

    # Combine all chunks
    all_hits = tdcsophiread.analysis.combine_hit_chunks(chunks)
    print(f"\n🔗 Combined: {len(all_hits['x']):,} hits")

    return all_hits


def example_6_custom_configuration():
    """Example 6: Custom detector configuration"""
    print("\n" + "=" * 60)
    print("Example 6: Custom Configuration")
    print("=" * 60)

    # Use VENUS defaults as base and show configuration access
    config = tdcsophiread.DetectorConfig.venus_defaults()

    print("🔧 VENUS Configuration:")
    print(f"   TDC frequency: {config.get_tdc_frequency()} Hz")
    print(f"   Chip size: {config.get_chip_size_x()} x {config.get_chip_size_y()}")
    print(f"   Super resolution: {config.get_super_resolution_factor()}")

    # Use custom config with processor
    processor = tdcsophiread.TDCProcessor(config)
    data_file = "resources/data/tiny.tpx3"
    hit_vector = processor.process_file_parallel(data_file, num_threads=4)
    hits = tdcsophiread.hits_to_numpy(hit_vector)

    print(f"\n✅ Processed with VENUS config: {len(hits['x']):,} hits")
    print(f"   Processing rate: {processor.get_last_hits_per_second()/1e6:.1f} M hits/sec")

    return hits, config


def example_7_performance_comparison():
    """Example 7: Performance comparison"""
    print("\n" + "=" * 60)
    print("Example 7: Performance Comparison")
    print("=" * 60)

    data_file = "resources/data/tiny.tpx3"

    # Single-threaded
    start = time.time()
    hits_single = tdcsophiread.process_tpx3(data_file, parallel=False)
    time_single = time.time() - start

    # Multi-threaded
    start = time.time()
    hits_multi = tdcsophiread.process_tpx3(data_file, parallel=True)
    time_multi = time.time() - start

    # Results
    print("⚡ Performance Comparison:")
    print(f"   Single-threaded: {time_single*1000:.1f} ms")
    print(f"   Multi-threaded:  {time_multi*1000:.1f} ms")
    print(f"   Speedup: {time_single/time_multi:.1f}x")

    rate_single = len(hits_single['x']) / time_single / 1e6
    rate_multi = len(hits_multi['x']) / time_multi / 1e6

    print(f"\n📈 Processing Rates:")
    print(f"   Single-threaded: {rate_single:.1f} M hits/sec")
    print(f"   Multi-threaded:  {rate_multi:.1f} M hits/sec")

    return hits_multi


def main():
    """Run all examples"""
    print("TDCSophiread Usage Examples")
    print(f"Version: {tdcsophiread.__version__}")

    try:
        # Run examples
        hits1 = example_1_simple_processing()
        hits2 = example_2_progress_tracking()
        hits3, stats = example_3_data_analysis()
        roi_hits, tof_hits = example_4_roi_selection()
        stream_hits = example_5_streaming()
        custom_hits, config = example_6_custom_configuration()
        final_hits = example_7_performance_comparison()

        print("\n" + "=" * 60)
        print("🎉 All examples completed successfully!")
        print("=" * 60)

        # Final summary
        print(f"\nFinal dataset: {len(final_hits['x']):,} hits")
        print(f"Memory usage: ~{final_hits['x'].nbytes + final_hits['y'].nbytes + final_hits['tof'].nbytes:.0f} bytes")

    except Exception as e:
        print(f"\n❌ Error running examples: {e}")
        return 1

    return 0


if __name__ == "__main__":
    exit(main())