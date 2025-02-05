# README for microTCP Project

## Overview
<font size="3"> MicroTCP is a lightweight TCP implementation written in C. The project is modularly organized with libraries, utilities, and demo (Mini-REDIS fileserver) in order to test it.</font>

## Requirements
- `CMAKE` version 3.18 or higher
- `Clang` compiler (GCC is not suggested)
- A C `POSIX` compliance

## Compilation Instructions
1. Navigate to the project root directory.
2. Create a build directory:
   ```bash
   $ mkdir build
   $ cd build
   ```
3. To configure the project:
   ```bash
   $ cmake ..
   ```
   Use the following options to enable specific modes:
   - `DEBUG_MODE`: Enable debug-specific features; Extensive logging, sanitizers, etc.
   - `VERBOSE_MODE`: Enables verbose logging, focuses mainly on microTCP-specific logs, excluding memory logging or other lower-level details (not recommended for benchmarking).
   - `OPTIMIZED_MODE`: Enables optimizations that break the initial constraints of the project. (Recommended for benchmarking)

   For example:
   ```bash
   $ cmake -DDEBUG_MODE=ON -DVERBOSE_MODE=ON ..
   ```
   or
   ```bash
   $ cmake -DOPTIMIZED_MODE=ON ..
   ```
4. To build the project:
   ```bash
   $ make
   ```

## Running Mini-REDIS demo (Demo for MicroTCP)
After building, the project, you can navigate to:

```bash
$ build/executables
```
In the directory mention above you will find two programs; The server, and the client-side of my Mini-REDIS application. 

## Notes
- If any issues arise, verify that the CMake version meets the minimum requirement.
- Enabling `DEBUG_MODE` activates complete logging, including detailed memory and system-level logs, also it activates `sanitizers`. In case you manually change my cmake, make sure that you do not use sanitizers that clash with each other (ex. -fsanitize=memory,address)
- `VERBOSE_MODE`
