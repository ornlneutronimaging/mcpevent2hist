# TDCSophiread Quick Start Guide

Get up and running with TDCSophiread in minutes! Process TPX3 neutron imaging data with **96M+ hits/sec** performance.

## Installation

### Option 1: Development Installation (Recommended)

```bash
# Clone the repository
git clone https://github.com/ornlneutronimaging/mcpevent2hist.git
cd mcpevent2hist/sophiread

# Set up pixi environment (recommended)
pixi install

# Build and install
# One stop-shop
pixi run dev-install

# Alternatively, run each step manually
pixi run configure  # Configure the build
pixi run build        # Configure and build C++
pixi run pip install -e . --no-build-isolation  # Install Python bindings
pixi run python-test  # Verify installation
```

### Option 2: PyPI Installation (when available)

```bash
pip install tdcsophiread
```

## Getting Sample Data

The repository includes Jupyter notebooks with real TPX3 data examples. To access the 12GB sample dataset:

```bash
# Download the data submodule (12GB)
pixi run setup-data
```

**Note**: The data download is **~12GB** and may take several minutes depending on your connection.

## 1-Minute Tutorial: Hit Extraction

```python
import tdcsophiread

# Process a TPX3 file to extract hits
hits = tdcsophiread.process_tpx3("data.tpx3")

# Check the results (structured numpy array)
print(f"Extracted {len(hits):,} hits")
print(f"Data fields: {hits.dtype.names}")
print(f"X range: {hits['x'].min()} - {hits['x'].max()}")
print(f"Y range: {hits['y'].min()} - {hits['y'].max()}")
print(f"TOF range: {hits['tof'].min()} - {hits['tof'].max()} (25ns units)")
```

## 5-Minute Tutorial: Complete Neutron Processing

```python
import tdcsophiread
import numpy as np
import matplotlib.pyplot as plt

# 1. Extract hits with progress tracking
def progress_callback(progress, message):
    print(f"{progress:.1%} - {message}")

hits = tdcsophiread.process_tpx3("data.tpx3",
                                parallel=True,
                                progress_callback=progress_callback)

# 2. Process hits to neutrons using clustering
neutrons = tdcsophiread.process_hits_to_neutrons(hits)

print(f"Extracted {len(hits):,} hits")
print(f"Found {len(neutrons):,} neutrons")
print(f"Neutron efficiency: {len(neutrons)/len(hits):.3f}")

# 3. Convert to physical units
tof_ms = hits['tof'] * 25 / 1e6  # Convert to milliseconds
neutron_tof_ms = neutrons['tof'] * 25 / 1e6

# 4. Create visualizations
fig, axes = plt.subplots(2, 2, figsize=(12, 10))

# Hit position map
axes[0,0].hist2d(hits['x'], hits['y'], bins=128, cmap='viridis')
axes[0,0].set_title(f'Hit Map ({len(hits):,} hits)')
axes[0,0].set_xlabel('X')
axes[0,0].set_ylabel('Y')

# Neutron position map
axes[0,1].scatter(neutrons['x']/8, neutrons['y']/8, alpha=0.6, s=1)
axes[0,1].set_title(f'Neutron Map ({len(neutrons):,} neutrons)')
axes[0,1].set_xlabel('X (pixels)')
axes[0,1].set_ylabel('Y (pixels)')

# TOF spectrum (hits)
axes[1,0].hist(tof_ms, bins=100, alpha=0.7, range=(0, 20))
axes[1,0].set_title('Hit TOF Spectrum')
axes[1,0].set_xlabel('TOF (ms)')
axes[1,0].set_ylabel('Counts')

# TOF spectrum (neutrons)
axes[1,1].hist(neutron_tof_ms, bins=100, alpha=0.7, range=(0, 20))
axes[1,1].set_title('Neutron TOF Spectrum')
axes[1,1].set_xlabel('TOF (ms)')
axes[1,1].set_ylabel('Counts')

plt.tight_layout()
plt.show()
```

## Clustering Algorithms

TDCSophiread provides 4 high-performance clustering algorithms:

```python
# 1. ABS (Adaptive Bucket Sort) - Fastest O(n)
config = tdcsophiread.NeutronProcessingConfig.venus_defaults()
config.clustering.algorithm = "abs"
config.clustering.abs.radius = 5.0
neutrons = tdcsophiread.process_hits_to_neutrons(hits, config)

# 2. Graph Clustering - Good balance of speed and accuracy
config.clustering.algorithm = "graph"
config.clustering.graph.radius = 5.0
neutrons = tdcsophiread.process_hits_to_neutrons(hits, config)

# 3. DBSCAN - Best for handling noise
config.clustering.algorithm = "dbscan"
config.clustering.dbscan.epsilon = 4.0
config.clustering.dbscan.min_points = 3
neutrons = tdcsophiread.process_hits_to_neutrons(hits, config)

# 4. Grid Clustering - Leverages detector geometry
config.clustering.algorithm = "grid"
config.clustering.grid.connection_distance = 4.0
neutrons = tdcsophiread.process_hits_to_neutrons(hits, config)
```

## Performance Monitoring

```python
# Create processor for detailed monitoring
config = tdcsophiread.NeutronProcessingConfig.venus_defaults()
processor = tdcsophiread.TemporalNeutronProcessor(config)

# Process with timing
neutrons = processor.processHits(hits)

# Get performance statistics
stats = processor.getStatistics()
print(f"""
Performance Results:
  Processing time: {stats.total_processing_time_ms:.1f} ms
  Hit rate: {stats.hits_per_second/1e6:.1f} M hits/sec
  Neutron efficiency: {stats.neutron_efficiency:.3f}
  Parallel efficiency: {stats.parallel_efficiency:.2f}
  Workers used: {stats.num_workers_used}
  Memory per worker: {stats.memory_per_worker_mb:.1f} MB
""")
```

## Working with Large Files

### Memory-Efficient Streaming

```python
# For very large files (>10GB), use streaming
hits = tdcsophiread.process_tpx3_stream(
    "large_file.tpx3",
    chunk_size_mb=512,  # Process in 512MB chunks
    progress_callback=progress_callback
)

print(f"Streamed {len(hits):,} hits efficiently")
```

### Chunked Processing

```python
# Custom chunk processing for maximum control
config = tdcsophiread.DetectorConfig.venus_defaults()
processor = tdcsophiread.TDCProcessor(config)

# Process file in chunks
hits = processor.process_file("huge_file.tpx3",
                             chunk_size_mb=1024,  # 1GB chunks
                             parallel=True,       # Use all cores
                             num_threads=0)       # Auto-detect

print(f"Performance: {processor.get_last_hits_per_second()/1e6:.1f} M hits/sec")
```

## Custom Configuration

### High-Performance Setup

```python
config = tdcsophiread.NeutronProcessingConfig.venus_defaults()

# Optimize for maximum speed
config.clustering.algorithm = "abs"              # Fastest algorithm
config.temporal.num_workers = 0                  # Use all cores
config.temporal.max_batch_size = 200000          # Large batches
config.performance.enable_memory_pools = True    # Memory optimization
config.performance.enable_vectorization = True   # SIMD optimization

processor = tdcsophiread.TemporalNeutronProcessor(config)
neutrons = processor.processHits(hits)
```

### Memory-Constrained Setup

```python
config = tdcsophiread.NeutronProcessingConfig.venus_defaults()

# Optimize for low memory usage
config.temporal.max_batch_size = 50000           # Smaller batches
config.temporal.num_workers = 4                  # Fewer workers
config.performance.enable_memory_pools = False   # Reduce memory overhead

processor = tdcsophiread.TemporalNeutronProcessor(config)
neutrons = processor.processHits(hits)
```

### High-Accuracy Setup

```python
config = tdcsophiread.NeutronProcessingConfig.venus_defaults()

# Optimize for accuracy
config.clustering.algorithm = "dbscan"           # Best noise handling
config.clustering.dbscan.epsilon = 3.0           # Tight clustering
config.clustering.dbscan.min_points = 3          # Quality threshold
config.extraction.weighted_by_tot = True         # TOT weighting
config.extraction.min_tot_threshold = 10         # Filter low-quality hits

processor = tdcsophiread.TemporalNeutronProcessor(config)
neutrons = processor.processHits(hits)
```

## Data Format

### Hit Data (Structured NumPy Array)

```python
hits = tdcsophiread.process_tpx3("data.tpx3")
print(f"Fields: {hits.dtype.names}")
# ('tof', 'x', 'y', 'timestamp', 'tot', 'chip_id', 'cluster_id')

# Access specific fields
x_coords = hits['x']          # Global X coordinates (uint16)
y_coords = hits['y']          # Global Y coordinates (uint16)
tof_values = hits['tof']      # Time-of-flight (uint32, 25ns units)
tot_values = hits['tot']      # Time-over-threshold (uint16)
chip_ids = hits['chip_id']    # Chip ID 0-3 (uint8)
timestamps = hits['timestamp'] # Hit timestamps (uint32, 25ns units)
```

### Neutron Data (Structured NumPy Array)

```python
neutrons = tdcsophiread.process_hits_to_neutrons(hits)
print(f"Fields: {neutrons.dtype.names}")
# ('x', 'y', 'tof', 'tot', 'n_hits', 'chip_id', 'reserved')

# Access neutron properties
x_subpixel = neutrons['x']     # Sub-pixel X coordinates (float64)
y_subpixel = neutrons['y']     # Sub-pixel Y coordinates (float64)
tof_neutron = neutrons['tof']  # Representative TOF (uint32, 25ns units)
tot_combined = neutrons['tot'] # Combined TOT from all hits (uint16)
cluster_size = neutrons['n_hits'] # Number of hits in cluster (uint16)
```

### Unit Conversions

```python
# Convert time units
tof_ms = hits['tof'] * 25 / 1e6        # 25ns units → milliseconds
tof_us = hits['tof'] * 25 / 1e3        # 25ns units → microseconds
timestamp_s = hits['timestamp'] * 25 / 1e9  # 25ns units → seconds

# Convert coordinates
pixel_x = neutrons['x'] / 8.0          # Sub-pixel → pixel (assuming factor=8)
pixel_y = neutrons['y'] / 8.0          # Sub-pixel → pixel (assuming factor=8)
```

## Exception Handling

```python
try:
    hits = tdcsophiread.process_tpx3("data.tpx3")
    neutrons = tdcsophiread.process_hits_to_neutrons(hits)
except tdcsophiread.TDCFileError as e:
    print(f"File error: {e}")
except tdcsophiread.TDCConfigError as e:
    print(f"Configuration error: {e}")
except tdcsophiread.TDCProcessingError as e:
    print(f"Processing error: {e}")
```

## Real-World Examples

### Using the Sample Data

```python
# After setting up git submodules
sample_file = "notebooks/data/Run_8217_April25_2025_Ni_Powder_MCP_TPX3_0_8C_1_9_AngsMin_serval_000000.tpx3"

# Extract hits
hits = tdcsophiread.process_tpx3(sample_file, parallel=True)

# Process to neutrons with different algorithms
abs_neutrons = tdcsophiread.process_hits_to_neutrons(hits)  # Default ABS

config = tdcsophiread.NeutronProcessingConfig.venus_defaults()
config.clustering.algorithm = "dbscan"
dbscan_neutrons = tdcsophiread.process_hits_to_neutrons(hits, config)

print(f"ABS found {len(abs_neutrons):,} neutrons")
print(f"DBSCAN found {len(dbscan_neutrons):,} neutrons")
```

### Performance Comparison

```python
import time

algorithms = ["abs", "graph", "dbscan", "grid"]
results = {}

for algo in algorithms:
    config = tdcsophiread.NeutronProcessingConfig.venus_defaults()
    config.clustering.algorithm = algo

    processor = tdcsophiread.TemporalNeutronProcessor(config)

    start_time = time.time()
    neutrons = processor.processHits(hits)
    elapsed = time.time() - start_time

    results[algo] = {
        'neutrons': len(neutrons),
        'time_ms': elapsed * 1000,
        'rate_mhps': len(hits) / elapsed / 1e6
    }

# Print comparison
for algo, stats in results.items():
    print(f"{algo:6}: {stats['neutrons']:6,} neutrons, "
          f"{stats['time_ms']:6.1f} ms, {stats['rate_mhps']:5.1f} M hits/sec")
```

## Performance Expectations

| File Size | Expected Performance | Recommended Settings |
|-----------|---------------------|---------------------|
| < 100MB   | 20-40 M hits/sec   | Default settings |
| 100MB-1GB | 50-80 M hits/sec   | `parallel=True` |
| 1GB-10GB  | 80-96 M hits/sec   | `parallel=True`, `num_threads=0` |
| > 10GB    | 90-96 M hits/sec   | `process_tpx3_stream()` |

**Memory Usage**: ~40-60 bytes per hit (including clustering overhead)

## Next Steps

1. **📚 Explore the Notebooks**: Check out the Jupyter notebooks in `notebooks/` for real data analysis examples
2. **📖 API Reference**: Read the [complete API documentation](api_reference.md)
3. **🔬 Real Data**: Use `git submodule update --init notebooks/data` to get sample TPX3 files
4. **⚡ Performance**: Try different clustering algorithms to find the best fit for your data

## Jupyter Notebooks

The repository includes comprehensive examples:

```bash
# Start Jupyter in the pixi environment
pixi run jupyter lab

# Or use your preferred environment
jupyter lab
```

**Available Notebooks:**
- `notebooks/hits_extraction_from_tpx3_Ni.ipynb` - Hit extraction examples
- `notebooks/neutrons_extraction_from_tpx3_Ni.ipynb` - Neutron processing examples  
- `notebooks/clustering_abs_ni.ipynb` - ABS clustering demo
- `notebooks/clustering_graph_ni.ipynb` - Graph clustering demo
- `notebooks/clustering_dbscan_Ni.ipynb` - DBSCAN clustering demo
- `notebooks/clustering_grid_Ni.ipynb` - Grid clustering demo

## Getting Help

- **📖 Documentation**: [API Reference](api_reference.md)
- **💻 Examples**: Jupyter notebooks in `notebooks/`
- **🐛 Issues**: [GitHub Issues](https://github.com/ornlneutronimaging/mcpevent2hist/issues)
- **💬 Discussions**: [GitHub Discussions](https://github.com/ornlneutronimaging/mcpevent2hist/discussions)

Ready to process neutron data at **96M+ hits/sec**? 🚀