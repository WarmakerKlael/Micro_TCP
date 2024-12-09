add_library(microtcp STATIC microtcp.c microtcp_core.c)

target_link_libraries(microtcp Logger)
target_link_libraries(microtcp microtcp_helpers)
target_link_libraries(microtcp microtcp_fsm)

target_include_directories(microtcp PUBLIC ${CMAKE_SOURCE_DIR}/utils/include)

add_subdirectory(fsm)