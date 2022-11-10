project(isn DESCRIPTION "Isotropic Sensor Network Protocol Library")
cmake_minimum_required(VERSION 3.16)
include(CTest)

option (USE_POSIX "Add POSIX based device drivers" ON)
option (USE_TESTS "Build also unit tests" ON)

## Requires the use of different cross-compilers
#option (USE_PSoC "Add PSoC6, PSoC5 and PSoC5 device drivers", OFF)
#option (USE_TM4C "Add TM4C device drivers", OFF)

add_library(${PROJECT_NAME} SHARED)

if (USE_POSIX)
    add_subdirectory(src/posix)
endif (USE_POSIX)

if (USE_TESTS)
    add_subdirectory(tests)
endif (USE_TESTS)

if (MSVC)
    set(ISN_SOURCES ${ISN_SOURCES} ${ISN_SOURCES_DIR}/posix/getopt.c)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    add_compile_options(/wd4996 /wd4267 /wd4244)
endif ()

add_subdirectory(src)
target_include_directories(${PROJECT_NAME} PUBLIC . include)

#add_compile_options(-lkernel32 -luser32 -lwinmm -lws2_32)
if (WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32)
endif (WIN32)
