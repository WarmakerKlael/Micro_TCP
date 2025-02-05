cmake_minimum_required(VERSION 3.18)
project(microTCP VERSION 0.2.0 LANGUAGES C)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Check if compiler `clang` is available: 
find_program(CLANG_PATH NAMES clang)
if(CLANG_PATH)
        set(CMAKE_C_COMPILER "${CLANG_PATH}")
        message(STATUS "Clang compiler found at: ${CLANG_PATH}")
else()
        message(FATAL_ERROR "Clang compiler not found! Please install `clang` compiler.")
endif()

# Write small C file for testing if GNU17 or GNU11 versions are supported:
set(GNU_CHECK_PROGRAM_NAME "check_gnu_stander.c")
file(WRITE "${CMAKE_BINARY_DIR}/${GNU_CHECK_PROGRAM_NAME}"
        "
        #if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 201112L
        #error C11 or newer required!
        #endif /* __STDC_VERSION__ */

        #if !defined(__GNUC__)
        #error GNU extensions required!
        #endif /* __GNUC__ */

        int main()
        {
                return -1;
        }
        "
)

set(FOUND_ACCEPTABLE_GNU_VERSION FALSE)
foreach(GNU_VERSION 17 11)
        try_compile(FOUND_ACCEPTABLE_GNU_VERSION
                SOURCES "${CMAKE_BINARY_DIR}/${GNU_CHECK_PROGRAM_NAME}"
                COMPILE_DEFINITIONS "-std=gnu${GNU_VERSION}"
        )

        if(FOUND_ACCEPTABLE_GNU_VERSION)
                message(STATUS "Found acceptable GNUC version: ${GNU_VERSION}.")
                set(CMAKE_C_STANDARD ${GNU_VERSION})
                set(CMAKE_C_EXTENSIONS ON) # Enable GNU extensions
                break()
        endif()
endforeach()

file(REMOVE "${CMAKE_BINARY_DIR}/${GNU_CHECK_PROGRAM_NAME}")

if(NOT FOUND_ACCEPTABLE_GNU_VERSION)
        message(FATAL_ERROR "Neither GNU C17 nor GNU C11 is supported by the compiler.")
endif()

# General warnings

# TODO: PUT this on try_compile too
add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Wno-unused-value
)

# Suppress specific warnings
add_compile_options(
        -Wno-gnu-statement-expression-from-macro-expansion
        -Wno-gnu-zero-variadic-macro-arguments
        -Wno-gnu-folding-constant
)

# Project name is used as mark/logo for logging.
get_filename_component(PROJECT_DIR_NAME ${CMAKE_SOURCE_DIR} NAME)
add_compile_definitions(
        PROJECT_TOP_LEVEL_DIRECTORY="${PROJECT_DIR_NAME}"
)

option(DEBUG_MODE "Enables -g for debugging, and full logging." OFF)
option(ENABLE_IWYU "Enables Include-What-You-Use parser" OFF)
option(VERBOSE_MODE "Enables and informational logging." OFF)
option(OPTIMIZED_MODE "Enables unorthodox optimizations." OFF)

if(ENABLE_IWYU)
        find_program(IWYU_PATH NAMES include-what-you-use)
        if(IWYU_PATH)
                set(CMAKE_C_INCLUDE_WHAT_YOU_USE include-what-you-use --no-fwd-decls)
                message(STATUS "CMAKE: include-what-you-use enabled.")
        else()
                message(SEND_ERROR "CMAKE: include-what-you-use not found on system.")
        endif()
else()
        message(STATUS "CMAKE: Include-What-You-Use disabled.")
endif()

if(DEBUG_MODE)
        add_compile_definitions(DEBUG_MODE)
        add_compile_options(-g)
        add_compile_options(-fsanitize=address,leak,undefined)
        add_link_options(-fsanitize=address,leak,undefined)
        message(STATUS "CMAKE: DEBUG_MODE enabled.")
endif()

if(VERBOSE_MODE)
        add_compile_definitions(VERBOSE_MODE)
        message(STATUS "CMAKE: VERBOSE_MODE enabled.")
endif()

if(OPTIMIZED_MODE)
        add_compile_definitions(OPTIMIZED_MODE)
        add_compile_options(-O3)
        message(STATUS "CMAKE: OPTIMIZED_MODE enabled.")
endif()

add_subdirectory(lib)
add_subdirectory(utils)
add_subdirectory(test)


# Add the mapping file
# set(IWYU_MAPPING_FILE ${CMAKE_SOURCE_DIR}/iwyu_mapping.imp)

# # Pass the mapping file to IWYU
# set(CMAKE_C_INCLUDE_WHAT_YOU_USE ${CMAKE_C_INCLUDE_WHAT_YOU_USE} -Xiwyu --mapping_file=${IWYU_MAPPING_FILE})

# Add defintions for debug and verbose mode.
