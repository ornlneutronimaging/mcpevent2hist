# Include the headers
include_directories(
    include
    ${PROJECT_SOURCE_DIR}/FastSophiread/include
    ${TIFF_INCLUDE_DIRS}
)

# Configure the commandline application
set(SRC_FILES
    src/user_config.cpp
    src/json_config_parser.cpp
    src/sophiread_core.cpp
    src/sophiread.cpp
)

# ----------------- CLI APPLICATION ----------------- #
add_executable(Sophiread
    ${SRC_FILES}
)
set_target_properties(Sophiread PROPERTIES VERSION ${PROJECT_VERSION})
target_link_libraries(Sophiread
    PRIVATE 
      FastSophiread
      TBB::tbb
      spdlog::spdlog
      fmt::fmt
      nlohmann_json::nlohmann_json
      ${HDF5_LIBRARIES}
      ${TIFF_LIBRARIES}
)

# ----------------- TESTS ----------------- #
# UserConfig tests
add_executable(
    UserConfigTest
    tests/test_user_config.cpp
    src/user_config.cpp
)
target_link_libraries(UserConfigTest
    PRIVATE 
        FastSophiread
        spdlog::spdlog
        GTest::GTest
        GTest::Main
)
gtest_discover_tests(UserConfigTest)
# Json config test
add_executable(
    JsonConfigParserTest
    tests/test_json_config_parser.cpp
    src/json_config_parser.cpp
)
target_link_libraries(JsonConfigParserTest
    PRIVATE 
        FastSophiread
        spdlog::spdlog
        GTest::GTest
        GTest::Main
        nlohmann_json::nlohmann_json
)
gtest_discover_tests(JsonConfigParserTest)
# core test
# add_executable(
#     SophireadCoreTest
#     tests/test_sophiread_core.cpp
#     src/sophiread_core.cpp
#     src/json_config_parser.cpp
# )
# target_link_libraries(SophireadCoreTest
#     PRIVATE 
#         FastSophiread
#         spdlog::spdlog
#         GTest::GTest
#         GTest::Main
#         nlohmann_json::nlohmann_json
#         ${HDF5_LIBRARIES}
#         ${TIFF_LIBRARIES}
#         TBB::tbb
# )
# gtest_discover_tests(SophireadCoreTest)

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
