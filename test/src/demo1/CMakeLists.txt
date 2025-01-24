add_executable(microtcp_client_demo1.out microtcp_client_demo1.c)
add_executable(microtcp_server_demo1.out microtcp_server_demo1.c)

target_include_directories(microtcp_client_demo1.out PUBLIC ${CMAKE_SOURCE_DIR}/lib/include)
target_include_directories(microtcp_client_demo1.out PUBLIC ${CMAKE_SOURCE_DIR}/test/include)
target_include_directories(microtcp_server_demo1.out PUBLIC ${CMAKE_SOURCE_DIR}/lib/include)
target_include_directories(microtcp_server_demo1.out PUBLIC ${CMAKE_SOURCE_DIR}/test/include)

target_link_libraries(microtcp_client_demo1.out microtcp)
target_link_libraries(microtcp_server_demo1.out microtcp)
