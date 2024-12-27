add_library(microtcp_core STATIC
        misc.c
        resource_allocation.c
        segment_io_control.c
        segment_processing.c
        socket_stats_updater.c
        receiver_thread.c
)

target_include_directories(microtcp_core PUBLIC ${CMAKE_SOURCE_DIR}/lib/include)
target_include_directories(microtcp_core PUBLIC ${CMAKE_SOURCE_DIR}/utils/include)
