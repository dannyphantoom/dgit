# dgit Makefile
# Simple build system for the dgit project

CXX = clang++
CXXFLAGS = -std=c++17 -Wall -Wextra -Wpedantic -Iinclude -Isrc
LDFLAGS = -lstdc++ -lz -lcurl -lssh

# Source files
CORE_SOURCES = src/core/sha1.cpp src/core/config.cpp src/core/index.cpp src/core/repository.cpp
OBJECT_SOURCES = src/objects/object.cpp src/objects/object_database.cpp
REF_SOURCES = src/refs/refs.cpp
NETWORK_SOURCES = src/network/network.cpp
PACK_SOURCES = src/packfile/packfile.cpp
MERGE_SOURCES = src/merge/merge.cpp
COMMAND_SOURCES = src/commands/commands.cpp src/commands/cli.cpp
MAIN_SOURCE = src/main.cpp

# Object files (in build directory)
CORE_OBJECTS = $(patsubst src/%.cpp, build/%.o, $(CORE_SOURCES))
OBJECT_OBJECTS = $(patsubst src/%.cpp, build/%.o, $(OBJECT_SOURCES))
REF_OBJECTS = $(patsubst src/%.cpp, build/%.o, $(REF_SOURCES))
COMMAND_OBJECTS = $(patsubst src/%.cpp, build/%.o, $(COMMAND_SOURCES))
MAIN_OBJECT = $(patsubst src/%.cpp, build/%.o, $(MAIN_SOURCE))

NETWORK_OBJECTS = $(patsubst src/%.cpp, build/%.o, $(NETWORK_SOURCES))
PACK_OBJECTS = $(patsubst src/%.cpp, build/%.o, $(PACK_SOURCES))
MERGE_OBJECTS = $(patsubst src/%.cpp, build/%.o, $(MERGE_SOURCES))

OBJECTS = $(CORE_OBJECTS) $(OBJECT_OBJECTS) $(REF_OBJECTS) $(NETWORK_OBJECTS) $(PACK_OBJECTS) $(MERGE_OBJECTS) $(COMMAND_OBJECTS) $(MAIN_OBJECT)
TARGET = build/bin/dgit

# Default target
all: $(TARGET)

# Create build directories
build:
	@mkdir -p build/bin build/lib build/core build/objects build/refs build/commands

# Compile source files
build/%.o: src/%.cpp | build
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Link executable
$(TARGET): $(OBJECTS) | build
	@mkdir -p $(dir $@)
	$(CXX) $(OBJECTS) $(LDFLAGS) -o $@

# Run the program
run: $(TARGET)
	./$(TARGET) --help

# Clean build artifacts
clean:
	rm -rf build/

# Clean all generated files
clean-all: clean
	rm -f src/*.o src/*/*.o

# Debug build
debug: CXXFLAGS += -g -O0 -DDEBUG
debug: clean all

# Release build
release: CXXFLAGS += -O3 -DNDEBUG
release: clean all

# Show help
help:
	@echo "dgit Build System"
	@echo ""
	@echo "Available targets:"
	@echo "  all       - Build the dgit executable (default)"
	@echo "  run       - Build and run dgit with --help"
	@echo "  clean     - Remove build directory"
	@echo "  clean-all - Remove all build artifacts"
	@echo "  debug     - Build with debug flags"
	@echo "  release   - Build with optimization flags"
	@echo "  help      - Show this help message"
	@echo ""
	@echo "Usage: make [target]"

.PHONY: all run clean clean-all debug release help build
