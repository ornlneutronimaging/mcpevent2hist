# TDCSophiread API Reference

## Overview

TDCSophiread is a high-performance Python package for processing TPX3 neutron imaging data with TDC timing. It provides a clean Python interface to a C++ core that achieves 120x speedup over pure Python implementations.

## Quick Start

```python
import tdcsophiread

# Simple processing
hits = tdcsophiread.process_tpx3("data.tpx3")
print(f"Processed {len(hits['x']):,} hits")

# With progress tracking
def progress(p, msg):
    print(f"{p:.1%} - {msg}")

hits = tdcsophiread.process_tpx3("data.tpx3", progress_callback=progress)
```

## Core Classes

### DetectorConfig

Manages detector configuration parameters including chip layout, timing settings, and coordinate transformations.

```python
# Use VENUS detector defaults
config = tdcsophiread.DetectorConfig.venus_defaults()

# Load from JSON file
config = tdcsophiread.DetectorConfig.from_file("config.json")

# Load from dictionary
config_dict = {
    "detector": {
        "chip_layout": {"chip_size_x": 512, "chip_size_y": 512},
        "timing": {"tdc_frequency_hz": 60.0}
    }
}
config = tdcsophiread.DetectorConfig.from_json(config_dict)

# Access configuration
freq = config.get_tdc_frequency()  # 60.0 Hz
chip_x = config.get_chip_size_x()  # 512 pixels

# Coordinate mapping
global_x, global_y = config.map_chip_to_global(chip_id=1, local_x=100, local_y=100)
```

### TDCProcessor

Main processing class for TPX3 files.

```python
config = tdcsophiread.DetectorConfig.venus_defaults()
processor = tdcsophiread.TDCProcessor(config)

# Single-threaded processing
hits = processor.process_file("data.tpx3")

# Parallel processing with TBB
hits = processor.process_file_parallel("data.tpx3", num_threads=12)

# Chunk processing for large files
hits, bytes_processed = processor.process_chunk("data.tpx3",
                                                start_offset=0,
                                                requested_size=1024*1024*1024)

# Configuration
processor.set_missing_tdc_correction_enabled(True)

# Performance metrics
print(f"Processing time: {processor.get_last_processing_time_ms():.1f} ms")
print(f"Hit count: {processor.get_last_hit_count():,}")
print(f"Rate: {processor.get_last_hits_per_second()/1e6:.1f} M hits/sec")
```

### TDCStreamProcessor

Memory-efficient processor for large files with progress tracking.

```python
config = tdcsophiread.DetectorConfig.venus_defaults()

# Using context manager
with tdcsophiread.TDCStreamProcessor(config) as processor:
    chunks = processor.process_file_stream("large_file.tpx3",
                                         chunk_size_mb=512,
                                         progress_callback=progress)

    # Combine chunks
    all_hits = tdcsophiread.analysis.combine_hit_chunks(chunks)
```

### TDCHit

Individual hit data structure.

```python
# Access hit fields
hit = tdcsophiread.TDCHit()
hit.x = 256          # Global X coordinate
hit.y = 256          # Global Y coordinate  
hit.tof = 400000     # Time-of-flight (25ns units)
hit.tot = 100        # Time-over-threshold
hit.chip_id = 0      # Chip identifier (0-3)
hit.timestamp = 1000 # Hit timestamp (25ns units)
```

## Convenience Functions

### process_tpx3()

High-level function for simple TPX3 file processing.

```python
# Basic usage
hits = tdcsophiread.process_tpx3("data.tpx3")

# All parameters
hits = tdcsophiread.process_tpx3(
    file_path="data.tpx3",
    parallel=True,              # Use parallel processing
    num_threads=0,              # 0 = auto-detect
    progress_callback=progress  # Progress tracking function
)

# Returns dictionary of numpy arrays
print(hits.keys())  # ['x', 'y', 'tof', 'tot', 'chip_id', 'timestamp']
```

### process_tpx3_stream()

Stream processing for large files.

```python
chunks = tdcsophiread.process_tpx3_stream(
    file_path="large_file.tpx3",
    chunk_size_mb=512,
    progress_callback=progress
)

# Process chunks individually or combine
for chunk in chunks:
    print(f"Chunk has {len(chunk['x'])} hits")
```

### hits_to_numpy()

Convert C++ hit vector to numpy arrays.

```python
processor = tdcsophiread.TDCProcessor(config)
hit_vector = processor.process_file("data.tpx3")
hits = tdcsophiread.hits_to_numpy(hit_vector)
```

## Analysis Module

The `analysis` module provides data analysis utilities.

### create_tof_spectrum()

Generate time-of-flight spectrum.

```python
bin_centers, counts = tdcsophiread.analysis.create_tof_spectrum(
    hits,
    tof_range_ms=(0, 20),   # TOF range in milliseconds
    num_bins=1000,          # Number of histogram bins
    chip_filter=[0, 1]      # Optional: only include specific chips
)
```

### select_roi()

Select hits within a region of interest.

```python
roi_hits = tdcsophiread.analysis.select_roi(
    hits,
    x_range=(100, 400),  # X coordinate range
    y_range=(100, 400)   # Y coordinate range
)
```

### filter_hits_by_tof()

Filter hits by time-of-flight range.

```python
filtered = tdcsophiread.analysis.filter_hits_by_tof(
    hits,
    tof_range_ms=(5.0, 15.0)  # TOF range in milliseconds
)
```

### calculate_hit_statistics()

Calculate comprehensive statistics.

```python
stats = tdcsophiread.analysis.calculate_hit_statistics(hits)

print(f"Total hits: {stats['total_hits']:,}")
print(f"X range: {stats['coordinate_ranges']['x_range']}")
print(f"TOF mean: {stats['timing_stats']['tof_mean_ms']:.2f} ms")
print(f"Active chips: {stats['chip_breakdown']['active_chips']}")
```

### Plotting Functions

If matplotlib is available:

```python
# TOF spectrum plot
fig = tdcsophiread.analysis.plot_tof_spectrum(
    hits,
    tof_range_ms=(0, 20),
    num_bins=1000,
    title="TOF Spectrum",
    show_stats=True
)

# 2D hit position map
fig = tdcsophiread.analysis.plot_hit_map(
    hits,
    bins=256,
    title="Hit Position Map",
    cmap='viridis'
)
```

## Exception Handling

TDCSophiread defines custom exceptions for better error handling:

```python
try:
    hits = tdcsophiread.process_tpx3("data.tpx3")
except tdcsophiread.TDCFileError as e:
    print(f"File error: {e}")
except tdcsophiread.TDCConfigError as e:
    print(f"Configuration error: {e}")
except tdcsophiread.TDCProcessingError as e:
    print(f"Processing error: {e}")
```

## Performance Tips

1. **Use parallel processing** for files > 100MB:
   ```python
   hits = tdcsophiread.process_tpx3("large.tpx3", parallel=True)
   ```

2. **Stream large files** to manage memory:
   ```python
   chunks = tdcsophiread.process_tpx3_stream("huge.tpx3", chunk_size_mb=512)
   ```

3. **Pre-filter data** to reduce memory usage:
   ```python
   for chunk in chunks:
       filtered = tdcsophiread.analysis.filter_hits_by_tof(chunk, (5, 15))
       # Process filtered chunk
   ```

4. **Use context managers** for resource cleanup:
   ```python
   with tdcsophiread.TDCStreamProcessor(config) as proc:
       # Processing happens here
       pass
   ```

## Data Format

Hit data is returned as a dictionary of numpy arrays:

```python
hits = {
    'x': np.array([...], dtype=np.uint16),      # Global X coordinates
    'y': np.array([...], dtype=np.uint16),      # Global Y coordinates
    'tof': np.array([...], dtype=np.uint32),    # Time-of-flight (25ns units)
    'tot': np.array([...], dtype=np.uint16),    # Time-over-threshold
    'chip_id': np.array([...], dtype=np.uint8), # Chip ID (0-3)
    'timestamp': np.array([...], dtype=np.uint32) # Hit timestamp (25ns units)
}
```

Convert TOF to milliseconds: `tof_ms = hits['tof'] * 25 / 1e6`

## Configuration Files

Example JSON configuration:

```json
{
  "detector": {
    "chip_layout": {
      "chip_size_x": 512,
      "chip_size_y": 512
    },
    "timing": {
      "tdc_frequency_hz": 60.0,
      "enable_missing_tdc_correction": true
    },
    "super_resolution": {
      "factor": 8
    }
  }
}
```