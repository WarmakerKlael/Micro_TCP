add_library(microtcp_logger STATIC 
microtcp_logger.c 
microtcp_logger_options.c 
microtcp_fsm_logger.c)

target_include_directories(microtcp_logger PUBLIC ${CMAKE_SOURCE_DIR}/utils/include)
target_include_directories(microtcp_logger PUBLIC ${CMAKE_SOURCE_DIR}/lib/include)
