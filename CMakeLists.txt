cmake_minimum_required(VERSION 3.4 FATAL_ERROR)
project(stg VERSION 1.0 LANGUAGES CXX)
include(build/tdlib/lib/cmake/Td/TdStaticTargets.cmake)
add_executable(stg src/main.cpp src/ted.cpp src/chat.cpp src/common.cpp src/tgclient.cpp)
target_include_directories(stg PRIVATE td_include)

add_library(raylib STATIC IMPORTED)
set_target_properties(raylib PROPERTIES
    IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/build/libraylib.a"
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/raylib/src"
)

target_link_libraries(stg PRIVATE Td::TdStatic raylib m)
