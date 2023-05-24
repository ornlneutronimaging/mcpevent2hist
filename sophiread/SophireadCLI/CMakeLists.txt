# Configure the commandline application
set(SRC_FILES
    src/sophiread.cpp
)

# Include the headers
include_directories(
    ${PROJECT_SOURCE_DIR}/SophireadLib/include
)

# ----------------- CLI APPLICATION ----------------- #
add_executable(Sophiread
    ${SRC_FILES}
)

set_target_properties(Sophiread PROPERTIES VERSION ${PROJECT_VERSION})
target_link_libraries(Sophiread SophireadLib hdf5 hdf5_cpp OpenMP::OpenMP_CXX)

# symlink the executable to the build directory
add_custom_command(TARGET Sophiread POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink 
    ${PROJECT_BINARY_DIR}/SophireadCLI/Sophiread
    ${PROJECT_BINARY_DIR}/Sophiread
)
