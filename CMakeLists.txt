# Specifies the minimum CMake version
cmake_minimum_required(VERSION 3.20)

# Add the library target from its source files
add_library(hello_lib src/hello_lib.c)

# Make the "include" directory available to any application that links this library
target_include_directories(hello_lib PUBLIC include)