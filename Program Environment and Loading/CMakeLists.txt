cmake_minimum_required(VERSION 3.12)
project(systems_hw1 C)

set(CMAKE_C_STANDARD 11)
set(GCC_COMPILE_FLAGS "-m32 -static -g -Wl,--section-start -Wl,.text=0x100800 -Wl,--section-start -Wl,.note.gnu.build-id=0x200000 -Wl,--section-start -Wl,.note.ABI-tag=0x300000")

set(CMAKE_C_FLAGS "${GCC_COMPILE_FLAGS}")

add_executable(systems_hw1 loader.c)