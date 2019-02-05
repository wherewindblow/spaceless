include_directories(..)

set(SPACELESS_FOUNDATION_SRC
        basics.h basics.cpp
        exception.h
        log.h log.cpp
        package.h package.cpp
        network.h network.cpp
        network_message.h network_message.cpp
        transaction.h transaction.cpp
        worker.h worker.cpp
        scheduler.h scheduler.cpp
        configuration.h configuration.cpp
        monitor.h monitor.cpp
        delegation.h delegation.cpp
        details/network_impl.h details/network_impl.cpp)

add_library(spaceless_foundation SHARED ${SPACELESS_FOUNDATION_SRC})
set_target_properties(spaceless_foundation PROPERTIES OUTPUT_NAME "spaceless_foundation")
