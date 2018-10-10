include_directories(..)

set(SPACELESS_PROTOCOL_SRC
        all.h
        message_declare.h message.h
        protocol.pb.h protocol.pb.cc
        command.h command.cpp)

add_library(spaceless_protocol STATIC ${SPACELESS_PROTOCOL_SRC})
set_target_properties(spaceless_protocol PROPERTIES OUTPUT_NAME "spaceless_protocol")
