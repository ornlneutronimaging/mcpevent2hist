# Include the headers
include_directories(
    include
    ${PROJECT_SOURCE_DIR}/FastSophiread/include
)

# Configure the commandline application
set(SRC_FILES
    src/user_config.cpp
    src/sophiread.cpp
)

# ----------------- CLI APPLICATION ----------------- #
add_executable(Sophiread
    ${SRC_FILES}
)
set_target_properties(Sophiread PROPERTIES VERSION ${PROJECT_VERSION})
target_link_libraries(
    Sophiread
    FastSophiread
    tbb
    spdlog::spdlog
    ${HDF5_LIBRARIES}
)

# ----------------- TESTS ----------------- #
# UserConfig tests
add_executable(
    UserConfigTest
    tests/test_user_config.cpp
    src/user_config.cpp
)
target_link_libraries(
    UserConfigTest
    FastSophiread
    spdlog::spdlog
    GTest::GTest
    GTest::Main
)
gtest_discover_tests(UserConfigTest)

# ----------------- SYMLINK ----------------- #
# symlink the executable to the build directory
add_custom_command(TARGET Sophiread POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink 
    ${PROJECT_BINARY_DIR}/SophireadCLI/Sophiread
    ${PROJECT_BINARY_DIR}/Sophiread
)

# ----------------- INSTALL ----------------- #
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "/usr/local" CACHE PATH "Default install prefix" FORCE)
endif()
install(TARGETS Sophiread
        RUNTIME DESTINATION bin)
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
include(CPack)
