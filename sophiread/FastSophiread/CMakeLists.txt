# Config FastSophireadLib
set(SRC_FAST_FILES
    src/abs.cpp
    src/centroid.cpp
    src/disk_io.cpp
    src/fastgaussian.cpp
    src/hit.cpp
    src/tpx3_fast.cpp
    src/gdc_processor.cpp)

# ------------- SophireadLibFast -------------- #
add_library(FastSophiread ${SRC_FAST_FILES})
target_include_directories(
  FastSophiread
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
         $<INSTALL_INTERFACE:include> $ENV{CONDA_PREFIX}/include
         ${EIGEN3_INCLUDE_DIR} ${HDF5_INCLUDE_DIRS})
target_link_directories(FastSophiread PRIVATE $ENV{CONDA_PREFIX}/lib)

# Install the library
install(
  TARGETS FastSophiread
  EXPORT sophireadTargets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Install the headers
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# ----------------- TESTS ----------------- #
function(add_sophiread_test test_name)
  add_executable(${test_name} tests/test_${test_name}.cpp)
  target_link_libraries(
    ${test_name}
    PRIVATE FastSophiread
            pthread
            TBB::tbb
            spdlog::spdlog
            GTest::GTest
            GTest::Main
            ${ARGN})
  gtest_discover_tests(${test_name})
endfunction()
# Add tests
add_sophiread_test(disk_io ${HDF5_LIBRARIES})
add_sophiread_test(hit)
add_sophiread_test(tpx3 ${HDF5_LIBRARIES})
add_sophiread_test(abs)
add_sophiread_test(centroid)
add_sophiread_test(fastgaussian)
add_sophiread_test(gdc_processor)

# ------------------ Benchmarks ------------------ # Define a function to add
# benchmark targets
function(add_sophiread_benchmark NAME)
  set(TARGET_NAME SophireadBenchmarks_${NAME})
  add_executable(${TARGET_NAME} benchmarks/benchmark_${NAME}.cpp)

  target_link_libraries(
    ${TARGET_NAME} PRIVATE FastSophiread pthread TBB::tbb spdlog::spdlog
                           ${ARGN} # Additional libraries passed as arguments
  )

  add_custom_command(
    TARGET ${TARGET_NAME}
    POST_BUILD
    COMMAND
      ${CMAKE_COMMAND} -E create_symlink
      ${PROJECT_BINARY_DIR}/FastSophiread/${TARGET_NAME}
      ${PROJECT_BINARY_DIR}/FastSophireadBenchmarks_${NAME}.app)
endfunction()

# Add benchmarks
add_sophiread_benchmark(raw2hits ${HDF5_LIBRARIES})
add_sophiread_benchmark(hits2events)
add_sophiread_benchmark(raw2events ${HDF5_LIBRARIES})

# ----------------- CLI ----------------- #
add_executable(FastSophireadBenchmarksCLI benchmarks/benchmark_mmap.cpp)
target_link_libraries(
  FastSophireadBenchmarksCLI PRIVATE FastSophiread pthread TBB::tbb
                                     spdlog::spdlog ${HDF5_LIBRARIES})
add_custom_command(
  TARGET FastSophireadBenchmarksCLI
  POST_BUILD
  COMMAND
    ${CMAKE_COMMAND} -E create_symlink
    ${PROJECT_BINARY_DIR}/FastSophiread/FastSophireadBenchmarksCLI
    ${PROJECT_BINARY_DIR}/FastSophireadBenchmarksCLI.app
  COMMAND
    ${CMAKE_COMMAND} -E create_symlink
    ${PROJECT_SOURCE_DIR}/FastSophiread/benchmarks/benchmark.sh
    ${PROJECT_BINARY_DIR}/FastSophiread_benchmark.sh)
