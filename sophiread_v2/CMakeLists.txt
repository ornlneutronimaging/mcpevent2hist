cmake_minimum_required(VERSION 3.10)
project(Sophiread)

# Add source files
set(SOURCES main.cpp src/tpx3.cpp)

# Add header files
set(HEADERS include/tpx3.h)

# Add include directories
include_directories(include)

# Add compiler flags
add_compile_options(-std=c++11 -Wall -Wextra)

# Add executable
add_executable(Sophiread ${SOURCES} ${HEADERS})
