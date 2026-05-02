cmake_minimum_required(VERSION 3.15)
project(overlay LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

add_executable(overlay WIN32 main.cpp)

target_compile_definitions(overlay PRIVATE UNICODE _UNICODE)
target_link_libraries(overlay PRIVATE user32 gdi32 gdiplus shell32)
target_include_directories(overlay PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

set_target_properties(overlay PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
