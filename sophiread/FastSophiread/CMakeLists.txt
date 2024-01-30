# Config SophireadLib
include_directories(
    include
    $ENV{CONDA_PREFIX}/include
    ${EIGEN3_INCLUDE_DIR}
    ${HDF5_INCLUDE_DIRS}
)

link_directories(
    $ENV{CONDA_PREFIX}/lib
)

set(SRC_FAST_FILES
    src/abs.cpp
    src/centroid.cpp
    src/disk_io.cpp
    src/fastgaussian.cpp
    src/hit.cpp
    src/tpx3_fast.cpp
)

# ------------- SophireadLibFast -------------- #
add_library(FastSophiread ${SRC_FAST_FILES})

# ----------------- TESTS ----------------- #
# DiskIO Tests
add_executable(
    SophireadTests_diskio
    tests/test_disk_io.cpp
)
target_link_libraries(
    SophireadTests_diskio
    FastSophiread
    tbb
    spdlog::spdlog
    GTest::GTest
    GTest::Main
    ${HDF5_LIBRARIES}
)
gtest_discover_tests(SophireadTests_diskio)
# Hit Test
add_executable(
    SophireadTests_hit
    tests/test_hit.cpp
)
target_link_libraries(
    SophireadTests_hit
    FastSophiread
    pthread
    tbb
    spdlog::spdlog
    GTest::GTest
    GTest::Main
)
gtest_discover_tests(SophireadTests_hit)
# TPX3 Test
add_executable(
    SophireadTests_tpx3
    tests/test_tpx3.cpp
)
target_link_libraries(
    SophireadTests_tpx3
    FastSophiread
    pthread
    tbb
    spdlog::spdlog
    GTest::GTest
    GTest::Main
    ${HDF5_LIBRARIES}
)
gtest_discover_tests(SophireadTests_tpx3)
# ABS Test
add_executable(
    SophireadTests_abs
    tests/test_abs.cpp
)
target_link_libraries(
    SophireadTests_abs
    FastSophiread
    pthread
    tbb
    spdlog::spdlog
    GTest::GTest
    GTest::Main
)
gtest_discover_tests(SophireadTests_abs)
# Centroid Test
add_executable(
    SophireadTests_centroid
    tests/test_centroid.cpp
)
target_link_libraries(
    SophireadTests_centroid
    FastSophiread
    pthread
    tbb
    spdlog::spdlog
    GTest::GTest
    GTest::Main
)
gtest_discover_tests(SophireadTests_centroid)
# Fast Gaussian Test
add_executable(
    SophireadTests_fastgaussian
    tests/test_fastgaussian.cpp
)
target_link_libraries(
    SophireadTests_fastgaussian
    FastSophiread
    pthread
    tbb
    spdlog::spdlog
    GTest::GTest
    GTest::Main
)
gtest_discover_tests(SophireadTests_fastgaussian)

# ------------------ Benchmarks ------------------ #
# Raw2Hits Benchmark
add_executable(
    SophireadBenchmarks_raw2hits
    benchmarks/benchmark_raw2hits.cpp
)
target_link_libraries(
    SophireadBenchmarks_raw2hits
    FastSophiread
    pthread
    tbb
    spdlog::spdlog
    ${HDF5_LIBRARIES}
)
add_custom_command(TARGET SophireadBenchmarks_raw2hits POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink 
    ${PROJECT_BINARY_DIR}/FastSophiread/SophireadBenchmarks_raw2hits
    ${PROJECT_BINARY_DIR}/FastSophireadBenchmarks_raw2hits.app
)
# Hits2Events Benchmark
add_executable(
    SophireadBenchmarks_hits2events
    benchmarks/benchmark_hits2events.cpp
)
target_link_libraries(
    SophireadBenchmarks_hits2events
    FastSophiread
    pthread
    tbb
    spdlog::spdlog
)
add_custom_command(TARGET SophireadBenchmarks_hits2events POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink 
    ${PROJECT_BINARY_DIR}/FastSophiread/SophireadBenchmarks_hits2events
    ${PROJECT_BINARY_DIR}/FastSophireadBenchmarks_hits2events.app
)
# Raw2Events Benchmark
add_executable(
    SophireadBenchmarks_raw2events
    benchmarks/benchmark_raw2events.cpp
)
target_link_libraries(
    SophireadBenchmarks_raw2events
    FastSophiread
    pthread
    tbb
    spdlog::spdlog
    ${HDF5_LIBRARIES}
)
add_custom_command(TARGET SophireadBenchmarks_raw2events POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink 
    ${PROJECT_BINARY_DIR}/FastSophiread/SophireadBenchmarks_raw2events
    ${PROJECT_BINARY_DIR}/FastSophireadBenchmarks_raw2events.app
)
# CLI
add_executable(
    FastSophireadBenchmarksCLI
    benchmarks/benchmark_mmap.cpp
)
target_link_libraries(
    FastSophireadBenchmarksCLI
    FastSophiread
    pthread
    tbb
    spdlog::spdlog
    ${HDF5_LIBRARIES}
)
add_custom_command(TARGET FastSophireadBenchmarksCLI POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink
    ${PROJECT_BINARY_DIR}/FastSophiread/FastSophireadBenchmarksCLI
    ${PROJECT_BINARY_DIR}/FastSophireadBenchmarksCLI.app

    COMMAND ${CMAKE_COMMAND} -E create_symlink
    ${PROJECT_SOURCE_DIR}/FastSophiread/benchmarks/benchmark.sh
    ${PROJECT_BINARY_DIR}/FastSophiread_benchmark.sh
)
