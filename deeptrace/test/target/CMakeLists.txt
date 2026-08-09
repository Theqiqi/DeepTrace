# Test target program: provides known values at known addresses for
# integration/e2e tests. Does not link deeptrace. ASLR disabled.
add_executable(deeptrace_target main.cpp)

# Disable ASLR so module base and global addresses are deterministic.
target_link_options(deeptrace_target PRIVATE /DYNAMICBASE:NO /HIGHENTROPYVA:NO)

set_target_properties(deeptrace_target PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
