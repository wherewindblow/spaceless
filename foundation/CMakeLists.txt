include_directories(..)

set(SPACELESS_COMMON_SRC
        basics.h basics.cpp
        exception.h
        log.h log.cpp
        package.h package.cpp
        network.h network.cpp
        transaction.h transaction.cpp
        worker.h worker.cpp
        scheduler.h scheduler.cpp
        configuration.h configuration.cpp
        monitor.h monitor.cpp)

add_library(spaceless_foundation STATIC ${SPACELESS_COMMON_SRC})
set_target_properties(spaceless_foundation PROPERTIES OUTPUT_NAME "spaceless_foundation")
