include_directories(..)

set(SPACELESS_CLIENT_SRC
        main.cpp
        client.h client.cpp)
add_executable(spaceless_client ${SPACELESS_CLIENT_SRC})

find_package(Boost REQUIRED COMPONENTS system filesystem)
include_directories(${Boost_INCLUDE_DIR})

target_link_libraries(spaceless_client
        spaceless_common
        spaceless_protocol
        lights_static
        ${Boost_LIBRARIES}
        protobuf
        pthread)
