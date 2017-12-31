include_directories(..)

set(SPACELESS_PROTOCOL_SRC
        protocol.pb.h protocol.pb.cc all.h)

add_library(spaceless_protocol STATIC ${SPACELESS_PROTOCOL_SRC})
set_target_properties(spaceless_protocol PROPERTIES OUTPUT_NAME "spaceless_protocol")
