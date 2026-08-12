# Production core: all layers except main(). Also linked by tests.
add_library(deeptrace_cli_core STATIC
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
    interface/cmd_debug_run.cpp
    interface/script.h
    interface/script.cpp
    interface/json.h
    interface/json.cpp
    interface/batch.h
    interface/batch.cpp
    interface/ptrscan.h
    interface/ptrscan.cpp
    interface/cmd_disasm.cpp
    interface/cmd_resolve.cpp
    interface/cmd_watch.cpp
    interface/cmd_inject.cpp
    interface/cmd_asm.cpp
    interface/cmd_shellcode.cpp
    interface/cmd_convert.cpp
    interface/cmd_hex2bin.cpp
    interface/cmd_script.cpp

    printing/printer.h
    printing/printer.cpp
)

target_include_directories(deeptrace_cli_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# deeptrace static library: public headers directly from ../deeptrace/include,
# library found from ../deeptrace/out/lib/<config>/ (no install/ middle layer).
target_include_directories(deeptrace_cli_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../deeptrace/include)
find_library(DEEPTRACE_LIB
    NAMES deeptrace
    PATHS ${CMAKE_CURRENT_SOURCE_DIR}/../../deeptrace/out/lib/${CMAKE_BUILD_TYPE}
    NO_DEFAULT_PATH
    REQUIRED
)
# deeptrace 的汇编能力依赖 Keystone(源码自建,链接进 deeptrace 的最终目标)。
# 静态库不合并依赖,消费方需显式链接 keystone.lib。
# 路径用预设的 binaryDir 命名(out/build/<配置小写>),不依赖 CMAKE_BUILD_TYPE 的大小写。
string(TOLOWER "${CMAKE_BUILD_TYPE}" DEEPTRACE_BUILD_CONFIG)
find_library(KEYSTONE_LIB
    NAMES keystone
    PATHS ${CMAKE_CURRENT_SOURCE_DIR}/../../deeptrace/out/build/${DEEPTRACE_BUILD_CONFIG}/third_party/keystone/lib
    NO_DEFAULT_PATH
    REQUIRED
)
# deeptrace 的反汇编能力依赖 Capstone(源码自建,链接进 deeptrace 的最终目标)。
# 与 keystone 同理:静态库不合并依赖,消费方需显式链接 capstone.lib。
# capstone.lib 输出到 deeptrace/out/lib/<配置>(deeptrace 预设的归档输出目录)。
find_library(CAPSTONE_LIB
    NAMES capstone
    PATHS ${CMAKE_CURRENT_SOURCE_DIR}/../../deeptrace/out/lib/${CMAKE_BUILD_TYPE}
    NO_DEFAULT_PATH
    REQUIRED
)
target_link_libraries(deeptrace_cli_core PUBLIC ${DEEPTRACE_LIB} ${KEYSTONE_LIB} ${CAPSTONE_LIB})

add_executable(deeptrace_cli main.cpp)
target_link_libraries(deeptrace_cli PRIVATE deeptrace_cli_core)
