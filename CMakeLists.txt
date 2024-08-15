cmake_minimum_required(VERSION 3.12)

project(harava)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(vendor/clipp)
include_directories(vendor/clipp/include)

find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
endif(CCACHE_FOUND)

include_directories(./include)
file(GLOB SRC ./src/*.cpp)
add_executable(${PROJECT_NAME} ${SRC})

install(TARGETS ${PROJECT_NAME})
