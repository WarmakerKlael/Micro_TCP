#add_executable(bandwidth_test bandwidth_test.c)
#add_executable(traffic_generator_client traffic_generator_client.c)
#add_executable(traffic_generator traffic_generator.cpp)
add_executable(test_microtcp_server test_microtcp_server.c)
#add_executable(test_microtcp_client test_microtcp_client.c)

target_include_directories(test_microtcp_server PUBLIC ${CMAKE_SOURCE_DIR})
target_include_directories(test_microtcp_server PUBLIC ${CMAKE_SOURCE_DIR}/utils)


#target_link_libraries(bandwidth_test microtcp)
target_link_libraries(test_microtcp_server microtcp)
#target_link_libraries(test_microtcp_client microtcp)
#target_link_libraries(traffic_generator microtcp)
#target_link_libraries(traffic_generator_client microtcp)

#install(TARGETS bandwidth_test DESTINATION bin)