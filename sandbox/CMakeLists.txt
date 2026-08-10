cmake_minimum_required(VERSION 3.24)

project(sandbox LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 自建 capstone 5.0.9(third_party/capstone,来自 vcpkg 下载的源码)。
# 原因:vcpkg 的 capstone port 在此环境默认把全部架构禁用(CAPSTONE_ARCHITECTURE_DEFAULT=OFF
# 且无任何架构开启),cs_open 对所有架构返回 CS_ERR_ARCH,见 结论.md。
# 这里仅启用 x86 后端以加快构建;capstone 的 CMake 默认全架构开启(不需 vcpkg)。
set(CAPSTONE_ARCHITECTURE_DEFAULT OFF CACHE BOOL "" FORCE)
set(CAPSTONE_X86_SUPPORT ON CACHE BOOL "" FORCE)
set(CAPSTONE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CAPSTONE_BUILD_CSTOOL OFF CACHE BOOL "" FORCE)
set(CAPSTONE_BUILD_CSTEST OFF CACHE BOOL "" FORCE)
set(CAPSTONE_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/capstone capstone_build EXCLUDE_FROM_ALL)

add_executable(sandbox_disasm main.cpp)
target_link_libraries(sandbox_disasm PRIVATE capstone)
target_include_directories(sandbox_disasm PRIVATE third_party/capstone/include)
