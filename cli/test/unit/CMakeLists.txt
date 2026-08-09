find_package(GTest CONFIG REQUIRED)

add_executable(pmem_cli_unit_test
    parser_test.cpp
    printer_test.cpp
    executor_test.cpp
)

target_include_directories(pmem_cli_unit_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(pmem_cli_unit_test PRIVATE
    pmem_cli_core
    GTest::gtest_main
)
include(GoogleTest)
gtest_discover_tests(pmem_cli_unit_test)
