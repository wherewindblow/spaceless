include_directories(..)

set(SPACELESS_COMMON_SRC
        log.h log.cpp
        network.h network.cpp basics.h exception.h)

add_library(spaceless_common STATIC ${SPACELESS_COMMON_SRC})
set_target_properties(spaceless_common PROPERTIES OUTPUT_NAME "spaceless_common")
