# Python bindings for TDCSophiread

# Find Python and pybind11
find_package(
  Python
  COMPONENTS Interpreter Development
  REQUIRED)
find_package(pybind11 REQUIRED)

# Create Python module (use _core to avoid naming conflict with package)
pybind11_add_module(_core tdcsophiread_python.cpp)

# Link to TDC library
target_link_libraries(_core PRIVATE TDCSophiread)

# Set properties
set_target_properties(
  _core
  PROPERTIES CXX_STANDARD 20
             CXX_STANDARD_REQUIRED ON
             # Remove lib prefix (we want tdcsophiread.so not
             # libtdcsophiread.so)
             PREFIX "")

# Pass version information
target_compile_definitions(_core PRIVATE PROJECT_VERSION="${PROJECT_VERSION}")

# Installation - compatible with scikit-build-core
if(SKBUILD)
  # Install into the tdcsophiread package directory
  install(TARGETS _core DESTINATION tdcsophiread)
else()
  # Development installation
  install(TARGETS _core LIBRARY DESTINATION ${Python_SITELIB})
endif()

# Add to python_bindings component for scikit-build-core
set_target_properties(_core PROPERTIES INSTALL_RPATH "$ORIGIN")

# Create an install component for scikit-build-core
install(
  TARGETS _core
  COMPONENT python_bindings
  DESTINATION tdcsophiread)
