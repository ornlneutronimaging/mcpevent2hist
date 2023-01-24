cmake_minimum_required(VERSION 3.20)
project(Sophiread)

set(CMAKE_CXX_STANDARD 14)

# copy test data directory
file(COPY ${CMAKE_SOURCE_DIR}/resources/data DESTINATION ${CMAKE_BINARY_DIR})

# ----------------- APPLICATION ----------------- #
# Add source files
set(SOURCES
    main.cpp
    src/tpx3.cpp
)

# Add header files
set(HEADERS
    include/tpx3.h
)

# Add include directories
include_directories(include)

# Add compiler flags
add_compile_options(-O3 -std=c++11 -Wall -Wextra)

# Add executable
add_executable(Sophiread ${SOURCES} ${HEADERS})


# ----------------- TESTS ----------------- #
enable_testing()
find_package(GTest REQUIRED)

# Add test files
set(TESTS
    include/tpx3.h
    src/tpx3.cpp
    tests/test_tpx3.cpp
)

# Add the executable for the test project
add_executable(SophireadTests ${TESTS})

# Link the test project with Google Test
target_link_libraries(SophireadTests GTest::GTest GTest::Main)

include(GoogleTest)
gtest_discover_tests(SophireadTests)
