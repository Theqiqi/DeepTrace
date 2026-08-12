cmake_minimum_required(VERSION 3.24)

project(sandbox LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(sandbox_fp main.cpp winapi.cpp memory.cpp)
target_link_libraries(sandbox_fp PRIVATE)
