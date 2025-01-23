cmake_minimum_required(VERSION 3.20)
project(microTCP VERSION 0.2.0 LANGUAGES C)

get_filename_component(PROJECT_DIR_NAME ${CMAKE_SOURCE_DIR} NAME)


# General warnings
add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
)

# Suppress specific warnings
add_compile_options(
    -Wno-gnu-statement-expression-from-macro-expansion
    -Wno-gnu-zero-variadic-macro-arguments
    -Wno-gnu-folding-constant
)

# Standard version
add_compile_options(-std=gnu11)

add_compile_definitions(
    PROJECT_TOP_LEVEL_DIRECTORY="${PROJECT_DIR_NAME}"
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_COMPILER clang)

option(DEBUG_MODE "Enables -g for debugging, and full logging." OFF)
option(ENABLE_IWYU "Enables Include-What-You-Use parser" OFF)
option(VERBOSE_MODE "Enables and informational logging." OFF)
option(UNORTHODOX_MODE "Enables unorthodox optimizations." OFF)


if(ENABLE_IWYU)
    # Set the IWYU command to be used for analyzing include directives
    set(CMAKE_C_INCLUDE_WHAT_YOU_USE include-what-you-use --no-fwd-decls)
    message(STATUS "CMAKE: Include-What-You-Use enabled.")
else()
    message(STATUS "CMAKE: Include-What-You-Use disabled.")
endif()

# Add the mapping file
# set(IWYU_MAPPING_FILE ${CMAKE_SOURCE_DIR}/iwyu_mapping.imp)

# # Pass the mapping file to IWYU
# set(CMAKE_C_INCLUDE_WHAT_YOU_USE ${CMAKE_C_INCLUDE_WHAT_YOU_USE} -Xiwyu --mapping_file=${IWYU_MAPPING_FILE})

# Add defintions for debug and verbose mode.
if(UNORTHODOX_MODE)
    add_compile_definitions(UNORTHODOX_MODE)
    message("CMAKE: UNORTHODOX_MODE enabled.")
endif()

if(DEBUG_MODE)
    add_compile_definitions(DEBUG_MODE)
    add_compile_options(-g)
    message("CMAKE: DEBUG_MODE enabled.")
endif()

if(VERBOSE_MODE)
    add_compile_definitions(VERBOSE_MODE)
    message("CMAKE: VERBOSE_MODE enabled.")
endif()

add_subdirectory(lib)
add_subdirectory(utils)
add_subdirectory(test)
