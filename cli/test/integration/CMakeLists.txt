find_package(GTest CONFIG REQUIRED)

add_executable(deeptrace_cli_integration_test
    cli_integration_test.cpp
)

target_include_directories(deeptrace_cli_integration_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(deeptrace_cli_integration_test PRIVATE
    deeptrace_cli_core
    GTest::gtest_main
)

# Companion resource for dll-injection tests: copy testdll.dll (built by the
# deeptrace test tree) next to the CLI test exe so the test can resolve it by exe dir.
add_custom_command(TARGET deeptrace_cli_integration_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/../deeptrace/out/bin/${CMAKE_BUILD_TYPE}/testdll.dll"
        "$<TARGET_FILE_DIR:deeptrace_cli_integration_test>/testdll.dll"
    COMMENT "Copying testdll.dll for dll-injection tests"
)

include(GoogleTest)
gtest_discover_tests(deeptrace_cli_integration_test)
