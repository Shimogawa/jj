cmake_minimum_required(VERSION 3.20)
project(jj C)

set(CMAKE_C_STANDARD 11)

include_directories(..)
include_directories(../hashmap.c)

add_executable(jj
        main.c
        ../hashmap.c/hashmap.c
        ../hashmap.c/hashmap.h
        ../jj.c
        ../jj.h)
