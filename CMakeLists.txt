# Check if the library is enabled via Kconfig
if(CONFIG_HELLO_LIB)
  zephyr_library_named(openfhe_embedded)

  # 1. CRITICAL DEFINITIONS for Embedded OpenFHE
  zephyr_library_compile_definitions(
    NATIVE_SIZE=32
    WITH_BE2=1
    WITH_BE4=OFF
    NATIVEINT=32
  )

  set(OPENFHE_ROOT /Users/madeira/openfhe-repo/openfhe-development)

  # === Sources
  zephyr_library_sources(
    src/api.cpp
    src/lattice_shim.cpp
    src/memory_override.cpp

    # Math Backend
    ${OPENFHE_ROOT}/src/core/lib/math/hal/intnat/benative-math-impl.cpp
    ${OPENFHE_ROOT}/src/core/lib/math/hal/intnat/mubintvecnat.cpp
    ${OPENFHE_ROOT}/src/core/lib/math/discretegaussiangeneratorgeneric.cpp
    ${OPENFHE_ROOT}/src/core/lib/math/distributiongenerator.cpp
    ${OPENFHE_ROOT}/src/core/lib/utils/hashutil.cpp
    ${OPENFHE_ROOT}/src/core/lib/utils/prng/blake2engine.cpp
    ${OPENFHE_ROOT}/src/core/lib/utils/get-call-stack.cpp
    ${OPENFHE_ROOT}/src/core/lib/utils/prng/blake2xb-ref.c
    ${OPENFHE_ROOT}/src/core/lib/utils/prng/blake2b-ref.c
    ${OPENFHE_ROOT}/src/core/lib/math/nbtheory2.cpp
    ${OPENFHE_ROOT}/src/core/lib/utils/memory.cpp
  )



  # === OpenFHE Include Directories ===
  # Add OpenFHE headers (but not all sources yet)
  zephyr_library_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include/shim
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${OPENFHE_ROOT}/src/pke/include
    ${OPENFHE_ROOT}/src/core/include
    ${OPENFHE_ROOT}/src/binfhe/include
  )

  # === Public include dir for dependents ===
  # Exposes cereal/* to applications that link this lib.
  # === Public include dirs ===
  zephyr_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include/shim
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${OPENFHE_ROOT}/src/pke/include
    ${OPENFHE_ROOT}/src/core/include
    ${OPENFHE_ROOT}/src/binfhe/include
  )

  zephyr_library_compile_options($<$<COMPILE_LANGUAGE:CXX>:-std=gnu++17>)
  zephyr_library_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Wno-pedantic>)

endif()
