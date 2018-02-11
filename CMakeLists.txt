cmake_minimum_required(VERSION 3.8)
project(spaceless)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -O0")


add_subdirectory(lights)
add_subdirectory(common)
add_subdirectory(protocol)
add_subdirectory(client)
add_subdirectory(resource_server)
add_subdirectory(storage_node)

include_directories(.)