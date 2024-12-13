# README for microTCP Project

## Overview
<font size="2"> microTCP is a lightweight TCP implementation written in C. The project is modularly organized with libraries, utilities, and tests.</font>

## Requirements
- CMake `version 3.20` or higher
- A C `POSIX` compliant compiler (e.g., `GCC` or `Clang`)

## Compilation Instructions
1. Navigate to the project root directory.
2. Create a build directory:
   ```bash
   $ mkdir build
   $ cd build
   ```
3. Run CMake to configure the project:
   ```bash
   $ cmake ..
   ```
   Use the following options to enable specific modes:
   - `DEBUG_MODE`: Enable debug-specific features.
   - `VERBOSE_MODE`: Enable verbose logging.

   For example:
   ```bash
   $ cmake -DDEBUG_MODE=ON -DVERBOSE_MODE=ON ..
   ```
4. Build the project:
   ```bash
   $ make
   ```

## Running Tests
After building, the test executables will be available in the build directory. Run the tests to verify functionality:
```bash
$ ./test/test_executable_name
```

## Notes
- If any issues arise, verify that the CMake version meets the minimum requirement.
- Tests and bandwidth testing tools included are for demonstration purposes only, as the project is still in Phase 1.
- Enabling `DEBUG_MODE` activates complete logging, including detailed memory and system-level logs.
- `VERBOSE_MODE` focuses mainly on microTCP-specific logs, excluding memory logging or other lower-level details.
