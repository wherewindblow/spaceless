include_directories(..)

set(SPACELESS_RESOURCE_SERVER_SRC
        main.cpp
        resource_server.h resource_server.cpp)
add_executable(spaceless_resource_server ${SPACELESS_RESOURCE_SERVER_SRC})

target_link_libraries(spaceless_resource_server
        spaceless_common
        spaceless_protocol
        lights_static
        protobuf
        PocoFoundation
        PocoNet
        PocoUtil)
