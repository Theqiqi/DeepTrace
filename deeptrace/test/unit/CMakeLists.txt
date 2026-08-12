find_package(GTest CONFIG REQUIRED)

add_executable(deeptrace_unit_test
    hex_test.cpp
    scan_test.cpp
    pointer_scan_test.cpp
    threadpool_test.cpp
    disasm_test.cpp
    asm_test.cpp
    service_asm_test.cpp
    format_test.cpp
)

target_link_libraries(deeptrace_unit_test PRIVATE deeptrace GTest::gtest_main)
target_include_directories(deeptrace_unit_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
include(GoogleTest)
gtest_discover_tests(deeptrace_unit_test)
