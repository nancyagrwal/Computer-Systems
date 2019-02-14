cmake_minimum_required(VERSION 3.5)

enable_language(ASM)

add_executable(server switch.s server.c stack.c qthread.c)
add_executable(test1 switch.s test1.c stack.c qthread.c)
