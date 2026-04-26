SET(CORE_SOURCES
    src/node.cpp
    src/param_manager.cpp
)

add_library(mini_ipc_core SHARED ${CORE_SOURCES})

target_include_directories(mini_ipc_core
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

find_package(yaml-cpp REQUIRED)

target_link_libraries(mini_ipc_core
    PUBLIC yaml-cpp
)

add_subdirectory(examples)