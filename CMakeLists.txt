# Check if the library is enabled via Kconfig
if(CONFIG_HELLO_LIB)
    # Create a Zephyr library
    zephyr_library()

    # Add all the math library sources that hello_lib depends on
    zephyr_library_sources(
        src/openfhe_embedded.c
        src/core/math/biginteger.c
        src/core/math/dgg.c
        src/core/math/math_hal.c
        src/core/math/polynomial.c
        src/core/math/prime_utils.c
        src/core/utils/serialization.c
        src/core/scheme/ckks/ckks_cryptoparams.c
        src/core/scheme/ckks/ckks_encoder.c
        src/core/scheme/ckks/ckks_keygen.c
        src/core/scheme/ckks/ckks_encryptor.c
        )

    # Make the include directory available to this library
    zephyr_library_include_directories(include)

    # Include directories for headers
    zephyr_library_include_directories(
        src
        src/core
        src/core/math
        src/core/lattice
        src/core/scheme
        src/core/scheme/ckks
        )

    # Make the include directory available to targets that link against this library
    zephyr_include_directories(include)
endif()