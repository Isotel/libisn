set(LIB_ISN_SOURCES_DIR ../../c/src)
set(LIB_ISN_INCLUDE_DIR ../../c/include)
set(LIB_ISN_SOURCES
        ${LIB_ISN_SOURCES_DIR}/isn_dispatch.c
        ${LIB_ISN_SOURCES_DIR}/isn_msg.c
        ${LIB_ISN_SOURCES_DIR}/isn_frame.c
        ${LIB_ISN_SOURCES_DIR}/isn_redirect.c
        ${LIB_ISN_SOURCES_DIR}/posix/isn_udp.c
        ${LIB_ISN_SOURCES_DIR}/posix/isn_serial.c
)

if (MSVC)
    set(LIB_ISN_SOURCES ${LIB_ISN_SOURCES} ${LIB_ISN_SOURCES_DIR}/posix/getopt.c)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    add_compile_options(/wd4996 /wd4267 /wd4244)
endif ()

function(CONFIGURE_EXECUTABLE E_NAME)
    add_executable(${E_NAME} ${ARGN} ${LIB_ISN_SOURCES})
    target_include_directories(${E_NAME} PRIVATE ${LIB_ISN_INCLUDE_DIR})
    if (WIN32)
        target_link_libraries(${E_NAME} PRIVATE ws2_32)
    endif()
endfunction()

#
configure_executable(udp_example udp_example.c)
configure_executable(udp_host udp_host.c)
configure_executable(serial_host serial_host.c)
configure_executable(udp_serial_bridge udp_serial_bridge.c)
