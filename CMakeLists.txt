cmake_minimum_required(VERSION 3.12)
project(libisn VERSION 0.0.1 LANGUAGES C)

set(CMAKE_C_STANDARD 11)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Wall)
endif()

add_subdirectory(examples)