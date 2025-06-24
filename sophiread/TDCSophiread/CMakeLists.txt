# TDCSophiread CMakeLists.txt TDC-only implementation for high-performance TPX3
# data processing

cmake_minimum_required(VERSION 3.20)

# Project will inherit version from parent project
message(STATUS "Configuring TDCSophiread (TDC-only implementation)")

# Source files (TDD implementation in progress)
set(TDC_SOURCES src/tdc_detector_config.cpp src/tdc_io.cpp)

# Create TDCSophiread library target
add_library(TDCSophiread ${TDC_SOURCES})

# Set up include directories
target_include_directories(
  TDCSophiread PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                      $<INSTALL_INTERFACE:include>)

# Set C++20 standard
target_compile_features(TDCSophiread PUBLIC cxx_std_20)

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

# Note: Additional test targets will be added incrementally following TDD
# approach Each test will be written BEFORE the corresponding implementation
