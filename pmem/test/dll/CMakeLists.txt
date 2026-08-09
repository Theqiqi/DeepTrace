# Companion DLL for dll-injection integration tests.
# Built next to the test executables so tests can resolve it by exe dir.
add_library(testdll SHARED testdll.cpp)

set_target_properties(testdll PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
