set(APPLICATION_NAME REDIS)

add_library(registry STATIC registry.c)
target_include_directories(registry PUBLIC ${CMAKE_SOURCE_DIR}/utils/include)
target_include_directories(registry PUBLIC ${CMAKE_SOURCE_DIR}/test/include)

add_executable(miniredis_client.out miniredis_client.c)
add_executable(miniredis_server.out miniredis_server.c)
target_include_directories(miniredis_client.out PUBLIC ${CMAKE_SOURCE_DIR}/lib/include)
target_include_directories(miniredis_client.out PUBLIC ${CMAKE_SOURCE_DIR}/test/include)
target_include_directories(miniredis_server.out PUBLIC ${CMAKE_SOURCE_DIR}/lib/include)
target_include_directories(miniredis_server.out PUBLIC ${CMAKE_SOURCE_DIR}/test/include)

# Pass APPLICATION_NAME as a preprocessor definition
target_compile_definitions(miniredis_client.out PRIVATE APPLICATION_NAME="${APPLICATION_NAME}")
target_compile_definitions(miniredis_server.out PRIVATE APPLICATION_NAME="${APPLICATION_NAME}")


target_link_libraries(miniredis_client.out microtcp)
target_link_libraries(miniredis_client.out microtcp_settings)
target_link_libraries(miniredis_server.out microtcp)
target_link_libraries(miniredis_server.out registry)
target_link_libraries(miniredis_server.out microtcp_settings)
