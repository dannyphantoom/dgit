# Main executable
add_executable(dgit main.cpp)

# Core components
target_sources(dgit PRIVATE
    core/sha1.cpp
    core/config.cpp
    core/index.cpp
    core/repository.cpp
)

# Object system
target_sources(dgit PRIVATE
    objects/object.cpp
    objects/object_database.cpp
)

# Reference system
target_sources(dgit PRIVATE
    refs/refs.cpp
)

# Commands
target_sources(dgit PRIVATE
    commands/commands.cpp
    commands/cli.cpp
)

# Link libraries
target_link_libraries(dgit
    Boost::filesystem
    Boost::system
    OpenSSL::SSL
    OpenSSL::Crypto
)

# Install target
install(TARGETS dgit DESTINATION bin)

# Development targets
add_custom_target(run
    COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/dgit --help
    DEPENDS dgit
    COMMENT "Running dgit with --help"
)

add_custom_target(debug
    COMMAND ${CMAKE_COMMAND} -DCMAKE_BUILD_TYPE=Debug ${CMAKE_SOURCE_DIR}
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target clean
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
    COMMENT "Building in debug mode"
)

add_custom_target(release
    COMMAND ${CMAKE_COMMAND} -DCMAKE_BUILD_TYPE=Release ${CMAKE_SOURCE_DIR}
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target clean
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
    COMMENT "Building in release mode"
)
