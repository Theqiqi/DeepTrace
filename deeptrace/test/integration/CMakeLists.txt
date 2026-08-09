find_package(GTest CONFIG REQUIRED)

add_executable(deeptrace_integration_test
    target_util.h
    target_util.cpp
    process_integration_test.cpp
)

target_link_libraries(deeptrace_integration_test PRIVATE deeptrace GTest::gtest_main)
target_include_directories(deeptrace_integration_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
include(GoogleTest)
gtest_discover_tests(deeptrace_integration_test)
