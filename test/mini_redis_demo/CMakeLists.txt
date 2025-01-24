add_library(registry STATIC registry.c)
target_include_directories(registry PUBLIC ${CMAKE_SOURCE_DIR}/utils/include)

add_executable(mini_redis_server.out mini_redis_server.c)
target_include_directories(mini_redis_server.out PUBLIC ${CMAKE_SOURCE_DIR}/lib/include)

target_link_libraries(mini_redis_server.out microtcp)
target_link_libraries(mini_redis_server.out registry)
target_link_libraries(mini_redis_server.out microtcp_settings)
