add_subdirectory(logging)

add_library(microtcp_misc_utils STATIC microtcp_helper_functions.c microtcp_prompt_util.c)
target_include_directories(microtcp_misc_utils PUBLIC ${CMAKE_SOURCE_DIR}/utils/include)
target_include_directories(microtcp_misc_utils PUBLIC ${CMAKE_SOURCE_DIR}/lib/include)
