include_directories(..)

set(SPACELESS_COMMAND_GENERATOR_SRC
        main.cpp)
add_executable(spaceless_command_generator ${SPACELESS_COMMAND_GENERATOR_SRC})

target_link_libraries(spaceless_command_generator lights_static)
