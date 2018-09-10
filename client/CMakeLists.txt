include_directories(..)

set(SPACELESS_CLIENT_SRC
        main.cpp
        core.h core.cpp)
add_executable(spaceless_client ${SPACELESS_CLIENT_SRC})

target_link_libraries(spaceless_client
        pthread
        spaceless_common
        spaceless_protocol
        lights_static
        protobuf
        pthread
        PocoFoundation
        PocoNet
        PocoUtil)
