add_library(microtcp SHARED microtcp.c)
target_include_directories(microtcp PUBLIC ${CMAKE_SOURCE_DIR}/utils)
