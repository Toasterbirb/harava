cmake_minimum_required(VERSION 3.12)

project(harava)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
endif(CCACHE_FOUND)

include_directories(./include ./vendor)
add_executable(${PROJECT_NAME} ./src/main.cpp)

install(TARGETS ${PROJECT_NAME})
