add_library(Logger STATIC logger.c logger_options.c)

target_include_directories(Logger PUBLIC ${CMAKE_SOURCE_DIR}/utils/include)