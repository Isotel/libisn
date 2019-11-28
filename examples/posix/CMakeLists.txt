set(LIB_ISN_SOURCES_DIR ../../c/src)
set(LIB_ISN_INCLUDE_DIR ../../c/include)
set(LIB_ISN_SOURCES
        ${LIB_ISN_SOURCES_DIR}/isn_dispatch.c
        ${LIB_ISN_SOURCES_DIR}/isn_msg.c
        ${LIB_ISN_SOURCES_DIR}/isn_frame.c
        ${LIB_ISN_SOURCES_DIR}/posix/isn_udp.c
        ${LIB_ISN_SOURCES_DIR}/posix/isn_serial.c
)

if (MSVC)
    set(LIB_ISN_SOURCES ${LIB_ISN_SOURCES} ${LIB_ISN_SOURCES_DIR}/posix/getopt.c)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    add_compile_options(/wd4996 /wd4267 /wd4244)
endif ()

add_executable(udp_example udp_example.c ${LIB_ISN_SOURCES})
target_include_directories(udp_example PRIVATE ${LIB_ISN_INCLUDE_DIR})
if (WIN32)
    target_link_libraries(udp_example PRIVATE ws2_32)
endif()
#
add_executable(udp_host udp_host.c ${LIB_ISN_SOURCES})
target_include_directories(udp_host PRIVATE ${LIB_ISN_INCLUDE_DIR})
if (WIN32)
    target_link_libraries(udp_host PRIVATE ws2_32)
endif()
#
add_executable(serial_host serial_host.c ${LIB_ISN_SOURCES})
target_include_directories(serial_host PRIVATE ${LIB_ISN_INCLUDE_DIR})
if (WIN32)
    target_link_libraries(serial_host PRIVATE ws2_32)
endif()
