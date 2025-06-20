# Sophiread CMakeLists.txt
cmake_minimum_required(VERSION 3.20)

execute_process(
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/version.sh -s print
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  OUTPUT_VARIABLE SOPHIREAD_VERSION
  RESULT_VARIABLE VERSION_RESULT
  OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT VERSION_RESULT EQUAL 0)
  message(FATAL_ERROR "Failed to determine version")
endif()

project("Sophiread" VERSION ${SOPHIREAD_VERSION})

# This is to avoid accidentally using Homebrew header/lib instead of the one
# from micromamba
if(DEFINED ENV{CONDA_PREFIX})
  set(CMAKE_PREFIX_PATH $ENV{CONDA_PREFIX} ${CMAKE_PREFIX_PATH})
endif()

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib")
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# Dependencies
find_package(Eigen3 REQUIRED)
find_package(HDF5 REQUIRED COMPONENTS CXX)
find_package(GTest REQUIRED)
find_package(nlohmann_json 3.2.0 REQUIRED)
find_package(TBB REQUIRED)
find_package(TIFF REQUIRED)
find_package(spdlog 1.8.0 REQUIRED)
find_package(fmt 7.0.0 REQUIRED)

# Set SPDLOG level
if(CMAKE_BUILD_TYPE STREQUAL "Release")
  add_definitions(-DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO)
else()
  add_definitions(-DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG)
endif()

# Testing setup
enable_testing()
include(GoogleTest)

# Copy resources
add_custom_command(
  OUTPUT ${CMAKE_BINARY_DIR}/data
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/resources/data
          ${CMAKE_BINARY_DIR}/data
  DEPENDS ${CMAKE_SOURCE_DIR}/resources/data)
add_custom_target(copy_resources ALL DEPENDS ${CMAKE_BINARY_DIR}/data)

# Add compiler flags
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-march=native -ffast-math -pthread -Wall -Wextra)
endif()

# Add FastSophiread library
add_subdirectory(FastSophiread)
add_subdirectory(SophireadCLI)

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

# For package config
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/sophiread.pc.in
               ${CMAKE_CURRENT_BINARY_DIR}/sophiread.pc @ONLY)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/sophiread.pc
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

# ----------------- DOXYGEN ----------------- #
find_package(Doxygen)
if(DOXYGEN_FOUND)
  file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile
       DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
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
