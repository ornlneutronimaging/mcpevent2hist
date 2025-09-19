# TDCSophiread CMakeLists.txt TDC-only implementation for high-performance TPX3
# data processing

cmake_minimum_required(VERSION 3.20)

# Include GNUInstallDirs for standard installation directories
include(GNUInstallDirs)

# Project will inherit version from parent project
message(STATUS "Configuring TDCSophiread (TDC-only implementation)")

# Find required dependencies
find_package(TBB REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(Eigen3 REQUIRED)
find_package(HDF5 COMPONENTS CXX)

# Core TDC processing sources (clustering disabled for clean implementation)
set(TDC_SOURCES
    src/tdc_detector_config.cpp
    src/tdc_io.cpp
    src/tdc_hit.cpp
    src/tdc_processor.cpp
    src/tdc_neutron.cpp
    # Streaming components (TDD approach)
    src/tdc_temporal_window.cpp
    src/tdc_streaming_config.cpp
    # New neutron processing architecture
    src/neutron_processing/neutron_config.cpp
    src/neutron_processing/neutron_factories.cpp
    src/neutron_processing/simple_centroid_extraction.cpp
    # Temporal processing framework
    src/neutron_processing/hit_batching.cpp
    src/neutron_processing/temporal_neutron_processor.cpp
    # Stateless clustering support
    src/neutron_processing/clustering_state.cpp
    src/neutron_processing/abs_clustering.cpp
    src/neutron_processing/graph_clustering.cpp
    src/neutron_processing/dbscan_clustering.cpp
    src/neutron_processing/grid_clustering.cpp)

# Legacy clustering sources moved to legacy/ and disabled
# src/legacy/tdc_clustering_config.cpp src/legacy/tdc_abs_clustering.cpp
# src/legacy/tdc_graph_clustering.cpp src/legacy/tdc_centroid_fitting.cpp
# src/legacy/tdc_cluster_processor.cpp

# Create TDCSophiread library target
add_library(TDCSophiread ${TDC_SOURCES})

# Set up include directories
target_include_directories(
  TDCSophiread PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                      $<INSTALL_INTERFACE:include> ${HDF5_INCLUDE_DIRS})

# Set C++20 standard
target_compile_features(TDCSophiread PUBLIC cxx_std_20)

# Add debug timing for performance analysis (disabled)
# target_compile_definitions(TDCSophiread PRIVATE DEBUG_TIMING)

# Required dependencies (same as FastSophiread + HDF5 for streaming)
target_link_libraries(TDCSophiread PUBLIC TBB::tbb nlohmann_json::nlohmann_json
                                          Eigen3::Eigen ${HDF5_LIBRARIES})

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
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
  find_package(GTest REQUIRED)
  enable_testing()
  include(GoogleTest)
endif()

if(BUILD_TESTS)
  # ----------------- TDD TEST INFRASTRUCTURE -----------------
  # Function to add TDC tests following TDD approach
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

  # Function to add neutron processing tests
  function(add_neutron_test test_name)
    add_executable(${test_name} tests/neutron_processing/test_${test_name}.cpp)
    target_link_libraries(
      ${test_name} PRIVATE TDCSophiread TBB::tbb nlohmann_json::nlohmann_json
                           GTest::GTest GTest::Main ${ARGN})
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

  # TDD Test 6: Neutron data structure and utilities (tests written FIRST)
  add_tdc_test(neutron)

  # Legacy clustering tests disabled - moved to legacy/
  # add_tdc_test(clustering_config) add_tdc_test(abs_clustering)
  # add_tdc_test(graph_clustering) add_tdc_test(centroid_fitting)
  # add_tdc_test(cluster_processor)

  # Legacy performance and integration tests disabled
  # option(BUILD_PERFORMANCE_TESTS "Build performance benchmark tests" OFF)
  # if(BUILD_PERFORMANCE_TESTS) add_tdc_test(clustering_performance) endif()
  # add_tdc_test(clustering_integration)

  # Streaming Tests: Memory-efficient streaming components (to be implemented
  # with TDD)
  add_tdc_test(streaming_config)
  # add_tdc_test(memory_monitor)
  add_tdc_test(temporal_window) # TDD Complete: 100% tests passing
  # add_tdc_test(async_hdf5_writer) add_tdc_test(streaming_processor)

  # ----------------- NEUTRON PROCESSING TESTS -----------------
  # New neutron processing architecture tests
  add_neutron_test(neutron_interfaces) # Interface and factory tests
  add_neutron_test(simple_centroid_extraction) # Centroid extraction tests
  add_neutron_test(graph_clustering) # Graph clustering algorithm tests
  add_neutron_test(dbscan_clustering) # DBSCAN clustering algorithm tests
  add_neutron_test(dbscan_parallel_safety) # DBSCAN parallel safety and memory
                                           # tests
  add_neutron_test(grid_clustering) # Grid clustering algorithm tests
  # Temporal processing tests
  add_neutron_test(temporal_neutron_processor) # Parallel temporal processor
                                               # tests
  add_neutron_test(tbb_parallel_verification) # TBB parallel processing
                                              # verification tests
  add_neutron_test(tbb_stress) # TBB stress testing with large datasets

  # Performance debugging test
  add_executable(test_ni_powder_clustering tests/test_ni_powder_clustering.cpp)
  target_link_libraries(test_ni_powder_clustering TDCSophiread)

  # Graph clustering benchmark
  add_executable(graph_clustering_benchmark
                 tests/neutron_processing/test_graph_clustering_benchmark.cpp)
  target_link_libraries(graph_clustering_benchmark TDCSophiread TBB::tbb)
  # Performance benchmarks add_neutron_test(abs_performance) # ABS clustering
  # optimization benchmarks - DISABLED (needs update) Memory optimization tests
  # add_neutron_test(memory_optimization) # Memory usage and zero-copy
  # validation - DISABLED (needs update)

endif() # BUILD_TESTS

# ----------------- PYTHON BINDINGS -----------------
option(BUILD_PYTHON_BINDINGS "Build Python bindings" ON)

if(BUILD_PYTHON_BINDINGS)
  add_subdirectory(python)
endif()
