include_directories(..)

set(SPACELESS_FOUNDATION_SRC
        basics.h basics.cpp
        exception.h exception.cpp
        log.h log.cpp
        package.h package.cpp
        network.h network.cpp
        actor_message.h actor_message.cpp
        transaction.h transaction.cpp
        worker.h worker.cpp
        scheduler.h scheduler.cpp
        configuration.h configuration.cpp
        monitor.h monitor.cpp
        delegation.h delegation.cpp
        details/network_impl.h details/network_impl.cpp)

add_library(spaceless_foundation SHARED ${SPACELESS_FOUNDATION_SRC})
set_target_properties(spaceless_foundation PROPERTIES OUTPUT_NAME "spaceless_foundation")
