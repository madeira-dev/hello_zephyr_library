# Check if the library is enabled via Kconfig
if(CONFIG_HELLO_LIB)
  zephyr_library_named(openfhe_embedded)

  # 1. CRITICAL DEFINITIONS for Embedded OpenFHE
  zephyr_library_compile_definitions(
    NATIVE_SIZE=32
    WITH_BE2=OFF
    WITH_BE4=1
    UBINT_32=1 # Force Dynamic Backend to use 64-bit limbs, NOT 128-bit
  )

  set(OPENFHE_ROOT /Users/madeira/openfhe-repo/openfhe-development)

  # === Sources
  zephyr_library_sources(
    src/api.cpp
    src/lattice_shim.cpp

    # Math Backend
    ${OPENFHE_ROOT}/src/core/lib/math/hal/intnat/mubintvecnat.cpp
    ${OPENFHE_ROOT}/src/core/lib/math/hal/bigintdyn/mubintvecdyn.cpp
    ${OPENFHE_ROOT}/src/core/lib/math/hal/bigintdyn/ubintdyn.cpp
    ${OPENFHE_ROOT}/src/core/lib/math/discretegaussiangeneratorgeneric.cpp
    ${OPENFHE_ROOT}/src/core/lib/math/hal/bigintdyn/be4-math-impl.cpp
    ${OPENFHE_ROOT}/src/core/lib/math/hal/intnat/benative-math-impl.cpp
    ${OPENFHE_ROOT}/src/core/lib/math/distributiongenerator.cpp
    ${OPENFHE_ROOT}/src/core/lib/utils/hashutil.cpp
    ${OPENFHE_ROOT}/src/core/lib/utils/prng/blake2engine.cpp
    ${OPENFHE_ROOT}/src/core/lib/utils/get-call-stack.cpp
    ${OPENFHE_ROOT}/src/core/lib/utils/memory.cpp
    ${OPENFHE_ROOT}/src/core/lib/utils/prng/blake2xb-ref.c
    ${OPENFHE_ROOT}/src/core/lib/utils/prng/blake2b-ref.c
    # ${OPENFHE_ROOT}/src/core/lib/math/hal/bigintntl/be6-math-impl.cpp

    # -- Math Utilities ---
    # Required for Roots of Unity and Prime Generation
    ${OPENFHE_ROOT}/src/core/lib/math/nbtheory2.cpp

    # --- Lattice Layer ---
    # The core polynomial objects
    # ${OPENFHE_ROOT}/src/core/lib/lattice/trapdoor-dcrtpoly.cpp
    # ${OPENFHE_ROOT}/src/core/lib/lattice/trapdoor-poly.cpp
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

  # CHANGED: Switch to gnu++17.
  # Standard c++17 is strict and rejects some GNU extensions OpenFHE might rely on.
  # target_compile_options(openfhe_embedded PUBLIC -std=gnu++17)
  # Note: Try the line above if standard 17 fails again.
  target_compile_features(openfhe_embedded PUBLIC cxx_std_17)
endif()
