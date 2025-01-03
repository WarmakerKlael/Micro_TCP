add_library(microtcp_fsm STATIC
        fsm_common.c
        fsm_connect.c
        fsm_accept.c
        # fsm_receive.c
        fsm_send.c
        fsm_shutdown_active.c
        fsm_shutdown_passive.c
)
target_link_libraries(microtcp_fsm microtcp_settings)
target_link_libraries(microtcp_fsm microtcp_logger)
target_link_libraries(microtcp_fsm microtcp_core)


target_include_directories(microtcp_fsm PUBLIC ${CMAKE_SOURCE_DIR}/lib/include)
target_include_directories(microtcp_fsm PUBLIC ${CMAKE_SOURCE_DIR}/utils/include)
