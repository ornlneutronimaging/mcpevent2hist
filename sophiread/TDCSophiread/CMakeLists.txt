# TDCSophiread CMakeLists.txt TDC-only implementation for high-performance TPX3
# data processing

cmake_minimum_required(VERSION 3.20)

# Project will inherit version from parent project
message(STATUS "Configuring TDCSophiread (TDC-only implementation)")

# Source files (TDD implementation in progress)
set(TDC_SOURCES
    src/tdc_detector_config.cpp
    src/tdc_io.cpp
    src/tdc_hit.cpp
    src/tdc_processor.cpp
    src/tdc_clustering_config.cpp
    src/tdc_neutron.cpp
    src/tdc_abs_clustering.cpp
    src/tdc_centroid_fitting.cpp
    src/tdc_cluster_processor.cpp)

# Create TDCSophiread library target
add_library(TDCSophiread ${TDC_SOURCES})

# Set up include directories
target_include_directories(
  TDCSophiread PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                      $<INSTALL_INTERFACE:include>)

# Set C++20 standard
target_compile_features(TDCSophiread PUBLIC cxx_std_20)

# Add debug timing for performance analysis (disabled)
# target_compile_definitions(TDCSophiread PRIVATE DEBUG_TIMING)

# Required dependencies (same as FastSophiread for now)
target_link_libraries(TDCSophiread PUBLIC TBB::tbb nlohmann_json::nlohmann_json
                                          Eigen3::Eigen)

# Installation configuration
install(
  TARGETS TDCSophiread
  EXPORT sophireadTargets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# Install headers
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Enable testing for this directory
enable_testing()
include(GoogleTest)

# ----------------- TDD TEST INFRASTRUCTURE ----------------- # Function to add
# TDC tests following TDD approach
function(add_tdc_test test_name)
  add_executable(${test_name} tests/test_${test_name}.cpp)
  target_link_libraries(
    ${test_name}
    PRIVATE TDCSophiread TBB::tbb nlohmann_json::nlohmann_json GTest::GTest
            GTest::Main ${ARGN} # Additional libraries can be passed as
                                # arguments
  )
  gtest_discover_tests(${test_name})

  # Set test properties for better output
  set_target_properties(${test_name} PROPERTIES CXX_STANDARD 20
                                                CXX_STANDARD_REQUIRED ON)
endfunction()

# TDD Test 1: DetectorConfig (tests written FIRST)
add_tdc_test(detector_config)

# TDD Test 2: Memory-mapped I/O (tests written FIRST)
add_tdc_test(io)

# TDD Test 3: TPX3 Packet parsing (tests written FIRST)
add_tdc_test(packet)

# TDD Test 4: Hit conversion and TDC correction (tests written FIRST)
add_tdc_test(hit)

# TDD Test 5: Basic single-threaded processor (tests written FIRST)
add_tdc_test(processor)

# TDD Test 6: Clustering configuration system (tests written FIRST)
add_tdc_test(clustering_config)

# TDD Test 7: Neutron data structure and utilities (tests written FIRST)
add_tdc_test(neutron)

# TDD Test 8: ABS clustering algorithm (tests written FIRST)
add_tdc_test(abs_clustering)

# DEBUG Test: ABS clustering debug with detailed logging
add_tdc_test(abs_debug)

# TDD Test 9: Centroid peak fitting algorithm (tests written FIRST)
add_tdc_test(centroid_fitting)

# TDD Test 10: Complete cluster processor pipeline (tests written FIRST)
add_tdc_test(cluster_processor)

# Performance Test: Clustering performance validation (disabled pending
# realistic expectations) Note: Original tests expected 120M hits/sec
# single-threaded, but this requires 32-core system and full
# TPX3->hits->neutrons pipeline. Clustering alone is only 10-15% of processing
# time.
option(BUILD_PERFORMANCE_TESTS "Build performance benchmark tests" OFF)
if(BUILD_PERFORMANCE_TESTS)
  add_tdc_test(clustering_performance)
endif()

# Integration Test: End-to-end clustering workflow validation
add_tdc_test(clustering_integration)

# Note: Additional test targets will be added incrementally following TDD
# approach Each test will be written BEFORE the corresponding implementation

# ----------------- PYTHON BINDINGS (Phase 3) -----------------
option(BUILD_PYTHON_BINDINGS "Build Python bindings" ON)

if(BUILD_PYTHON_BINDINGS)
  add_subdirectory(python)
endif()
