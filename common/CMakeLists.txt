include_directories(..)

set(SPACELESS_COMMON_SRC
        basics.h
        exception.h
        log.h log.cpp
        network.h network.cpp)

add_library(spaceless_common STATIC ${SPACELESS_COMMON_SRC})
set_target_properties(spaceless_common PROPERTIES OUTPUT_NAME "spaceless_common")
