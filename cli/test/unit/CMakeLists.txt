find_package(GTest CONFIG REQUIRED)

add_executable(deeptrace_cli_unit_test
    parser_test.cpp
    printer_test.cpp
    executor_test.cpp
    script_test.cpp
    aa_script_test.cpp
    batch_test.cpp
)

target_include_directories(deeptrace_cli_unit_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(deeptrace_cli_unit_test PRIVATE
    deeptrace_cli_core
    GTest::gtest_main
)
include(GoogleTest)
gtest_discover_tests(deeptrace_cli_unit_test)
