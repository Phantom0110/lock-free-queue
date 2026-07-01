cmake_minimum_required(VERSION 3.16)
project(LockFreeQueue LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_RELEASE "-O2")

include_directories(include)

find_package(Threads REQUIRED)

add_executable(tests src/main.cpp)
target_compile_options(tests PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(tests PRIVATE Threads::Threads)

add_executable(benchmark src/benchmark.cpp)
target_compile_options(benchmark PRIVATE -Wall -Wextra)
target_link_libraries(benchmark PRIVATE Threads::Threads)
