add_executable(talker talker.cpp)
target_link_libraries(talker PRIVATE mini_ipc_core)

add_executable(listener listener.cpp)
target_link_libraries(listener PRIVATE mini_ipc_core)