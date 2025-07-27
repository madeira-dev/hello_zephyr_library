# Check if the library is enabled via Kconfig
if(CONFIG_HELLO_LIB)
    # Create a Zephyr library
    zephyr_library()

    # Add main hello_lib source file
    zephyr_library_sources(src/hello_lib.c)

    # Add all the math library sources that hello_lib depends on
    zephyr_library_sources(
        src/core/math/biginteger.c
        src/core/math/math_hal.c
        src/core/math/polynomial.c
        )

    # Make the include directory available to this library
    zephyr_library_include_directories(include)

    # Include directories for headers
    zephyr_library_include_directories(
        src
        src/core
        src/core/math
        src/core/lattice
        )

    # Make the include directory available to targets that link against this library
    zephyr_include_directories(include)
endif()