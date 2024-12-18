add_library(microtcp STATIC
        microtcp.c
        microtcp_core.c)

target_include_directories(microtcp PUBLIC ${CMAKE_SOURCE_DIR}/utils/include)

target_link_libraries(microtcp microtcp_logger)
target_link_libraries(microtcp microtcp_helpers)
target_link_libraries(microtcp microtcp_fsm)
target_link_libraries(microtcp microtcp_settings)

add_subdirectory(fsm)
add_subdirectory(settings)
