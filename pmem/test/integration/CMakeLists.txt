find_package(GTest CONFIG REQUIRED)

add_executable(pmem_integration_test
    target_util.h
    target_util.cpp
    process_integration_test.cpp
)

target_link_libraries(pmem_integration_test PRIVATE pmem GTest::gtest_main)
target_include_directories(pmem_integration_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
include(GoogleTest)
gtest_discover_tests(pmem_integration_test)
