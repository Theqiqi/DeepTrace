find_package(GTest CONFIG REQUIRED)

add_executable(pmem_unit_test
    hex_test.cpp
    scan_test.cpp
    disasm_test.cpp
    asm_test.cpp
    format_test.cpp
)

target_link_libraries(pmem_unit_test PRIVATE pmem GTest::gtest_main)
target_include_directories(pmem_unit_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
include(GoogleTest)
gtest_discover_tests(pmem_unit_test)
