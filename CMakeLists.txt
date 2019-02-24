cmake_minimum_required(VERSION 3.8)
project(spaceless)

set(CMAKE_CXX_STANDARD 17)
# NOTE: Must define same macro that same with building. Because it maybe use to switch that place more variable
#       and not do that will leak to segment failure.
# POCO_ENABLE_CPP11 also can define in Poco/Config.h.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -O0 -DPOCO_ENABLE_CPP11")

add_subdirectory(lights)
add_subdirectory(foundation)
add_subdirectory(protocol)
add_subdirectory(crypto)
add_subdirectory(command_generator)
add_subdirectory(client)
add_subdirectory(resource_server)
add_subdirectory(storage_node)

include_directories(.)