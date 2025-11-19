# GitHub Issue: `process_tpx3_stream()` Misleading Name and Memory Limitation

## Title
`process_tpx3_stream()` does not provide bounded-memory streaming - misleading function name

## Issue Type
- [x] Bug (misleading API)
- [x] Documentation
- [ ] Enhancement

## Severity
**High** - Users following documentation may run out of memory on large files

## Description

The function `process_tpx3_stream()` has a misleading name that suggests it provides memory-efficient streaming processing. However, it still accumulates **all hits in memory** before returning, making it unsuitable for very large files (>100GB).

### Current Behavior

```python
# This function name suggests streaming, but it's NOT bounded-memory!
hits = tdcsophiread.process_tpx3_stream("500GB_file.tpx3", chunk_size_mb=512)
# Result: ~600GB of RAM required (all hits loaded into memory)
```

**Implementation** (tdcsophiread_python.cpp:626-655):
```cpp
m.def("process_tpx3_stream",
  [](const std::string& file_path, size_t chunk_size_mb = 512, ...) {
    auto hits = processor.processFile(file_path, chunk_size_mb, false, 0);
    return TDCHitView(std::move(hits));  // ❌ Returns ALL hits in memory!
  });
```

### What Users Expect

Based on the function name "stream", users expect:
- Constant memory usage regardless of file size
- Incremental processing without loading entire dataset
- Ability to process 500GB+ files on systems with <32GB RAM

### What Actually Happens

- Memory usage = `num_hits × ~40-60 bytes`
- For 500GB file with 12 billion hits: ~600GB RAM required
- No different from `process_tpx3()` in terms of memory usage
- The only difference is `parallel=False` by default

### Impact

**Critical for users with large datasets:**
- Users following quickstart.md or api_reference.md will use this function for large files
- They will run out of memory and crash
- Function name creates false expectation of bounded-memory processing

### Actual Bounded-Memory Solution

The correct function for large files is `process_tpx3_to_hdf5()` (added in recent commits):

```python
# TRUE bounded-memory streaming (constant ~512MB memory)
result = tdcsophiread.process_tpx3_to_hdf5(
    "500GB_file.tpx3",
    "output.h5",
    parallel=True,
    chunk_size_mb=512
)
print(f"Processed {result.total_hits} hits with constant memory")
```

## Proposed Solutions

### Option 1: Rename the function (Breaking Change)
```python
# More accurate name
hits = tdcsophiread.process_tpx3_chunked("file.tpx3")  # Still loads all in memory
```

### Option 2: Deprecate and redirect (Backward Compatible)
```python
@deprecated("Use process_tpx3() for in-memory or process_tpx3_to_hdf5() for streaming")
def process_tpx3_stream(...):
    warnings.warn("process_tpx3_stream() loads all hits in memory...")
    return process_tpx3(...)
```

### Option 3: Make it actually stream (Best Solution)
Implement true streaming that yields hits in batches:
```python
for hit_batch in tdcsophiread.process_tpx3_stream("file.tpx3"):
    # Process each batch separately
    process_batch(hit_batch)
```

## Documentation Impact

**Files requiring updates:**
- `docs/quickstart.md` (lines 171-182, 332-333)
- `docs/api_reference.md` (lines 208-220, 332-333)
- `README.md` (line 504)

**Current documentation claims:**
> "Memory-efficient streaming for large files"
> "Process large TPX3 files with chunk-based memory mapping"

**Reality:**
- NOT memory-efficient for large files (loads all data)
- Chunk-based reading, but not chunk-based memory usage
- Should use `process_tpx3_to_hdf5()` for true bounded-memory streaming

## Reproduction

```python
import tdcsophiread
import psutil
import os

process = psutil.Process(os.getpid())

# Monitor memory before
mem_before = process.memory_info().rss / 1024**3  # GB

# Try to "stream" a large file
hits = tdcsophiread.process_tpx3_stream("large_file.tpx3")

# Monitor memory after
mem_after = process.memory_info().rss / 1024**3  # GB

print(f"Memory increased by: {mem_after - mem_before:.2f} GB")
print(f"Expected for streaming: ~0.5 GB")
print(f"Actual: Grows with file size")
```

## Environment
- TDCSophiread version: 3.1.3+
- Python version: Any
- Platform: All

## Related Issues
- Documentation issue: Missing docs for `process_tpx3_to_hdf5()`
- CLI `--streaming` flag uses correct implementation but not documented

## Priority
**High** - Affects users with large datasets, can cause system crashes due to OOM

## Labels
- bug
- documentation
- api-design
- memory-optimization
- breaking-change (if renamed)
