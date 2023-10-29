cmake_minimum_required(VERSION 3.0)
project(atomic_bitset)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wpedantic")

include_directories(.)

add_executable(test_st
        atomic_bit_allocator.h
        test/test_st.cpp
        )
add_executable(test_mt
        atomic_bit_allocator.h
        test/test_mt.cpp
        )

enable_testing()

add_test( NAME test_st COMMAND $<TARGET_FILE:test_st>)
add_test( NAME test_mt COMMAND $<TARGET_FILE:test_mt>)
