cmake_minimum_required(VERSION 3.24)

project(deeptrace_cli VERSION 1.4.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(src)
add_subdirectory(test/target)
add_subdirectory(test/unit)
add_subdirectory(test/integration)
