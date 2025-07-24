# Legacy Sophiread Components CMakeLists.txt DEPRECATED: These components will
# be removed in the next major release
#
# This file contains the legacy build configuration for: - FastSophiread
# (template-heavy implementation) - SophireadCLI (GDC-based CLI tools) -
# SophireadGUI (Qt-based GUI application) - SophireadStreamCLI (legacy streaming
# CLI)
#
# These components use the deprecated GDC-based TOF calculation which has been
# disapproved by detector experts. Please migrate to TDCSophiread.

cmake_minimum_required(VERSION 3.20)

message(STATUS "Building legacy Sophiread components (DEPRECATED)")
message(WARNING "Legacy components use unreliable GDC-based TOF calculation")
message(WARNING "Please migrate to TDCSophiread for production use")

# Legacy dependencies that are no longer required for TDCSophiread
find_package(
  Qt5
  COMPONENTS Core Widgets Charts
  QUIET)

# FastSophiread - Legacy template-heavy implementation
message(STATUS "Building legacy FastSophiread")
add_subdirectory(FastSophiread)

# SophireadCLI - Legacy CLI tools with GDC
message(STATUS "Building legacy SophireadCLI")
add_subdirectory(SophireadCLI)

# SophireadGUI - Qt-based GUI (only if Qt5 is available)
if(Qt5_FOUND)
  message(STATUS "Building legacy SophireadGUI")
  add_subdirectory(SophireadGUI)
else()
  message(WARNING "Qt5 not found - skipping legacy SophireadGUI")
endif()

# SophireadStreamCLI - Legacy streaming CLI
message(STATUS "Building legacy SophireadStreamCLI")
add_subdirectory(SophireadStreamCLI)

# Legacy export configuration
install(
  EXPORT sophireadLegacyTargets
  FILE sophireadLegacyTargets.cmake
  NAMESPACE sophiread::legacy::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sophiread/legacy
  OPTIONAL)
