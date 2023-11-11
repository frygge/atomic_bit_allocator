cmake_minimum_required(VERSION 3.12)

include(CTest)

add_executable(test_st)
target_sources(test_st PRIVATE
        test_st.cpp)
target_include_directories(test_st PRIVATE
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>)
target_compile_features(test_st PRIVATE cxx_std_17)
target_compile_options(test_st PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/std:c++17>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
        $<$<CXX_COMPILER_ID:MSVC>:/WX>
        $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-Wall>
        $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-Werror>
        $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-pedantic>)


add_executable(test_mt)
target_sources(test_mt PRIVATE
        test_mt.cpp)
target_include_directories(test_mt
        PRIVATE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>)
target_compile_features(test_mt PRIVATE cxx_std_17)
target_compile_options(test_mt PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/std:c++17>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
        $<$<CXX_COMPILER_ID:MSVC>:/WX>
        $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-Wall>
        $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-Werror>
        $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-pedantic>)


add_executable(measure_performance)
target_sources(measure_performance PRIVATE
        measure_performance.cpp)
target_include_directories(measure_performance
        PRIVATE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>)
target_compile_features(measure_performance PRIVATE cxx_std_17)
target_compile_options(measure_performance PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/std:c++17>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
        $<$<CXX_COMPILER_ID:MSVC>:/WX>
        $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-Wall>
        $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-Werror>
        $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-Wno-terminate>
        $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-Wno-error=terminate>
        $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-pedantic>)


add_test( NAME test_st COMMAND $<TARGET_FILE:test_st>)
add_test( NAME test_mt COMMAND $<TARGET_FILE:test_mt>)
