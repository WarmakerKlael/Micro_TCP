add_library(microtcp STATIC microtcp.c microtcp_core_utils.c)
add_library(circle_buffer STATIC circle_buffer.c)

target_link_libraries(microtcp Logger)
target_link_libraries(microtcp microtcp_helpers)
target_link_libraries(microtcp circle_buffer)

target_link_libraries(circle_buffer Logger)

target_include_directories(microtcp PUBLIC ${CMAKE_SOURCE_DIR}/utils/include)

