# TDCSophiread Quick Start Guide

Get up and running with TDCSophiread in minutes!

## Installation

```bash
# Install from PyPI (when available)
pip install tdcsophiread

# Or install from source
git clone https://github.com/ornlneutronimaging/mcpevent2hist.git
cd mcpevent2hist/sophiread
pip install -e .
```

## 1-Minute Tutorial

```python
import tdcsophiread

# Process a TPX3 file (one line!)
hits = tdcsophiread.process_tpx3("your_data.tpx3")

# Check the results
print(f"Processed {len(hits['x']):,} hits")
print(f"X range: {hits['x'].min()} - {hits['x'].max()}")
print(f"Y range: {hits['y'].min()} - {hits['y'].max()}")
```

## 5-Minute Tutorial

```python
import tdcsophiread
import matplotlib.pyplot as plt

# 1. Process with progress tracking
def progress(p, msg):
    print(f"{p:.1%} - {msg}")

hits = tdcsophiread.process_tpx3("data.tpx3", progress_callback=progress)

# 2. Get comprehensive statistics
stats = tdcsophiread.analysis.calculate_hit_statistics(hits)
print(f"Total hits: {stats['total_hits']:,}")
print(f"Active chips: {stats['chip_breakdown']['active_chips']}")

# 3. Create TOF spectrum
bin_centers, counts = tdcsophiread.analysis.create_tof_spectrum(hits)

# 4. Plot results
plt.figure(figsize=(10, 4))

plt.subplot(1, 2, 1)
plt.plot(bin_centers, counts)
plt.xlabel('TOF (ms)')
plt.ylabel('Counts')
plt.title('TOF Spectrum')

plt.subplot(1, 2, 2)
plt.hist2d(hits['x'], hits['y'], bins=128, cmap='viridis')
plt.xlabel('X')
plt.ylabel('Y')
plt.title('Hit Map')
plt.colorbar()

plt.tight_layout()
plt.show()
```

## Common Use Cases

### Large File Processing

```python
# For files > 1GB, use streaming
chunks = tdcsophiread.process_tpx3_stream(
    "large_file.tpx3",
    chunk_size_mb=512,
    progress_callback=progress
)

# Process chunks individually or combine
all_hits = tdcsophiread.analysis.combine_hit_chunks(chunks)
```

### Region of Interest Analysis

```python
# Select spatial region
roi_hits = tdcsophiread.analysis.select_roi(
    hits,
    x_range=(100, 400),
    y_range=(100, 400)
)

# Filter by time-of-flight
tof_filtered = tdcsophiread.analysis.filter_hits_by_tof(
    hits,
    tof_range_ms=(5.0, 15.0)
)
```

### Custom Configuration

```python
# Create custom detector config
config_dict = {
    "timing": {"tdc_frequency": 60.0},
    "detector_layout": {"gap_x": 5, "gap_y": 5}
}
config = tdcsophiread.DetectorConfig.from_json(config_dict)

# Use with processor
processor = tdcsophiread.TDCProcessor(config)
hits = processor.process_file_parallel("data.tpx3")
```

## Performance Tips

1. **Use parallel processing** for files > 100MB:
   ```python
   hits = tdcsophiread.process_tpx3("file.tpx3", parallel=True)
   ```

2. **Stream large files** to save memory:
   ```python
   chunks = tdcsophiread.process_tpx3_stream("huge.tpx3", chunk_size_mb=512)
   ```

3. **Filter early** to reduce memory usage:
   ```python
   for chunk in chunks:
       filtered = tdcsophiread.analysis.filter_hits_by_tof(chunk, (5, 15))
       # Process filtered chunk
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
    'timestamp': np.array([...], dtype=np.uint32) # Hit timestamp (25ns)
}
```

Convert TOF to physical units: `tof_ms = hits['tof'] * 25 / 1e6`

## Error Handling

```python
try:
    hits = tdcsophiread.process_tpx3("data.tpx3")
except tdcsophiread.TDCFileError as e:
    print(f"File error: {e}")
except tdcsophiread.TDCProcessingError as e:
    print(f"Processing error: {e}")
```

## Next Steps

- 📖 Read the [full API reference](api_reference.md)
- 💻 Try the [Jupyter tutorial](../examples/tdcsophiread_tutorial.ipynb)
- 🚀 Run the [usage examples](../examples/basic_usage.py)

## Performance Expectations

- **Small files** (< 100MB): ~10-20 M hits/sec
- **Large files** (> 1GB): ~30-50 M hits/sec with parallel processing
- **Memory usage**: ~40 bytes per hit (for numpy arrays)
- **Speedup**: 100-150x faster than pure Python implementations

## Getting Help

- Check the [API reference](api_reference.md) for detailed documentation
- Look at [examples](../examples/) for common usage patterns
- Open an issue on GitHub for bugs or feature requests

Happy analyzing! 🎉