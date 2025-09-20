# dgit - Git Implementation in C++

A complete Git implementation written in C++ from scratch, featuring all core Git functionality including object storage, branching, staging, and commit management.

## Features

✅ **Core Git Functionality**
- Complete Git object model (blobs, trees, commits, tags)
- SHA-1 hashing for object identification
- Repository initialization and management
- File staging and tracking (index)
- Commit creation with full metadata
- Branch management
- Reference tracking (HEAD, branches, tags)

✅ **Commands Implemented**
- `init` - Initialize a new Git repository
- `add` - Add files to the staging area
- `commit` - Record changes to the repository
- `status` - Show the working tree status
- `log` - Show commit logs
- `branch` - List, create, or delete branches
- `checkout` - Switch branches or restore working tree files

## Build System

This project supports multiple build systems:

### Option 1: CMake (Recommended)
```bash
mkdir build
cd build
cmake ..
make
```

### Option 2: Makefile
```bash
make                    # Build the project
make run               # Build and run with --help
make clean             # Clean build directory
make clean-all         # Clean all build artifacts
make debug             # Build with debug flags
make release           # Build with optimization
```

## Directory Structure

```
dgit/
├── build/                 # Build artifacts (generated)
│   ├── bin/              # Executables
│   └── lib/              # Libraries
├── include/dgit/         # Header files
├── src/                  # Source code
│   ├── core/            # Core functionality
│   ├── objects/         # Git object system
│   ├── refs/            # Reference management
│   └── commands/        # CLI commands
├── tests/               # Test suite
└── docs/                # Documentation
```

## Dependencies

- C++17 compatible compiler (clang++, g++)
- Boost libraries (filesystem, system)
- OpenSSL (for SHA-1 hashing)

## Usage

### Initialize a Repository
```bash
./build/bin/dgit init
```

### Add Files
```bash
./build/bin/dgit add filename.txt
./build/bin/dgit add .  # Add all files
```

### Create a Commit
```bash
./build/bin/dgit commit -m "Initial commit"
```

### Check Status
```bash
./build/bin/dgit status
```

### View History
```bash
./build/bin/dgit log
```

### Manage Branches
```bash
./build/bin/dgit branch              # List branches
./build/bin/dgit branch new-branch   # Create branch
./build/bin/dgit checkout branch-name # Switch branch
```

## Project Structure

### Core Components
- **Repository** - Main repository management
- **ObjectDatabase** - Git object storage and retrieval
- **Refs** - Reference management (branches, tags, HEAD)
- **Index** - Staging area for file changes
- **Config** - Git configuration management

### Git Objects
- **Blob** - File contents
- **Tree** - Directory structure
- **Commit** - Commit metadata and tree reference
- **Tag** - Annotated tags

## Development

### Adding New Commands
1. Add command class in `src/commands/`
2. Register command in `src/commands/cli.cpp`
3. Update `include/dgit/commands.hpp`

### Building Tests
```bash
# Configure with tests enabled
cd build
cmake .. -DBUILD_TESTS=ON
make

# Run all tests
make test

# Run tests with verbose output
make test-verbose

# Run tests in debug mode
make test-debug

# Alternative: Use the test script
cd ..
./test.sh
```

## Testing

The project includes a comprehensive test suite using Google Test framework:

### Test Structure
```
tests/
├── test_main.cpp          # Test entry point
├── test_sha1.cpp          # SHA-1 implementation tests
├── test_objects.cpp       # Object model tests
└── CMakeLists.txt         # Test configuration
```

### Test Categories

#### Unit Tests
- **SHA-1 Implementation**: Hash correctness, consistency, performance
- **Object Model**: Blob, Tree, Commit, Tag creation and serialization
- **Configuration System**: Config file parsing and value retrieval
- **Repository Operations**: Initialization, object storage, reference management

#### Integration Tests
- **Repository Lifecycle**: Creation, opening, configuration
- **File Operations**: Adding, staging, committing files
- **Branch Management**: Creating, switching, deleting branches
- **Network Operations**: Remote management, push/pull simulation

#### Performance Tests
- **Hashing Speed**: SHA-1 performance benchmarks
- **Object Creation**: Memory and speed tests for object instantiation
- **Large Data Handling**: Performance with large files and repositories

#### Memory Tests
- **Leak Detection**: Valgrind integration for memory leak detection
- **Resource Management**: Proper cleanup and resource handling

### Running Specific Tests

```bash
# Run all tests
make test

# Run specific test suite
./build/tests/dgit_tests

# Run specific test
./build/tests/dgit_tests --gtest_filter=SHA1Test.HashString

# Run tests with output
./build/tests/dgit_tests -v

# Run performance tests only
./build/tests/dgit_tests --gtest_filter=*Performance*
```

### Test Script

Use the comprehensive test script for automated testing:

```bash
./test.sh
```

This script will:
- Build the project with tests
- Run all unit tests
- Perform integration tests
- Execute performance benchmarks
- Generate a test report

### Writing Tests

To add new tests:

1. **Create test file** in `tests/` directory
2. **Include necessary headers**:
   ```cpp
   #include <gtest/gtest.h>
   #include "dgit/your_component.hpp"
   ```
3. **Write test cases** using Google Test macros:
   ```cpp
   TEST(YourComponentTest, TestName) {
       // Test code here
       EXPECT_EQ(result, expected);
   }
   ```
4. **Add to CMakeLists.txt** in tests directory
5. **Build and run** tests

### Continuous Integration

The test suite is designed to work with CI/CD systems:
- All tests should pass in under 5 minutes
- Memory tests use Valgrind for leak detection
- Performance tests ensure acceptable speed
- Integration tests verify end-to-end functionality

## Architecture

The implementation follows Git's core architecture:
- Objects are identified by SHA-1 hashes
- References point to specific commits
- The index tracks staged changes
- Configuration is stored in `.git/config`
- Objects are stored in `.git/objects/`

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests for new functionality
5. Submit a pull request

## License

This project is open source. Feel free to use, modify, and distribute.

## Future Enhancements

- Network protocols (push/pull)
- Merge functionality
- Remote repository management
- Packfile support
- Garbage collection
- Interactive rebasing
