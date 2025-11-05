# Check if the library is enabled via Kconfig
if(CONFIG_HELLO_LIB)
    # Create a Zephyr library
    zephyr_library()

    # Add all the math library sources that hello_lib depends on
    zephyr_library_sources(
        src/openfhe_embedded.c
        src/ckks/ckks_params.c
        src/math/ntt.c
        src/ckks/encode.c
        )

    # Make the include directory available to this library
    zephyr_library_include_directories(include)

    # Include directories for headers
    zephyr_library_include_directories(
        src
        src/internal
        src/ckks
        )

    # Make the include directory available to targets that link against this library
    zephyr_include_directories(include)
endif()
