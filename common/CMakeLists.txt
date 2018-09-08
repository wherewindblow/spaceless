include_directories(..)

set(SPACELESS_COMMON_SRC
        basics.h basics.cpp
        exception.h
        log.h log.cpp
        package.h package.cpp
        network.h network.cpp
        work_schedule.h work_schedule.cpp
        transaction.h transaction.cpp)

add_library(spaceless_common STATIC ${SPACELESS_COMMON_SRC})
set_target_properties(spaceless_common PROPERTIES OUTPUT_NAME "spaceless_common")
