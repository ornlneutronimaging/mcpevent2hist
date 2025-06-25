# Python bindings for TDCSophiread

# Find Python and pybind11
find_package(
  Python
  COMPONENTS Interpreter Development
  REQUIRED)
find_package(pybind11 REQUIRED)

# Create Python module
pybind11_add_module(tdcsophiread tdcsophiread_python.cpp)

# Link to TDC library
target_link_libraries(tdcsophiread PRIVATE TDCSophiread)

# Set properties
set_target_properties(
  tdcsophiread
  PROPERTIES CXX_STANDARD 20
             CXX_STANDARD_REQUIRED ON
             # Remove lib prefix (we want tdcsophiread.so not
             # libtdcsophiread.so)
             PREFIX "")

# Pass version information
target_compile_definitions(tdcsophiread
                           PRIVATE PROJECT_VERSION="${PROJECT_VERSION}")

# Installation
install(TARGETS tdcsophiread LIBRARY DESTINATION ${Python_SITELIB})
