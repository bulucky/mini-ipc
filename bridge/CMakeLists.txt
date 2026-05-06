set(BRIDGE_SOURCES
    src/main.cpp
    src/websocket_server.cpp
    src/websocket_session.cpp
    src/bridge_dispatcher.cpp
)

include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.12.0
)
FetchContent_MakeAvailable(nlohmann_json)

add_executable(mini_ipc_bridge ${BRIDGE_SOURCES})

target_include_directories(mini_ipc_bridge
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(mini_ipc_bridge
    PRIVATE mini_ipc_core 
    PRIVATE pthread
    PRIVATE nlohmann_json::nlohmann_json    
)