SET(CORE_SOURCES
    src/node.cpp
)

add_library(mini_ipc_core SHARED ${CORE_SOURCES})

target_include_directories(mini_ipc_core
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_subdirectory(examples)