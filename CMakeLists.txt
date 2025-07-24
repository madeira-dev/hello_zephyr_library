zephyr_library_named(hello_lib)

zephyr_library_sources_ifdef(CONFIG_HELLO_LIB
    src/hello_lib.c
)

# Make the include directory available to this library
zephyr_library_include_directories_ifdef(CONFIG_HELLO_LIB
    include
)

# Make the include directory available to targets that link against this library
zephyr_include_directories_ifdef(CONFIG_HELLO_LIB
    include
)