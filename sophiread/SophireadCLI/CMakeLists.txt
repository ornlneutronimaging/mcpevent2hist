# Include the headers
include_directories(include ${PROJECT_SOURCE_DIR}/FastSophiread/include
                    ${TIFF_INCLUDE_DIRS})

# Configure the commandline application
set(SRC_FILES src/user_config.cpp src/json_config_parser.cpp
              src/sophiread_core.cpp)

# ----------------- CLI APPLICATION ----------------- #
add_executable(Sophiread ${SRC_FILES} src/sophiread.cpp)
set_target_properties(Sophiread PROPERTIES VERSION ${PROJECT_VERSION})
target_link_libraries(
  Sophiread
  PRIVATE FastSophiread
          TBB::tbb
          spdlog::spdlog
          fmt::fmt
          nlohmann_json::nlohmann_json
          ${HDF5_LIBRARIES}
          ${TIFF_LIBRARIES})
#
add_executable(venus_auto_reducer ${SRC_FILES} src/venus_auto_reducer.cpp)
target_link_libraries(
  venus_auto_reducer
  PRIVATE FastSophiread
          TBB::tbb
          spdlog::spdlog
          fmt::fmt
          nlohmann_json::nlohmann_json
          ${HDF5_LIBRARIES}
          ${TIFF_LIBRARIES})

# ----------------- GDC EXTRACTOR APP ----------------- #
add_executable(SophireadGDCExtractor src/main_gdc_extractor.cpp
                                     src/gdc_extractor.cpp)
target_link_libraries(
  SophireadGDCExtractor PRIVATE FastSophiread pthread TBB::tbb spdlog::spdlog
                                ${HDF5_LIBRARIES})

# ----------------- TESTS ----------------- # UserConfig tests
add_executable(UserConfigTest tests/test_user_config.cpp src/user_config.cpp)
target_link_libraries(UserConfigTest PRIVATE FastSophiread spdlog::spdlog
                                             GTest::GTest GTest::Main)
gtest_discover_tests(UserConfigTest)
# Json config test
add_executable(JsonConfigParserTest tests/test_json_config_parser.cpp
                                    src/json_config_parser.cpp)
target_link_libraries(
  JsonConfigParserTest PRIVATE FastSophiread spdlog::spdlog GTest::GTest
                               GTest::Main nlohmann_json::nlohmann_json)
gtest_discover_tests(JsonConfigParserTest)
# core test
add_executable(
  SophireadCoreTest tests/test_sophiread_core.cpp src/sophiread_core.cpp
                    src/json_config_parser.cpp)
target_link_libraries(
  SophireadCoreTest
  PRIVATE FastSophiread
          spdlog::spdlog
          GTest::GTest
          GTest::Main
          nlohmann_json::nlohmann_json
          ${HDF5_LIBRARIES}
          ${TIFF_LIBRARIES}
          TBB::tbb)
gtest_discover_tests(SophireadCoreTest)

# ----------------- SYMLINK ----------------- # symlink the executable to the
# build directory
add_custom_command(
  TARGET Sophiread
  POST_BUILD
  COMMAND
    ${CMAKE_COMMAND} -E create_symlink
    ${PROJECT_BINARY_DIR}/SophireadCLI/Sophiread
    ${PROJECT_BINARY_DIR}/Sophiread)
add_custom_command(
  TARGET venus_auto_reducer
  POST_BUILD
  COMMAND
    ${CMAKE_COMMAND} -E create_symlink
    ${PROJECT_BINARY_DIR}/SophireadCLI/venus_auto_reducer
    ${PROJECT_BINARY_DIR}/venus_auto_reducer)
add_custom_command(
  TARGET SophireadGDCExtractor
  POST_BUILD
  COMMAND
    ${CMAKE_COMMAND} -E create_symlink
    ${PROJECT_BINARY_DIR}/SophireadCLI/SophireadGDCExtractor
    ${PROJECT_BINARY_DIR}/SophireadGDCExtractor)

# ----------------- INSTALL ----------------- #
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX
      "/usr/local"
      CACHE PATH "Default install prefix" FORCE)
endif()

# Install executables
install(
  TARGETS Sophiread venus_auto_reducer
  EXPORT sophireadTargets
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# Install the headers
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Install GDC Extractor
install(TARGETS SophireadGDCExtractor
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# Set up packaging (optional)
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
include(CPack)
