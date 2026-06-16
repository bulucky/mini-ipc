set(GTEST_SOURCES
    test_param_manager.cpp
)

add_executable(test_param_manager 
    ${GTEST_SOURCES}
    ${CMAKE_SOURCE_DIR}/core/src/param_manager.cpp
)

target_include_directories(test_param_manager
    PRIVATE ${CMAKE_SOURCE_DIR}/core/include
    PRIVATE ${CMAKE_SOURCE_DIR}/core/src
)

find_package(yaml-cpp REQUIRED)

target_link_libraries(test_param_manager
    PRIVATE yaml-cpp
    PRIVATE GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(test_param_manager)