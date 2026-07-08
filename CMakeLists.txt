cmake_minimum_required(VERSION 3.20)

project(SaturnOS C)

set(CMAKE_C_STANDARD 11)

add_library(kernel STATIC
    kernel/kernel.c
)

target_include_directories(kernel PUBLIC
    kernel
)

message(STATUS "Building SaturnOS")