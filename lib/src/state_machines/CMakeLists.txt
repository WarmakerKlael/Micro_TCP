add_library(state_machines STATIC connect_state_machine.c accept_state_machine.c)

target_include_directories(state_machines PUBLIC ${CMAKE_SOURCE_DIR}/lib/include)
target_include_directories(state_machines PUBLIC ${CMAKE_SOURCE_DIR}/utils/include)
