# Production core: all layers except main(). Also linked by tests.
add_library(pmem_cli_core STATIC
    command/request.h
    command/commands.h
    command/commands.cpp
    command/parser.h
    command/parser.cpp

    interface/executor.h
    interface/executor.cpp
    interface/cmd.h
    interface/cmd_process.cpp
    interface/cmd_memory.cpp
    interface/cmd_module.cpp
    interface/cmd_thread.cpp
    interface/cmd_debug.cpp
    interface/cmd_disasm.cpp
    interface/cmd_resolve.cpp
    interface/cmd_watch.cpp
    interface/cmd_inject.cpp
    interface/cmd_asm.cpp
    interface/cmd_shellcode.cpp

    printing/printer.h
    printing/printer.cpp
)

target_include_directories(pmem_cli_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# pmem static library: public headers directly from ../pmem/include,
# library found from ../pmem/out/lib/<config>/ (no install/ middle layer).
target_include_directories(pmem_cli_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../pmem/include)
find_library(PMEM_LIB
    NAMES pmem
    PATHS ${CMAKE_CURRENT_SOURCE_DIR}/../../pmem/out/lib/${CMAKE_BUILD_TYPE}
    NO_DEFAULT_PATH
    REQUIRED
)
# pmem 的汇编能力依赖 Keystone(源码自建,链接进 pmem 的最终目标)。
# 静态库不合并依赖,消费方需显式链接 keystone.lib。
# 路径用预设的 binaryDir 命名(out/build/<配置小写>),不依赖 CMAKE_BUILD_TYPE 的大小写。
string(TOLOWER "${CMAKE_BUILD_TYPE}" PMEM_BUILD_CONFIG)
find_library(KEYSTONE_LIB
    NAMES keystone
    PATHS ${CMAKE_CURRENT_SOURCE_DIR}/../../pmem/out/build/${PMEM_BUILD_CONFIG}/third_party/keystone/lib
    NO_DEFAULT_PATH
    REQUIRED
)
target_link_libraries(pmem_cli_core PUBLIC ${PMEM_LIB} ${KEYSTONE_LIB})

add_executable(pmem_cli main.cpp)
target_link_libraries(pmem_cli PRIVATE pmem_cli_core)
