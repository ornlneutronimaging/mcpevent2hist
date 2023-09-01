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
    src/tpx3_fast.cpp
    src/disk_io.cpp
)

# ------------- SophireadLibFast -------------- #
add_library(
    FastSophireadLib
    ${SRC_FAST_FILES}
)

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
