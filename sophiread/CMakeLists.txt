# Sophiread CMakeLists.txt
cmake_minimum_required(VERSION 3.20)

# Version detection - handle both regular builds and PyPI/scikit-build-core
if(SKBUILD)
  # For scikit-build-core, version is passed from pyproject.toml
  if(DEFINED SKBUILD_PROJECT_VERSION)
    set(SOPHIREAD_VERSION ${SKBUILD_PROJECT_VERSION})
  else()
    # Fallback to default version for scikit-build-core
    set(SOPHIREAD_VERSION "3.1.1")
  endif()
  message(STATUS "Using version from scikit-build-core: ${SOPHIREAD_VERSION}")
elseif(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/scripts/version.sh)
  # Use version.sh if it exists
  execute_process(
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/version.sh -s print
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE SOPHIREAD_VERSION
    RESULT_VARIABLE VERSION_RESULT
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(NOT VERSION_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to determine version from version.sh")
  endif()
else()
  # Fallback: read version from include/version.h if scripts/version.sh doesn't
  # exist
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/include/version.h)
    file(READ ${CMAKE_CURRENT_SOURCE_DIR}/include/version.h VERSION_HEADER)
    string(REGEX MATCH "#define VERSION_MAJOR ([0-9]+)" _ ${VERSION_HEADER})
    set(VERSION_MAJOR ${CMAKE_MATCH_1})
    string(REGEX MATCH "#define VERSION_MINOR ([0-9]+)" _ ${VERSION_HEADER})
    set(VERSION_MINOR ${CMAKE_MATCH_1})
    string(REGEX MATCH "#define VERSION_PATCH ([0-9]+)" _ ${VERSION_HEADER})
    set(VERSION_PATCH ${CMAKE_MATCH_1})
    set(SOPHIREAD_VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")
    message(STATUS "Using version from include/version.h: ${SOPHIREAD_VERSION}")
  else()
    message(
      FATAL_ERROR "Cannot determine version - no version.sh or version.h found")
  endif()
endif()

project("Sophiread" VERSION ${SOPHIREAD_VERSION})

# This is to avoid accidentally using Homebrew header/lib instead of the one
# from micromamba or pixi
if(DEFINED ENV{CONDA_PREFIX})
  set(CMAKE_PREFIX_PATH $ENV{CONDA_PREFIX} ${CMAKE_PREFIX_PATH})
endif()

# For scikit-build-core: respect Python installation prefix
if(DEFINED ENV{PYTHONPATH})
  list(PREPEND CMAKE_PREFIX_PATH $ENV{Python_ROOT_DIR})
endif()

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib")
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Build options - define early so they can be used throughout
option(BUILD_TESTS "Build unit tests (requires GTest)" ON)

# For scikit-build-core (PyPI builds), default to not building tests
if(SKBUILD)
  set(BUILD_TESTS
      OFF
      CACHE BOOL "Build unit tests" FORCE)
  message(STATUS "Building via scikit-build-core, disabling tests by default")
endif()

# Dependencies
find_package(Eigen3 REQUIRED)
find_package(HDF5 REQUIRED COMPONENTS CXX)
find_package(nlohmann_json 3.2.0 REQUIRED)
find_package(TBB REQUIRED)
find_package(TIFF REQUIRED)
find_package(spdlog 1.8.0 REQUIRED)
find_package(fmt 7.0.0 REQUIRED)

# Test dependencies (only when BUILD_TESTS is ON)
if(BUILD_TESTS)
  find_package(GTest REQUIRED)
endif()

# Set SPDLOG level
if(CMAKE_BUILD_TYPE STREQUAL "Release")
  add_definitions(-DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO)
else()
  add_definitions(-DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG)
endif()

# Testing setup (only when BUILD_TESTS is ON)
if(BUILD_TESTS)
  enable_testing()
  include(GoogleTest)

  # Copy test resources
  add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/data
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/resources/data ${CMAKE_BINARY_DIR}/data
    DEPENDS ${CMAKE_SOURCE_DIR}/resources/data)
  add_custom_target(copy_resources ALL DEPENDS ${CMAKE_BINARY_DIR}/data)
endif()

# Add compiler flags
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-march=native -ffast-math -pthread -Wall -Wextra)

  # Ensure maximum optimization for performance-critical code
  if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-O3 -DNDEBUG)
  endif()
endif()

# Legacy build option
option(
  BUILD_LEGACY
  "Build legacy components (DEPRECATED - will be removed in next major release)"
  OFF)

# Add TDCSophiread library (default)
add_subdirectory(TDCSophiread)

# Add legacy implementation (optional, DEPRECATED)
if(BUILD_LEGACY)
  message(
    WARNING
      "
================================================================================
WARNING: Legacy build is DEPRECATED and will be removed in the next major release.
Legacy components (FastSophiread, GUIs) use unreliable GDC-based TOF calculation.
Please migrate to TDCSophiread which uses detector-expert-approved TDC-only approach.
Performance: TDCSophiread achieves 96M+ hits/sec vs legacy <10M hits/sec
================================================================================"
  )
  add_subdirectory(legacy)
else()
  message(STATUS "Skipping legacy components (use -DBUILD_LEGACY=ON to enable)")
endif()

# ----------------- PACKAGE CONFIGURATION ----------------- #

# Export targets from subdirectories
install(
  EXPORT sophireadTargets
  FILE sophireadTargets.cmake
  NAMESPACE sophiread::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sophiread)

# Generate and install the sophireadConfig.cmake and
# sophireadConfigVersion.cmake files
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/sophireadConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY AnyNewerVersion)

configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/sophireadConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/sophireadConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sophiread)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/sophireadConfig.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/sophireadConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sophiread)

# Package config for TDCSophiread (default) and backward compatibility
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/sophiread.pc.in
               ${CMAKE_CURRENT_BINARY_DIR}/tdcsophiread.pc @ONLY)

# Install with both names for compatibility
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/tdcsophiread.pc
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

# Create backward-compatible symlink for existing DAQ systems
install(
  CODE "
  execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
    tdcsophiread.pc
    \${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/pkgconfig/sophiread.pc)
")

# ----------------- DOXYGEN ----------------- #
find_package(Doxygen)
if(DOXYGEN_FOUND)
  # Configure Doxyfile with correct paths for build directory
  file(READ ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile DOXYFILE_CONTENTS)
  string(REPLACE "README.md" "${CMAKE_CURRENT_SOURCE_DIR}/README.md"
                 DOXYFILE_CONTENTS "${DOXYFILE_CONTENTS}")
  string(REPLACE "TDCSophiread/include"
                 "${CMAKE_CURRENT_SOURCE_DIR}/TDCSophiread/include"
                 DOXYFILE_CONTENTS "${DOXYFILE_CONTENTS}")
  string(REPLACE "TDCSophiread/src"
                 "${CMAKE_CURRENT_SOURCE_DIR}/TDCSophiread/src"
                 DOXYFILE_CONTENTS "${DOXYFILE_CONTENTS}")
  file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile "${DOXYFILE_CONTENTS}")

  file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/docs")
  add_custom_target(
    docs
    COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
    COMMENT "Generating API documentation with Doxygen"
    VERBATIM)
  if(APPLE)
    set(OPEN_COMMAND open)
  elseif(UNIX)
    set(OPEN_COMMAND xdg-open)
  elseif(WIN32)
    set(OPEN_COMMAND start)
  endif()
  add_custom_command(
    TARGET docs
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR}/docs/html open
            index.html)
endif(DOXYGEN_FOUND)
