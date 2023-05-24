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
