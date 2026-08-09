add_library(pmem STATIC
    domain/types.h

    algorithm/hex.h
    algorithm/hex.cpp
    algorithm/scan.h
    algorithm/scan.cpp
    algorithm/format.h
    algorithm/format.cpp

    infrastructure/process/process.h
    infrastructure/process/process.cpp
    infrastructure/memory/memory.h
    infrastructure/memory/memory.cpp
    infrastructure/module/module.h
    infrastructure/module/module.cpp
    infrastructure/thread/thread.h
    infrastructure/thread/thread.cpp
    infrastructure/debug/debug.h
    infrastructure/debug/debug.cpp
    infrastructure/inject/inject.h
    infrastructure/inject/inject.cpp
    infrastructure/disassembly/disasm.h
    infrastructure/disassembly/disasm.cpp
    infrastructure/assembly/asmenc.h
    infrastructure/assembly/asmenc.cpp

    service/session.h
    service/session.cpp
    service/store.h
    service/store.cpp
    service/process.h
    service/process.cpp
    service/memory.h
    service/memory.cpp
    service/module.h
    service/module.cpp
    service/thread.h
    service/thread.cpp
    service/debug.h
    service/debug.cpp
    service/disasm.h
    service/disasm.cpp
    service/resolve.h
    service/resolve.cpp
    service/watch.h
    service/watch.cpp
    service/inject.h
    service/inject.cpp
    service/asm.h
    service/asm.cpp
)

target_include_directories(pmem PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_include_directories(pmem PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/keystone/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/capstone/include
)

target_link_libraries(pmem PRIVATE keystone capstone_static)

target_compile_options(pmem PRIVATE /W4)
