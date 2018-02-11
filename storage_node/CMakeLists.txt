include_directories(..)

set(SPACELESS_STORAGE_NODE_SRC
        main.cpp
        core.h core.cpp
        transcation.h transcation.cpp)

add_executable(spaceless_storage_node ${SPACELESS_STORAGE_NODE_SRC})

target_link_libraries(spaceless_storage_node
        spaceless_common
        spaceless_protocol
        lights_static
        protobuf
        PocoFoundation
        PocoNet
        PocoUtil)
