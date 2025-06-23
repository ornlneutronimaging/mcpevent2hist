# TDCSophiread CMakeLists.txt
# TDC-only implementation for high-performance TPX3 data processing

cmake_minimum_required(VERSION 3.20)

# Project will inherit version from parent project
message(STATUS "Configuring TDCSophiread (TDC-only implementation)")

# Source files (starting with placeholder, will be replaced with TDD implementation)
set(TDC_SOURCES
    src/tdc_placeholder.cpp
)

# Create TDCSophiread library target
add_library(TDCSophiread ${TDC_SOURCES})

# Set up include directories
target_include_directories(TDCSophiread
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Set C++20 standard
target_compile_features(TDCSophiread PUBLIC cxx_std_20)

# Required dependencies (same as FastSophiread for now)
target_link_libraries(TDCSophiread 
    PUBLIC 
        TBB::tbb
        nlohmann_json::nlohmann_json
        Eigen3::Eigen
)

# Installation configuration
install(
    TARGETS TDCSophiread
    EXPORT sophireadTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install headers
install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Enable testing for this directory
enable_testing()

# Note: Tests, benchmarks, and source files will be added incrementally
# following TDD approach