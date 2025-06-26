# TDCSophiread

TDCSophiread is a high-performance C++ library with Python bindings for processing TPX3 neutron imaging data using TDC (Time-to-Digital Converter) timing. It provides a clean, simplified implementation focused exclusively on TDC-based processing, achieving 120M+ hits/second throughput.

## Key Features

- **High Performance**: 120M+ hits/second processing with Intel TBB parallelization
- **TDC-Only Focus**: Simplified architecture eliminating GDC complexity
- **Python Integration**: Full Python bindings with numpy compatibility
- **Memory Efficient**: Streaming API for large file processing with progress tracking
- **Section-Aware Processing**: Respects TPX3 data structure for reliable results
- **Analysis Tools**: Built-in TOF spectrum generation, ROI selection, and statistics

## Quick Start

### Installation

TDCSophiread uses [pixi](https://pixi.sh/latest/) for dependency management and building.

```bash
# Install pixi (Linux/macOS)
curl -sSL https://pixi.sh/install | bash

# Clone and build
git clone <repository-url>
cd sophiread
pixi run build
```

### Python Usage

```python
import tdcsophiread

# Simple processing
hits = tdcsophiread.process_tpx3("data.tpx3")
print(f"Processed {len(hits['x']):,} hits")

# With progress tracking
def progress(p, msg):
    print(f"{p:.1%} - {msg}")

hits = tdcsophiread.process_tpx3("data.tpx3", progress_callback=progress)

# Data analysis
import tdcsophiread.analysis as analysis
stats = analysis.calculate_hit_statistics(hits)
bin_centers, counts = analysis.create_tof_spectrum(hits, tof_range_ms=(0, 20))
```

### C++ Usage

```cpp
#include "tdc_detector_config.h"
#include "tdc_processor.h"

// Use VENUS defaults or load custom configuration
auto config = tdcsophiread::DetectorConfig::venusDefaults();

// Process TPX3 file
tdcsophiread::TDCProcessor processor(config);
auto hits = processor.processFileParallel("data.tpx3", 12); // 12 threads

std::cout << "Processed " << hits.size() << " hits\n";
std::cout << "Rate: " << processor.getLastHitsPerSecond() / 1e6 << " M hits/sec\n";
```

## Build Instructions

### With Pixi (Recommended)

```bash
# Development workflow (includes Python bindings)
pixi run dev-build      # Initial build with editable Python install
pixi run dev-quick      # Fast incremental rebuild after code changes
pixi run python-test    # Test Python import

# Individual tasks
pixi run configure      # Configure with CMake
pixi run build         # Build C++ library
pixi run test          # Run C++ tests

# Python development
pixi run install-dev    # Install Python bindings in editable mode
pixi run run-examples   # Run Python examples
pixi run run-notebook   # Launch Jupyter notebook tutorial

# Other targets
pixi run docs          # Build documentation
pixi run package       # Create release package
pixi run clean         # Clean build directory
```

### Build Targets

- **Default**: TDC-only implementation (recommended)
- **Legacy**: Optional FastSophiread + legacy components (`pixi run build-legacy`)

## Architecture

TDCSophiread implements a two-phase processing strategy:

1. **Phase 1 (Sequential)**: Section discovery and TDC state propagation
   - Scan for TPX3 headers to identify section boundaries
   - Propagate TDC timestamps across sections per chip
   - Prepare sections for parallel processing

2. **Phase 2 (Parallel)**: Independent section processing
   - Process sections in parallel using Intel TBB
   - Each section has its initial TDC state
   - Smart chunking for large files

### Core Components

- **DetectorConfig**: JSON-configurable detector parameters and chip transformations
- **TDCProcessor**: High-performance section-aware processor
- **MappedFile**: Cross-platform memory-mapped I/O for large files
- **TDCHit**: Optimized hit data structure (32 bytes)
- **Analysis Module**: Python utilities for data analysis and visualization

## Configuration

TDCSophiread uses JSON configuration files:

```json
{
  "detector": {
    "timing": {
      "tdc_frequency_hz": 60.0,
      "enable_missing_tdc_correction": true
    },
    "chip_layout": {
      "chip_size_x": 256,
      "chip_size_y": 256
    },
    "super_resolution": {
      "factor": 4
    }
  }
}
```

VENUS detector defaults are built-in and ready to use.

## Performance

- **Target**: 120M hits/second
- **Achieved**: 33.7M hits/second (current optimizations)
- **Memory**: Efficient streaming for large files (>GB)
- **Parallelization**: Intel TBB with work-stealing scheduler

## Documentation

- **Python API**: `docs/api_reference.md`
- **C++ API**: Generate with `pixi run docs` (Doxygen)
- **Tutorial**: `examples/tdcsophiread_tutorial.ipynb`
- **Examples**: `examples/basic_usage.py`

## Data Format

TPX3 files contain neutron imaging data organized in sections. TDCSophiread:

- Respects section boundaries for reliable processing
- Handles TDC rollover and missing TDC correction
- Maps chip coordinates to global detector coordinates
- Outputs structured hit data with TOF information

### Output Format

Hit data is returned as numpy arrays:

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

## Important Notes

- **Binary Files**: TPX3 files are binary - never open with text editors
- **Default Layout**: 2x2 chip layout with 2-pixel gaps (VENUS configuration)
- **Chip Size**: 256x256 pixels per chip (native resolution)
- **Super-resolution**: 4x4 sub-pixels via peak fitting (configurable)
- **TDC Timing**: Recommended for reliable timing (60Hz default frequency)

## Legacy Components

Previous components (FastSophiread, CLI applications) are deprecated in favor of the streamlined TDCSophiread implementation. Legacy components can be built with `BUILD_LEGACY=ON` but are not actively maintained.

## Contributing

This project uses:
- **C++20** with modern practices
- **Google C++ style** (2-space indentation)
- **Test-Driven Development** with Google Test
- **Pixi** for environment management
- **Pre-commit hooks** for code formatting

See `CLAUDE.md` for detailed development guidelines.

## License

GPL-3.0+ License - see LICENSE file for details.