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

set(SRC_FILES
    src/abs.cpp
    src/centroid.cpp
    src/dbscan.cpp
    src/fastgaussian.cpp
    src/tpx3.cpp
)

# ------------- SophireadLib -------------- #
add_library(
    SophireadLib
    ${SRC_FILES}
)

# ----------------- TESTS ----------------- #
# IO Tests
add_executable(SophireadTests_IO tests/test_tpx3.cpp)
target_link_libraries(SophireadTests_IO SophireadLib GTest::GTest GTest::Main hdf5 hdf5_cpp OpenMP::OpenMP_CXX)
gtest_discover_tests(SophireadTests_IO)

# Clustering Tests
add_executable(SophireadTests_CLUSTER tests/test_clustering.cpp)
target_link_libraries(SophireadTests_CLUSTER SophireadLib GTest::GTest GTest::Main hdf5 hdf5_cpp OpenMP::OpenMP_CXX)
gtest_discover_tests(SophireadTests_CLUSTER)

# Peakfitting Tests
add_executable(SophireadTests_PEAKFITTING tests/test_peakfitting.cpp)
target_link_libraries(SophireadTests_PEAKFITTING SophireadLib GTest::GTest GTest::Main hdf5 hdf5_cpp OpenMP::OpenMP_CXX)
gtest_discover_tests(SophireadTests_PEAKFITTING)

# ------------------ Benchmarks ------------------ #
# ABS
add_executable(
    SophireadBenchmarks_ABS
    benchmarks/benchmark_abs.cpp
)
target_link_libraries(
    SophireadBenchmarks_ABS
    SophireadLib
)
# symlink executable to the build directory
add_custom_command(TARGET SophireadLib POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink 
    ${PROJECT_BINARY_DIR}/SophireadLib/SophireadBenchmarks_ABS
    ${PROJECT_BINARY_DIR}/SophireadBenchmarks_ABS.app
)

add_executable(
    SophireadBenchmarks_ABS_Thread
    benchmarks/benchmark_abs_thread.cpp
)
target_link_libraries(
    SophireadBenchmarks_ABS_Thread
    SophireadLib
    pthread
)
# symlink executable to the build directory
add_custom_command(TARGET SophireadLib POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink 
    ${PROJECT_BINARY_DIR}/SophireadLib/SophireadBenchmarks_ABS_Thread
    ${PROJECT_BINARY_DIR}/SophireadBenchmarks_ABS_Thread.app
)

add_executable(
    SophireadBenchmarks_raw2hits
    benchmarks/benchmark_raw2hits.cpp
)
target_link_libraries(
    SophireadBenchmarks_raw2hits
    SophireadLib
    pthread
    tbb
)
# symlink executable to the build directory
add_custom_command(TARGET SophireadLib POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink 
    ${PROJECT_BINARY_DIR}/SophireadLib/SophireadBenchmarks_raw2hits
    ${PROJECT_BINARY_DIR}/SophireadBenchmarks_raw2hits.app
)

# add_executable(
#     SophireadBenchmarks_raw2hits_Thread
#     benchmarks/benchmark_raw2hits_thread.cpp
# )
# target_link_libraries(
#     SophireadBenchmarks_raw2hits_Thread
#     SophireadLib
#     pthread
# )
# # symlink executable to the build directory
# add_custom_command(TARGET SophireadLib POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E create_symlink 
#     ${PROJECT_BINARY_DIR}/SophireadLib/SophireadBenchmarks_raw2hits_Thread
#     ${PROJECT_BINARY_DIR}/SophireadBenchmarks_raw2hits_Thread.app
# )