add_library(microtcp_fsm STATIC
        fsm_connect.c
        fsm_accept.c
        fsm_shutdown.c
        fsm_receive.c
)
target_link_libraries(microtcp_fsm microtcp_settings)

target_include_directories(microtcp_fsm PUBLIC ${CMAKE_SOURCE_DIR}/lib/include)
target_include_directories(microtcp_fsm PUBLIC ${CMAKE_SOURCE_DIR}/utils/include)
