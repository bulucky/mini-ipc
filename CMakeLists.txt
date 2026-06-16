cmake_minimum_required(VERSION 3.22)
project(MiniIPC VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.17.x
)
FetchContent_MakeAvailable(googletest)
enable_testing()

add_subdirectory(core)
add_subdirectory(bridge)