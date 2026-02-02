cmake_minimum_required(VERSION 3.22)
project(stdexec-playground LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Fetch stdexec from NVIDIA
include(FetchContent)
FetchContent_Declare(
    stdexec
    GIT_REPOSITORY https://github.com/NVIDIA/stdexec.git
    GIT_TAG main
)
FetchContent_MakeAvailable(stdexec)

# Helper function to add examples
function(add_example name)
    add_executable(${name} src/${name}.cpp)
    target_link_libraries(${name} PRIVATE STDEXEC::stdexec)
    target_compile_options(${name} PRIVATE -Wall -Wextra -Wpedantic)
endfunction()

# Examples
add_example(01_basic_sender)
add_example(02_then_chain)
add_example(03_when_all)
add_example(04_let_value)
add_example(05_thread_pool)
add_example(06_error_handling)
add_example(07_cancellation)
add_example(08_custom_sender)
add_example(09_channel_comparison)
