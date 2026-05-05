set(BRIDGE_SOURCES
    src/main.cpp
    src/websocket_server.cpp
    src/websocket_session.cpp
    src/bridge_dispatcher.cpp
)

add_executable(mini_ipc_bridge ${BRIDGE_SOURCES})

target_include_directories(mini_ipc_bridge
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(mini_ipc_bridge
    PRIVATE mini_ipc_core 
    PRIVATE pthread    
)