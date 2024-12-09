add_subdirectory(logging)

add_library(microtcp_helpers STATIC microtcp_helper_functions.c)
target_include_directories(microtcp_helpers PUBLIC ${CMAKE_SOURCE_DIR}/utils/include)
target_include_directories(microtcp_helpers PUBLIC ${CMAKE_SOURCE_DIR}/lib/include)
