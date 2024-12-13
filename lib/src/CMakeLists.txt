add_library(microtcp STATIC
        microtcp.c
        microtcp_core.c)

add_library(microtcp_settings STATIC
        microtcp_settings.c
        #microtcp_settings_prompts
)

target_link_libraries(microtcp microtcp_logger)
target_link_libraries(microtcp microtcp_helpers)
target_link_libraries(microtcp microtcp_fsm)
target_link_libraries(microtcp microtcp_settings)

target_include_directories(microtcp PUBLIC ${CMAKE_SOURCE_DIR}/utils/include)

target_include_directories(microtcp_settings PUBLIC ${CMAKE_SOURCE_DIR}/utils/include)
target_include_directories(microtcp_settings PUBLIC ${CMAKE_SOURCE_DIR}/lib/include)

add_subdirectory(fsm)
