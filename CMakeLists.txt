cmake_minimum_required(VERSION 3.20)

project(SaturnOS C)

set(CMAKE_C_STANDARD 11)

add_executable(saturnos
    kernel/src/kernel.c
)

target_include_directories(saturnos PRIVATE
    kernel/include
)

message(STATUS "Building SaturnOS")