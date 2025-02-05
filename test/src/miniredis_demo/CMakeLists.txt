set(APPLICATION_NAME REDIS)

add_library(registry STATIC registry.c)
target_include_directories(registry PUBLIC ${CMAKE_SOURCE_DIR}/utils/include)
target_include_directories(registry PUBLIC ${CMAKE_SOURCE_DIR}/test/include)

add_library(miniredis_commands STATIC miniredis_commands.c)
target_include_directories(miniredis_commands PUBLIC ${CMAKE_SOURCE_DIR}/test/include)

add_executable(miniredis_client.out miniredis_client.c)
add_executable(miniredis_server.out miniredis_server.c)
target_include_directories(miniredis_client.out PUBLIC ${CMAKE_SOURCE_DIR}/lib/include)
target_include_directories(miniredis_client.out PUBLIC ${CMAKE_SOURCE_DIR}/test/include)
target_include_directories(miniredis_server.out PUBLIC ${CMAKE_SOURCE_DIR}/lib/include)
target_include_directories(miniredis_server.out PUBLIC ${CMAKE_SOURCE_DIR}/test/include)
target_compile_options(miniredis_client.out PRIVATE "-Wno-switch")
target_compile_options(miniredis_server.out PRIVATE "-Wno-switch")

# Pass APPLICATION_NAME as a preprocessor definition
target_compile_definitions(miniredis_client.out PRIVATE APPLICATION_NAME="${APPLICATION_NAME}")
target_compile_definitions(miniredis_server.out PRIVATE APPLICATION_NAME="${APPLICATION_NAME}")


target_link_libraries(miniredis_client.out microtcp)
target_link_libraries(miniredis_client.out microtcp_settings)
target_link_libraries(miniredis_client.out miniredis_commands)
target_link_libraries(miniredis_server.out microtcp)
target_link_libraries(miniredis_server.out microtcp_settings)
target_link_libraries(miniredis_server.out miniredis_commands)
target_link_libraries(miniredis_server.out registry)

find_library(PTHREAD_LIB_PATH NAMES pthread)
if(PTHREAD_LIB_PATH)
        message(STATUS "Pthread library found at: ${PTHREAD_LIB_PATH}")
        target_compile_options(miniredis_server.out PRIVATE "-pthread")
        target_link_libraries(miniredis_server.out pthread)
else()
        message(FATAL_ERROR "Pthread library NOT found. Necessary for miniredis_server.c.")
endif()
