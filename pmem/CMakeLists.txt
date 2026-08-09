cmake_minimum_required(VERSION 3.24)

project(pmem VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- Keystone 汇编引擎(X86 后端,源码自建) ----
# 绕开 keystone 根 CMakeLists(避免 kstool/fuzz/install 等无关目标,也避免其默认
# 把 /MD 替换为 /MT 造成与 pmem Debug 的 /MDd 冲突),直接集成其 llvm 子目录。
# LLVM_TARGETS_TO_BUILD=X86 裁剪编译量。
# LLVM 构建要求 python(llvm-build 生成 LLVMBuild 数据),Windows 侧无 python,
# 使用 third_party/python 嵌入式 python(_pth 已配置可 import llvmbuild)。
set(LLVM_TARGETS_TO_BUILD X86 CACHE STRING "LLVM targets to build" FORCE)
set(PYTHON_EXECUTABLE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/python/python.exe" CACHE FILEPATH "Python for LLVM build" FORCE)
add_subdirectory(third_party/keystone/llvm ${CMAKE_CURRENT_BINARY_DIR}/third_party/keystone)

add_subdirectory(src)
add_subdirectory(test/target)
add_subdirectory(test/dll)
add_subdirectory(test/unit)
add_subdirectory(test/integration)
