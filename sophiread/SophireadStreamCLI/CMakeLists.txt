# Configure the CLI app using streaming mode

# For concurrent queue
include(FetchContent)

FetchContent_Declare(
    readerwriterqueue
    GIT_REPOSITORY    https://github.com/cameron314/readerwriterqueue
    GIT_TAG           master
)

FetchContent_MakeAvailable(readerwriterqueue)

# Add the source files
set(SRC_FILES
    src/sophiread_stream.cpp
)

# Add inlcude directories
include_directories(
    ${PROJECT_SOURCE_DIR}/SophireadLib/include
)

# ------------------ CLI STREAM DEMO ------------------ #
add_executable(SophireadStream
    ${SRC_FILES}
)
set_target_properties(SophireadStream PROPERTIES VERSION ${PROJECT_VERSION})
target_link_libraries(SophireadStream PUBLIC readerwriterqueue SophireadLib hdf5 hdf5_cpp OpenMP::OpenMP_CXX)

# symlink the executable to the build directory
add_custom_command(TARGET SophireadStream POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink 
    ${PROJECT_BINARY_DIR}/SophireadStreamCLI/SophireadStream
    ${PROJECT_BINARY_DIR}/SophireadStream
)
