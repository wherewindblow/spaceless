include_directories(..)

set(SPACELESS_RESOURCE_SERVER_SRC
        main.cpp
        resource_server.h resource_server.cpp)
add_executable(space_resource_server ${SPACELESS_RESOURCE_SERVER_SRC})

find_package(Boost REQUIRED COMPONENTS system filesystem)
include_directories(${Boost_INCLUDE_DIR})

target_link_libraries(space_resource_server spaceless_common lights_static ${Boost_LIBRARIES})
