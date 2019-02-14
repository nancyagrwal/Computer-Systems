cmake_minimum_required(VERSION 3.5)
project(team_1_hw3)

set(CMAKE_CXX_STANDARD 14)

include_directories(.)

add_library(hw3 SHARED
        homework.c
        libhw3.c
        misc.c)

add_library(misc STATIC misc.c)

target_compile_options(misc PRIVATE -Wall -DREAL_FS)
target_link_libraries(misc fuse)

add_executable(homework
        homework.c
        libhw3.c)

target_compile_options(homework PRIVATE -lfuse)
target_link_libraries(homework misc)


add_executable(mktest
        mktest.c)

add_executable(read-img
        read-img.c)

add_executable(mkfs-hw3
        mkfs-hw3.c)

add_custom_target(run
        COMMAND fusermount -u testdir
        DEPENDS homework
        )

