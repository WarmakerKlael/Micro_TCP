add_library(microtcp_logger STATIC microtcp_logger.c microtcp_logger_options.c)

target_include_directories(microtcp_logger PUBLIC ${CMAKE_SOURCE_DIR}/utils/include)