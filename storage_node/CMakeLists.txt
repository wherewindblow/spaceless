include_directories(..)

set(SPACELESS_STORAGE_NODE_SRC
        main.cpp
        core.h core.cpp
        transaction.h transaction.cpp)

add_executable(spaceless_storage_node ${SPACELESS_STORAGE_NODE_SRC})

target_link_libraries(spaceless_storage_node
        pthread
        spaceless_foundation
        spaceless_protocol
        spaceless_crypto
        lights_shared
        protobuf
        cryptopp
        PocoFoundation
        PocoNet
        PocoUtil)
