
add_library(microtcp_settings STATIC
        microtcp_settings.c
        microtcp_settings_prompts.c
)

target_include_directories(microtcp_settings PUBLIC ${CMAKE_SOURCE_DIR}/utils/include)
target_include_directories(microtcp_settings PUBLIC ${CMAKE_SOURCE_DIR}/lib/include)
