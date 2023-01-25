cmake_minimum_required(VERSION 3.20)
project(Sophiread)

set(CMAKE_CXX_STANDARD 14)

# copy test data directory
file(COPY ${CMAKE_SOURCE_DIR}/resources/data DESTINATION ${CMAKE_BINARY_DIR})

# ----------------- APPLICATION ----------------- #
# Add source files
set(SOURCES
    main.cpp
    src/ars.cpp
    src/dbscan.cpp
    src/tpx3.cpp
)

# Add header files
set(HEADERS
    include/ars.h
    include/clustering.h
    include/dbscan.h
    include/tpx3.h
)

# Add include directories
include_directories(include)

# Add compiler flags
add_compile_options(-O3 -std=c++11 -Wall -Wextra)

# Add executable
add_executable(Sophiread ${SOURCES} ${HEADERS})


# ----------------- TESTS ----------------- #
enable_testing()
find_package(GTest REQUIRED)
include(GoogleTest)

# IO Tests
set(TESTS_IO
    include/tpx3.h
    src/tpx3.cpp
    tests/test_tpx3.cpp
)
add_executable(SophireadTests_IO ${TESTS_IO})
target_link_libraries(SophireadTests_IO GTest::GTest GTest::Main)
gtest_discover_tests(SophireadTests_IO)

# Clustering Tests
set(TESTS_ClUSTER
    include/ars.h
    src/ars.cpp
    include/dbscan.h
    src/dbscan.cpp
    tests/test_clustering.cpp
)
add_executable(SophireadTests_CLUSTER ${TESTS_ClUSTER})
target_link_libraries(SophireadTests_CLUSTER GTest::GTest GTest::Main)
gtest_discover_tests(SophireadTests_CLUSTER)

# Peakfitting Tests
set(TESTS_PEAKFITTING
    include/centroid.h
    src/centroid.cpp
    include/fastgaussian.h
    src/fastgaussian.cpp
    tests/test_peakfitting.cpp
)
add_executable(SophireadTests_PEAKFITTING ${TESTS_PEAKFITTING})
target_link_libraries(SophireadTests_PEAKFITTING GTest::GTest GTest::Main)
gtest_discover_tests(SophireadTests_PEAKFITTING)
