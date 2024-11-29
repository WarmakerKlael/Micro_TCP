add_library(Logger STATIC logger.c)

target_include_directories(Logger PUBLIC ${CMAKE_SOURCE_DIR}/utils)