# Configure the sophiread lib that can be used as a subproject
# for other tpx3 projects

find_package(Eigen3 REQUIRED)
find_package(OpenMP REQUIRED)
find_package(HDF5 REQUIRED)

# Add the include directories
include_directories(
    $ENV{CONDA_PREFIX}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${EIGEN3_INCLUDE_DIR}
    ${HDF5_INCLUDE_DIRS}
)

link_directories(
    $ENV{CONDA_PREFIX}/lib
)

# Add the source files
add_library(SophireadLib
    abs.cpp
    dbscan.cpp
    tpx3.cpp
    centroid.cpp
    fastgaussian.cpp
)
set_target_properties(SophireadLib PROPERTIES VERSION ${PROJECT_VERSION})
