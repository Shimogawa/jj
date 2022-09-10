cmake_minimum_required(VERSION 3.22)
project(jj C)

set(CMAKE_C_STANDARD 11)

include_directories(.)
include_directories(hashmap.c)

add_executable(jj
        examples/main.c
        hashmap.c/hashmap.c
        hashmap.c/hashmap.h
        jj.c
        jj.h)
