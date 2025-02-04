add_subdirectory(fsm)
add_subdirectory(settings)
add_subdirectory(core)

add_library(microtcp STATIC microtcp.c)
set_source_files_properties(microtcp.c PROPERTIES COMPILE_FLAGS "-Wno-unused-parameter")

add_library(microtcp_misc_utils STATIC microtcp_helper_functions.c microtcp_prompt_util.c)
target_include_directories(microtcp_misc_utils PUBLIC ${CMAKE_SOURCE_DIR}/utils/include)
target_include_directories(microtcp_misc_utils PUBLIC ${CMAKE_SOURCE_DIR}/lib/include)

target_include_directories(microtcp PUBLIC ${CMAKE_SOURCE_DIR}/utils/include)

target_link_libraries(microtcp microtcp_logger)
target_link_libraries(microtcp microtcp_misc_utils)
target_link_libraries(microtcp microtcp_fsm)
target_link_libraries(microtcp microtcp_settings)

