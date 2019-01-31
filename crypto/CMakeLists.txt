include_directories(..)

set(SPACELESS_CRYPTO_SRC
        base64.h base64.cpp
        des.h des.cpp
        aes.h aes.cpp
        rsa.h rsa.cpp basic.h rng.h rng.cpp)

add_library(spaceless_crypto SHARED ${SPACELESS_CRYPTO_SRC})
set_target_properties(spaceless_crypto PROPERTIES OUTPUT_NAME "spaceless_crypto")
