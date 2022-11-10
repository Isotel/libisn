add_executable(TestFrameLong isn_frame_long_test.c ../src/isn_frame_long.c ../src/isn_io.c ../src/posix/isn_clock.c)
target_include_directories(TestFrameLong PUBLIC .. ../include)

add_executable(TestFrameJumbo isn_frame_jumbo_test.c ../src/isn_frame_jumbo.c ../src/isn_io.c ../src/posix/isn_clock.c)
target_include_directories(TestFrameJumbo PUBLIC .. ../include)

add_test(NAME TestFrameLong COMMAND TestFrameLong)