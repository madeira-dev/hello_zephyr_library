# Check if the library is enabled via Kconfig
if(CONFIG_HELLO_LIB)
    # Create a Zephyr library
    zephyr_library()

    # Add source files
    zephyr_library_sources(src/hello_lib.c)

    # Make the include directory available to this library
    zephyr_library_include_directories(include)

    # Make the include directory available to targets that link against this library
    zephyr_include_directories(include)
endif()