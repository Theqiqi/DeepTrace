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

# Companion fixtures for debug-run tests: copy the JSON script samples next to
# the test exe so tests can load them and substitute the runtime %G_INT% address.
add_custom_command(TARGET deeptrace_cli_integration_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/../scripts/debug_session.json"
        "$<TARGET_FILE_DIR:deeptrace_cli_integration_test>/debug_session.json"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/../scripts/debug_write.json"
        "$<TARGET_FILE_DIR:deeptrace_cli_integration_test>/debug_write.json"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/../scripts/debug_bad.json"
        "$<TARGET_FILE_DIR:deeptrace_cli_integration_test>/debug_bad.json"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/../scripts/debug_break_only.json"
        "$<TARGET_FILE_DIR:deeptrace_cli_integration_test>/debug_break_only.json"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/../scripts/script_call.aa"
        "$<TARGET_FILE_DIR:deeptrace_cli_integration_test>/script_call.aa"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/../scripts/script_hook.aa"
        "$<TARGET_FILE_DIR:deeptrace_cli_integration_test>/script_hook.aa"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/../scripts/script_bad.aa"
        "$<TARGET_FILE_DIR:deeptrace_cli_integration_test>/script_bad.aa"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/../scripts/script_badasm.aa"
        "$<TARGET_FILE_DIR:deeptrace_cli_integration_test>/script_badasm.aa"
    COMMENT "Copying debug-run + AA script fixtures"
)

include(GoogleTest)
gtest_discover_tests(deeptrace_cli_integration_test)
