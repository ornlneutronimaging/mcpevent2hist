cmake_minimum_required(VERSION 3.20)
project(Sophiread)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_AUTOMOC ON)  # for meta object compiler, needed for Qt5

# copy test data directory
file(COPY ${CMAKE_SOURCE_DIR}/resources/data DESTINATION ${CMAKE_BINARY_DIR})

find_package(OpenMP REQUIRED)
find_package(Qt5 COMPONENTS Widgets REQUIRED)
find_package(HDF5 REQUIRED)

# Add include&linking directories
include_directories(
    include
    $ENV{CONDA_PREFIX}/include
    ${HDF5_INCLUDE_DIRS}
)
link_directories(
    $ENV{CONDA_PREFIX}/lib
)


# Add compiler flags
add_compile_options(-O3 -std=c++11 -Wall -Wextra ${OpenMP_CXX_FLAGS})

# ----------------- LIBRARY ----------------- #
add_library(SophireadLib
    src/abs.cpp
    src/dbscan.cpp
    src/tpx3.cpp
    src/centroid.cpp
    src/fastgaussian.cpp
)

# ----------------- CLI APPLICATION ----------------- #
add_executable(Sophiread
    sophiread.cpp
)
target_link_libraries(Sophiread SophireadLib OpenMP::OpenMP_CXX)

# ------------------ GUI APPLICATION ------------------ #
qt5_wrap_ui(SophireadDisplayUI
    ui/mainwindow.ui
)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
add_executable(SophireadDisplay
    sophiread_display.cpp
    include/mainwindow.h
    src/mainwindow.cpp
    ${SophireadDisplayUI}
)
target_link_libraries(SophireadDisplay SophireadLib Qt5::Widgets qwt hdf5 hdf5_cpp OpenMP::OpenMP_CXX)


# ----------------- TESTS ----------------- #
enable_testing()
find_package(GTest REQUIRED)
include(GoogleTest)

# IO Tests
add_executable(SophireadTests_IO tests/test_tpx3.cpp)
target_link_libraries(SophireadTests_IO SophireadLib GTest::GTest GTest::Main OpenMP::OpenMP_CXX)
gtest_discover_tests(SophireadTests_IO)

# Clustering Tests
add_executable(SophireadTests_CLUSTER tests/test_clustering.cpp)
target_link_libraries(SophireadTests_CLUSTER SophireadLib GTest::GTest GTest::Main OpenMP::OpenMP_CXX)
gtest_discover_tests(SophireadTests_CLUSTER)

# Peakfitting Tests
add_executable(SophireadTests_PEAKFITTING tests/test_peakfitting.cpp)
target_link_libraries(SophireadTests_PEAKFITTING SophireadLib GTest::GTest GTest::Main OpenMP::OpenMP_CXX)
gtest_discover_tests(SophireadTests_PEAKFITTING)


# ----------------- INSTALL ----------------- #
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "/usr/local" CACHE PATH "Default install prefix" FORCE)
endif()
install(TARGETS Sophiread
        RUNTIME DESTINATION bin)
