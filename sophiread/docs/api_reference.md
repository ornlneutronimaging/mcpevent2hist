# TDCSophiread API Reference

## Overview

TDCSophiread is a high-performance Python package for processing TPX3 neutron imaging data using TDC-only timing. It achieves **96M+ hits/sec** throughput with parallel processing and provides both hit extraction and neutron clustering capabilities.

**Key Features:**
- TDC-only processing (detector-expert approved)
- 4 clustering algorithms: ABS, Graph, DBSCAN, Grid
- Parallel processing with Intel TBB
- Zero-copy temporal batching
- 96M+ hits/sec performance

## Quick Start

```python
import tdcsophiread

# 1. Extract hits from TPX3 file
hits = tdcsophiread.process_tpx3("data.tpx3")
print(f"Extracted {len(hits):,} hits")

# 2. Process hits to neutrons with clustering
neutrons = tdcsophiread.process_hits_to_neutrons(hits)
print(f"Found {len(neutrons):,} neutrons")
```

## Core Classes

### DetectorConfig

Manages detector configuration for TPX3 chip layout and TDC timing.

```python
# Use VENUS detector defaults
config = tdcsophiread.DetectorConfig.venus_defaults()

# Load from JSON file
config = tdcsophiread.DetectorConfig.from_file("config.json")

# Load from dictionary
config_dict = {
    "timing": {"tdc_frequency_hz": 60.0},
    "detector": {"chip_size_x": 256, "chip_size_y": 256}
}
config = tdcsophiread.DetectorConfig.from_json(config_dict)

# Access configuration
freq = config.get_tdc_frequency()  # 60.0 Hz
size_x = config.get_chip_size_x()  # 256 pixels

# Coordinate transformations
global_x, global_y = config.map_chip_to_global(chip_id=1, local_x=100, local_y=200)
```

### TDCProcessor

Main processor for extracting hits from TPX3 files.

```python
config = tdcsophiread.DetectorConfig.venus_defaults()
processor = tdcsophiread.TDCProcessor(config)

# Process file with chunk-based memory mapping
hits = processor.process_file("data.tpx3",
                             chunk_size_mb=512,  # Memory-efficient processing
                             parallel=True,      # Use parallel processing
                             num_threads=0)      # Auto-detect cores

# Performance metrics
print(f"Processing time: {processor.get_last_processing_time_ms():.1f} ms")
print(f"Hit count: {processor.get_last_hit_count():,}")
print(f"Rate: {processor.get_last_hits_per_second()/1e6:.1f} M hits/sec")
```

### NeutronProcessingConfig

Configuration for hit clustering and neutron extraction.

```python
# Create VENUS defaults
config = tdcsophiread.NeutronProcessingConfig.venus_defaults()

# Configure clustering algorithm
config.clustering.algorithm = "abs"  # or "graph", "dbscan", "grid"
config.clustering.abs.radius = 5.0
config.clustering.abs.neutron_correlation_window = 75.0  # nanoseconds

# Configure neutron extraction
config.extraction.algorithm = "simple_centroid"
config.extraction.super_resolution_factor = 8.0
config.extraction.weighted_by_tot = True

# Configure parallel processing
config.temporal.num_workers = 0        # Auto-detect
config.temporal.max_batch_size = 100000
```

### TemporalNeutronProcessor

High-performance neutron processing with parallel temporal batching.

```python
# Create processor with configuration
config = tdcsophiread.NeutronProcessingConfig.venus_defaults()
processor = tdcsophiread.TemporalNeutronProcessor(config)

# Process hits to neutrons (zero-copy)
neutrons = processor.processHits(hits)

# Performance metrics
print(f"Processing time: {processor.getLastProcessingTimeMs():.1f} ms")
print(f"Rate: {processor.getLastHitsPerSecond()/1e6:.1f} M hits/sec")
print(f"Efficiency: {processor.getLastNeutronEfficiency():.3f}")

# Get detailed statistics
stats = processor.getStatistics()
print(f"Total hits: {stats.total_hits_processed:,}")
print(f"Total neutrons: {stats.total_neutrons_produced:,}")
print(f"Parallel efficiency: {stats.parallel_efficiency:.2f}")
```

## Data Structures

### TDCHit

Individual hit data structure.

```python
# Hit fields (structured numpy array)
hit_array = hits  # From process_tpx3()
print(f"Fields: {hit_array.dtype.names}")
# ('tof', 'x', 'y', 'timestamp', 'tot', 'chip_id', 'cluster_id')

# Access hit data
x_coords = hit_array['x']          # Global X coordinates (uint16)
y_coords = hit_array['y']          # Global Y coordinates (uint16)  
tof_values = hit_array['tof']      # Time-of-flight (uint32, 25ns units)
tot_values = hit_array['tot']      # Time-over-threshold (uint16)
chip_ids = hit_array['chip_id']    # Chip ID 0-3 (uint8)
timestamps = hit_array['timestamp'] # Hit timestamps (uint32, 25ns units)
```

### TDCNeutron

Neutron event data structure.

```python
# Neutron fields (structured numpy array)
neutron_array = neutrons  # From process_hits_to_neutrons()
print(f"Fields: {neutron_array.dtype.names}")
# ('x', 'y', 'tof', 'tot', 'n_hits', 'chip_id', 'reserved')

# Access neutron data
x_coords = neutron_array['x']       # Sub-pixel X coordinates (float64)
y_coords = neutron_array['y']       # Sub-pixel Y coordinates (float64)
tof_values = neutron_array['tof']   # Time-of-flight (uint32, 25ns units)
tot_values = neutron_array['tot']   # Combined TOT (uint16)
n_hits = neutron_array['n_hits']    # Number of hits in cluster (uint16)
chip_ids = neutron_array['chip_id'] # Chip ID (uint8)
```

## High-Level Functions

### process_tpx3()

Extract hits from TPX3 files.

```python
# Basic usage
hits = tdcsophiread.process_tpx3("data.tpx3")

# With progress tracking
def progress_callback(progress, message):
    print(f"{progress:.1%} - {message}")

hits = tdcsophiread.process_tpx3(
    file_path="data.tpx3",
    parallel=True,              # Use parallel processing
    num_threads=0,              # 0 = auto-detect cores
    progress_callback=progress_callback
)

# Returns structured numpy array
print(f"Extracted {len(hits):,} hits")
print(f"Data type: {hits.dtype}")
```

### process_hits_to_neutrons()

Process hits to neutrons using clustering and extraction.

```python
# Basic usage with default configuration
neutrons = tdcsophiread.process_hits_to_neutrons(hits)

# With custom configuration
config = tdcsophiread.NeutronProcessingConfig.venus_defaults()
config.clustering.algorithm = "dbscan"
config.clustering.dbscan.epsilon = 4.0
config.clustering.dbscan.min_points = 3

neutrons = tdcsophiread.process_hits_to_neutrons(hits, config)

print(f"Found {len(neutrons):,} neutrons")
```

### process_tpx3_stream()

Memory-efficient streaming for large files.

```python
hits = tdcsophiread.process_tpx3_stream(
    file_path="large_file.tpx3",
    chunk_size_mb=512,           # Process in 512MB chunks
    progress_callback=progress_callback
)

print(f"Streamed {len(hits):,} hits")
```

## Clustering Algorithms

### ABS (Adaptive Bucket Sort)

Fast O(n) clustering with temporal buckets.

```python
config = tdcsophiread.NeutronProcessingConfig.venus_defaults()
config.clustering.algorithm = "abs"
config.clustering.abs.radius = 5.0                      # Spatial radius (pixels)
config.clustering.abs.neutron_correlation_window = 75.0 # Temporal window (ns)
config.clustering.abs.min_cluster_size = 1              # Minimum hits per cluster

processor = tdcsophiread.TemporalNeutronProcessor(config)
neutrons = processor.processHits(hits)
```

### Graph Clustering

Connected components with spatial hashing.

```python
config.clustering.algorithm = "graph"
config.clustering.graph.radius = 5.0              # Connection radius (pixels)
config.clustering.graph.min_cluster_size = 1      # Minimum cluster size
config.clustering.graph.enable_spatial_hash = True # Use spatial optimization
config.clustering.graph.parallel_threshold = 100000 # Parallel processing threshold
```

### DBSCAN

Density-based clustering for complex patterns.

```python
config.clustering.algorithm = "dbscan"
config.clustering.dbscan.epsilon = 5.0            # Neighborhood radius (pixels)
config.clustering.dbscan.min_points = 4           # Minimum points for core
config.clustering.dbscan.neutron_correlation_window = 75.0 # Temporal window (ns)
```

### Grid Clustering

O(n) clustering using detector grid structure.

```python
config.clustering.algorithm = "grid"
config.clustering.grid.grid_cols = 32             # Grid columns
config.clustering.grid.grid_rows = 32             # Grid rows
config.clustering.grid.connection_distance = 4.0   # Max connection distance
config.clustering.grid.merge_adjacent_cells = True # Merge across boundaries
```

## Performance Monitoring

### Processing Statistics

```python
processor = tdcsophiread.TemporalNeutronProcessor(config)
neutrons = processor.processHits(hits)

# Get detailed statistics
stats = processor.getStatistics()
print(f"""
Processing Statistics:
  Total hits: {stats.total_hits_processed:,}
  Total neutrons: {stats.total_neutrons_produced:,}
  Processing time: {stats.total_processing_time_ms:.1f} ms
  Hit rate: {stats.hits_per_second/1e6:.1f} M hits/sec
  Neutron efficiency: {stats.neutron_efficiency:.3f}
  Parallel efficiency: {stats.parallel_efficiency:.2f}
  Workers used: {stats.num_workers_used}
  Batches created: {stats.num_batches_created}
""")
```

### Algorithm Information

```python
print(f"Clustering algorithm: {processor.getHitClusteringAlgorithm()}")
print(f"Extraction algorithm: {processor.getNeutronExtractionAlgorithm()}")
print(f"Number of workers: {processor.getNumWorkers()}")
```

## Exception Handling

TDCSophiread defines custom exceptions:

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

## Performance Tips

### 1. Optimize for Your Data Size

```python
# Small files (<100MB): Default settings
hits = tdcsophiread.process_tpx3("small.tpx3")

# Large files (>1GB): Parallel processing
hits = tdcsophiread.process_tpx3("large.tpx3", parallel=True, num_threads=0)

# Very large files (>10GB): Streaming
hits = tdcsophiread.process_tpx3_stream("huge.tpx3", chunk_size_mb=512)
```

### 2. Choose the Right Clustering Algorithm

```python
# For speed: ABS (fastest, O(n))
config.clustering.algorithm = "abs"

# For accuracy: DBSCAN (handles noise well)
config.clustering.algorithm = "dbscan"

# For detector geometry: Grid (leverages natural structure)
config.clustering.algorithm = "grid"

# For general use: Graph (good balance)
config.clustering.algorithm = "graph"
```

### 3. Optimize Batch Sizes

```python
config.temporal.min_batch_size = 1000    # Smaller for low-latency
config.temporal.max_batch_size = 50000   # Smaller for memory-constrained systems
config.temporal.max_batch_size = 200000  # Larger for high-performance systems
```

### 4. Monitor Performance

```python
import time

start_time = time.time()
neutrons = processor.processHits(hits)
python_overhead = (time.time() - start_time) * 1000 - processor.getLastProcessingTimeMs()

print(f"C++ processing: {processor.getLastProcessingTimeMs():.1f} ms")
print(f"Python overhead: {python_overhead:.1f} ms")
```

## Data Format Details

### Time Conversions

```python
# Convert TOF to milliseconds
tof_ms = hits['tof'] * 25 / 1e6  # 25ns units to milliseconds

# Convert timestamps to seconds
timestamps_s = hits['timestamp'] * 25 / 1e9  # 25ns units to seconds
```

### Coordinate Systems

```python
# Chip coordinates: 0-255 (local to each chip)
# Global coordinates: Mapped to detector space
# Sub-pixel coordinates: Scaled by super_resolution_factor (default 8.0)

# Example: Convert neutron coordinates back to pixels
pixel_x = neutrons['x'] / 8.0  # Assuming super_resolution_factor = 8.0
pixel_y = neutrons['y'] / 8.0
```

### Zero-Copy Access

```python
# TDCSophiread uses zero-copy numpy arrays for efficiency
hits_view = tdcsophiread.hits_to_numpy_view(hits)      # Zero-copy hit view
neutron_view = tdcsophiread.neutrons_to_numpy_view(neutrons)  # Zero-copy neutron view

# Access underlying data without copying
data_ptr = hits_view.data.data  # Direct memory access
```

## Configuration Examples

### High-Performance Configuration

```python
config = tdcsophiread.NeutronProcessingConfig.venus_defaults()

# Optimize for speed
config.clustering.algorithm = "abs"
config.temporal.num_workers = 0  # Use all cores
config.temporal.max_batch_size = 200000  # Large batches
config.performance.enable_memory_pools = True
config.performance.enable_vectorization = True
```

### Memory-Constrained Configuration

```python
config = tdcsophiread.NeutronProcessingConfig.venus_defaults()

# Optimize for memory
config.temporal.max_batch_size = 50000   # Smaller batches
config.temporal.num_workers = 4          # Fewer workers
config.performance.enable_memory_pools = False
```

### High-Accuracy Configuration

```python
config = tdcsophiread.NeutronProcessingConfig.venus_defaults()

# Optimize for accuracy
config.clustering.algorithm = "dbscan"
config.clustering.dbscan.epsilon = 3.0
config.clustering.dbscan.min_points = 3
config.extraction.weighted_by_tot = True
config.extraction.min_tot_threshold = 10  # Filter low-quality hits
```

For complete examples, see the Jupyter notebooks in the `notebooks/` directory.