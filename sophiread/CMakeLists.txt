cmake_minimum_required(VERSION 3.20)

execute_process(COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/version.sh -s print
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE SOPHIREAD_VERSION)

project("Sophiread" VERSION ${SOPHIREAD_VERSION})

# set(CMAKE_BUILD_TYPE DEBUG)
set(CMAKE_CXX_STANDARD 14)
# set(CMAKE_AUTOMOC ON)  # for meta object compiler, needed for Qt5

# Dependencies
find_package(Eigen3 REQUIRED)
find_package(OpenMP REQUIRED)
find_package(HDF5 REQUIRED)
find_package(GTest REQUIRED)

# Testing setup
enable_testing()
include(GoogleTest)
file(COPY ${CMAKE_SOURCE_DIR}/resources/data DESTINATION ${CMAKE_BINARY_DIR})

# Add compiler flags
add_compile_options(-O3 -std=c++14 -pthread -Wall -Wextra ${OpenMP_CXX_FLAGS})

# Add Sophiread library
# NOTE: this will take care of the testing as well
add_subdirectory(SophireadLib)

# Add Sophiread command line interface
add_subdirectory(SophireadCLI)
