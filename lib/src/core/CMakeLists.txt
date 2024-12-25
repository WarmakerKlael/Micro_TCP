add_library(microtcp_core STATIC 
                resource_allocation.c 
                control_segments_io.c
                segment_processing.c
                socket_stats_updater.c
                misc.c)

target_include_directories(microtcp_core PUBLIC ${CMAKE_SOURCE_DIR}/lib/include)
target_include_directories(microtcp_core PUBLIC ${CMAKE_SOURCE_DIR}/utils/include)