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
add_library(
    FastSophireadLib
    ${SRC_FAST_FILES}
)

# ----------------- TESTS ----------------- #
# DiskIO Tests
add_executable(
    SophireadTests_diskio
    tests/test_disk_io.cpp
)
target_link_libraries(
    SophireadTests_diskio
    FastSophireadLib
    pthread
    tbb
    spdlog::spdlog
    GTest::GTest
    GTest::Main
)
gtest_discover_tests(SophireadTests_diskio)
# Hit Test
add_executable(
    SophireadTests_hit
    tests/test_hit.cpp
)
target_link_libraries(
    SophireadTests_hit
    FastSophireadLib
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
    FastSophireadLib
    pthread
    tbb
    spdlog::spdlog
    GTest::GTest
    GTest::Main
)
gtest_discover_tests(SophireadTests_tpx3)
# ABS Test
add_executable(
    SophireadTests_abs
    tests/test_abs.cpp
)
target_link_libraries(
    SophireadTests_abs
    FastSophireadLib
    pthread
    tbb
    spdlog::spdlog
    GTest::GTest
    GTest::Main
)
gtest_discover_tests(SophireadTests_abs)

# ------------------ Benchmarks ------------------ #
add_executable(
    SophireadBenchmarks_raw2hits
    benchmarks/benchmark_raw2hits.cpp
)
target_link_libraries(
    SophireadBenchmarks_raw2hits
    FastSophireadLib
    pthread
    tbb
    spdlog::spdlog
)
# symlink executable to the build directory
add_custom_command(TARGET FastSophireadLib POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink 
    ${PROJECT_BINARY_DIR}/FastSophireadLib/SophireadBenchmarks_raw2hits
    ${PROJECT_BINARY_DIR}/FastSophireadBenchmarks_raw2hits.app
)
