cmake_minimum_required(VERSION 3.0)
project(atomic_bitset)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wpedantic")

include_directories(.)

add_executable(test_st
        atomic_bit_allocator.h
        test/test_st.cpp
        )

