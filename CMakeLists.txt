cmake_minimum_required(VERSION 3.20)
project(microTCP VERSION 0.2.0 LANGUAGES C)

get_filename_component(PROJECT_DIR_NAME ${CMAKE_SOURCE_DIR} NAME)

add_compile_options(-Wall -Wextra -Wno-unused-parameter) #-Wextra #Strive for pedantic
add_compile_definitions(PROJECT_TOP_LEVEL_DIRECTORY= "${PROJECT_DIR_NAME}")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_COMPILER clang)

option(DEBUG_MODE "Enables -g for debugging, and full logging." OFF)
option(ENABLE_IWYU "Enables Include-What-You-Use parser" OFF)
option(VERBOSE_MODE "Enables and informational logging." OFF)


if(ENABLE_IWYU)
    # Set the IWYU command to be used for analyzing include directives
    set(CMAKE_C_INCLUDE_WHAT_YOU_USE include-what-you-use --no-fwd-decls)
    message(STATUS "Include-What-You-Use enabled.")
else()
    message(STATUS "Include-What-You-Use disabled.")
endif()

# Add the mapping file
# set(IWYU_MAPPING_FILE ${CMAKE_SOURCE_DIR}/iwyu_mapping.imp)

# # Pass the mapping file to IWYU
# set(CMAKE_C_INCLUDE_WHAT_YOU_USE ${CMAKE_C_INCLUDE_WHAT_YOU_USE} -Xiwyu --mapping_file=${IWYU_MAPPING_FILE})

# Add defintions for debug and verbose mode.
if(DEBUG_MODE)
    add_compile_definitions(DEBUG_MODE)
    add_compile_options(-g)
    message("CMAKE: set project in DEBUG_MODE.")
endif()

if(VERBOSE_MODE)
    add_compile_definitions(VERBOSE_MODE)
    message("CMAKE: set project in VERBOSE_MODE.")
endif()
add_subdirectory(lib)
add_subdirectory(utils)
add_subdirectory(test)
