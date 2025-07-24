# Legacy Sophiread Components

**⚠️ DEPRECATED - These components will be removed in the next major release ⚠️**

## Overview

This directory contains legacy Sophiread components that have been superseded by the new **TDCSophiread** architecture. These components are deprecated due to fundamental architectural flaws and unreliable TOF calculation methods.

## Why These Components Are Deprecated

### 1. **Unreliable GDC-based TOF Calculation**
- Legacy components use Global Data Clock (GDC) timestamps for time-of-flight calculations
- **Detector experts have disapproved this approach** due to timing inaccuracies
- GDC timestamps are not synchronized with neutron pulse timing

### 2. **Poor Performance**
- Legacy FastSophiread: <10M hits/sec throughput
- **TDCSophiread achieves 96M+ hits/sec** (10x improvement)
- Template-heavy design causes code bloat and memory issues

### 3. **Architectural Problems**
- Complex template hierarchies make code hard to maintain
- No proper parallel processing support
- Memory inefficient clustering algorithms
- Stateful algorithms that don't scale

## Legacy Components

### FastSophiread/
- **Purpose**: Template-heavy implementation with GDC-based processing
- **Issues**: Poor performance, complex templates, unreliable TOF
- **Replacement**: Use TDCSophiread with TDC-only processing

### SophireadCLI/
- **Purpose**: Command-line tools for GDC-based analysis
- **Issues**: Uses deprecated GDC timestamps
- **Replacement**: Use TDCSophiread Python API or CLI tools

### SophireadGUI/
- **Purpose**: Qt-based graphical interface
- **Issues**: Depends on deprecated FastSophiread backend
- **Replacement**: Use Jupyter notebooks with TDCSophiread

### SophireadStreamCLI/
- **Purpose**: Streaming processing CLI
- **Issues**: Memory inefficient, poor parallel processing
- **Replacement**: Use TDCSophiread temporal batching

## Migration Guide

### From FastSophiread to TDCSophiread:

**Old (FastSophiread):**
```cpp
#include "tpx3_fast.h"
FastProcessor processor;
auto result = processor.process(file, config);
```

**New (TDCSophiread):**
```cpp
#include "tdc_processor.h"
TDCProcessor processor(config);
auto hits = processor.processFile(file);
```

**Python Migration:**
```python
# Old API (deprecated)
import fastsophiread
result = fastsophiread.process_gdc(file)

# New API (recommended)
import tdcsophiread
hits = tdcsophiread.process_tpx3(file)
neutrons = tdcsophiread.process_hits_to_neutrons(hits)
```

## Performance Comparison

| Component | Legacy Performance | TDCSophiread Performance | Improvement |
|-----------|-------------------|-------------------------|-------------|
| Hit Processing | <10M hits/sec | 96M+ hits/sec | **10x faster** |
| Memory Usage | High (template bloat) | Optimized (58% reduction) | **2.4x less** |
| Clustering | Sequential only | Parallel TBB | **Multi-core** |
| TOF Accuracy | Poor (GDC-based) | Excellent (TDC-only) | **Detector-approved** |

## Timeline for Removal

- **Current Release**: Legacy components available with `-DBUILD_LEGACY=ON`
- **Next Major Release**: Legacy components will be **completely removed**
- **Recommendation**: Migrate to TDCSophiread immediately

## Technical Details

### Why TDC-only Processing?

1. **TDC packets are synchronized** with neutron pulse generation
2. **GDC timestamps are asynchronous** and unreliable for TOF
3. **Detector experts recommend TDC-only** approach
4. **TDCSophiread corrects missing TDC packets** automatically

### Architecture Improvements

- **Iterator-based interfaces** for zero-copy processing
- **Stateless algorithms** that scale with TBB parallel processing
- **Temporal batching** that respects TPX3 data constraints
- **Memory-efficient** clustering with 58% memory reduction

## Support

For migration assistance or questions about TDCSophiread:
1. Check the main README.md and documentation
2. Review Jupyter notebooks in `notebooks/` directory
3. Examine unit tests in `TDCSophiread/tests/`

**Do not use legacy components for new projects.**