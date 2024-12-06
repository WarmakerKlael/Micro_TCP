cmake_minimum_required(VERSION 3.20)
project(microTCP VERSION 0.1.0 LANGUAGES C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(DEBUG_MODE)
        add_compile_definitions(DEBUG_MODE)
endif()
add_subdirectory(lib)
add_subdirectory(utils)
add_subdirectory(test)
