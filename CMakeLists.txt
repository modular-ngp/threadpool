cmake_minimum_required(VERSION 4.00)
project(threadpool VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_EXPERIMENTAL_CXX_MODULE_CMAKE_API ON)

add_library(threadpool)
add_library(threadpool::threadpool ALIAS threadpool)
target_sources(threadpool
        PUBLIC
        FILE_SET cxx_modules TYPE CXX_MODULES FILES
        modules/threadpool.ixx
)

find_package(Threads REQUIRED)
target_link_libraries(threadpool PRIVATE Threads::Threads)