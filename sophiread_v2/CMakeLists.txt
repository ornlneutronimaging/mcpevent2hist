cmake_minimum_required(VERSION 3.10)
project(Sophiread)

set(CMAKE_CXX_STANDARD 14)

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
add_compile_options(-std=c++11 -Wall -Wextra)

# Add executable
add_executable(Sophiread ${SOURCES} ${HEADERS})


# ----------------- TESTS ----------------- #
enable_testing()
find_package(GTest REQUIRED)

# Add test files
set(TESTS
    tests/test_tpx3.cpp
)

# Add the executable for the test project
add_executable(SophireadTests ${TESTS})

# Link the test project with Google Test
target_link_libraries(SophireadTests GTest::GTest GTest::Main)

# Add test
add_test(NAME SophireadTests COMMAND SophireadTests)

include(GoogleTest)
gtest_discover_tests(SophireadTests)
