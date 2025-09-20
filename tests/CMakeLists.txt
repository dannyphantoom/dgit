# Find Google Test
find_package(GTest REQUIRED)

# Include test headers
include_directories(${GTEST_INCLUDE_DIRS})

# Test executable
add_executable(dgit_tests
    test_main.cpp
    test_sha1.cpp
    test_objects.cpp
)

# Link libraries
target_link_libraries(dgit_tests
    ${GTEST_LIBRARIES}
    ${GTEST_MAIN_LIBRARIES}
    pthread
)

# Test discovery
include(GoogleTest)
gtest_discover_tests(dgit_tests)

# Custom targets for testing
add_custom_target(test
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS dgit_tests
    COMMENT "Running all tests"
)

add_custom_target(test-verbose
    COMMAND ${CMAKE_CTEST_COMMAND} -V
    DEPENDS dgit_tests
    COMMENT "Running all tests with verbose output"
)

add_custom_target(test-debug
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -C Debug
    DEPENDS dgit_tests
    COMMENT "Running tests in debug mode"
)
