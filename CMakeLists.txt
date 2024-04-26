cmake_minimum_required(VERSION 3.0.0)
project(logging VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_FLAGS_DEBUG "-ggdb3 -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

option(USE_BOOST "Enable using boost.asio instead of internal multithreading mechanism" OFF)

add_executable(Test diagnostics/tests/main.cpp)
target_compile_definitions(Test PRIVATE USE_CATCH2)
target_compile_options(Test PRIVATE "-Wall" "-Wextra")
target_include_directories(Test PRIVATE ${CMAKE_SOURCE_DIR}/..)

if(USE_BOOST)
    find_package(Boost REQUIRED)
    target_link_libraries(Test PRIVATE boost)
endif()
