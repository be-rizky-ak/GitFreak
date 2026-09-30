cmake_minimum_required(VERSION 3.21)

project(GitFreak
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Slint REQUIRED)

add_executable(GitFreak
    src/main.cpp
)

slint_target_sources(GitFreak
    ui/MainWindow.slint
)

target_link_libraries(GitFreak
    PRIVATE Slint::Slint
)

add_custom_command(TARGET GitFreak POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "C:/Program Files/Slint-cpp 1.18.1/lib/slint_cpp.dll"
        "$<TARGET_FILE_DIR:GitFreak>"
)