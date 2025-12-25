# Check if the library is enabled via Kconfig
if(CONFIG_HELLO_LIB)
  zephyr_library_named(openfhe_embedded)

  # 1. CRITICAL DEFINITIONS for Embedded OpenFHE
  zephyr_library_compile_definitions(
    NATIVE_SIZE=32
    WITH_BE2=1
    WITH_BE4=OFF
    NATIVEINT=32
    MATHBACKEND=2
    M_PI=3.14159265358979323846
    M_E=2.71828182845904523536
  )

  set(OPENFHE_ROOT /Users/madeira/openfhe-repo/openfhe-development)

  # === Sources
  zephyr_library_sources(
    src/api.cpp
    src/lattice_shim.cpp
    src/memory_override.cpp
    src/ckksrns-scheme-patched.cpp
    src/cryptocontext-patched.cpp

    # === Core Math ===
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
    ${OPENFHE_ROOT}/src/core/lib/lattice/constants-lattice-impl.cpp
    ${OPENFHE_ROOT}/src/core/lib/lattice/stdlatticeparms.cpp
    ${OPENFHE_ROOT}/src/core/lib/math/dftransform.cpp
    ${OPENFHE_ROOT}/src/core/lib/math/chebyshev.cpp

    # --- PKE / SCHEME (CKKS) ---
    # 1. Base Crypto Context & Keys
    # ${OPENFHE_ROOT}/src/pke/lib/cryptocontext.cpp
    ${OPENFHE_ROOT}/src/pke/lib/cryptocontextfactory.cpp
    ${OPENFHE_ROOT}/src/pke/lib/key/publickey.cpp
    ${OPENFHE_ROOT}/src/pke/lib/key/privatekey.cpp
    ${OPENFHE_ROOT}/src/pke/lib/key/evalkey.cpp
    ${OPENFHE_ROOT}/src/pke/lib/schemebase/rlwe-cryptoparameters-impl.cpp
    ${OPENFHE_ROOT}/src/pke/lib/schemebase/base-pke.cpp

    ${OPENFHE_ROOT}/src/pke/lib/encoding/ckkspackedencoding.cpp
    ${OPENFHE_ROOT}/src/pke/lib/scheme/ckksrns/ckksrns-leveledshe.cpp
    ${OPENFHE_ROOT}/src/pke/lib/scheme/ckksrns/ckksrns-multiparty.cpp
    ${OPENFHE_ROOT}/src/pke/lib/schemebase/base-multiparty.cpp
    ${OPENFHE_ROOT}/src/pke/lib/scheme/ckksrns/ckksrns-parametergeneration.cpp
    ${OPENFHE_ROOT}/src/pke/lib/scheme/ckksrns/ckksrns-pke.cpp
    ${OPENFHE_ROOT}/src/pke/lib/scheme/ckksrns/ckksrns-advancedshe.cpp
    # ${OPENFHE_ROOT}/src/pke/lib/scheme/ckksrns/ckksrns-fhe.cpp
    # ${OPENFHE_ROOT}/src/pke/lib/scheme/ckksrns/ckksrns-scheme.cpp
    ${OPENFHE_ROOT}/src/pke/lib/schemerns/rns-pke.cpp
    ${OPENFHE_ROOT}/src/core/lib/lattice/stdlatticeparms.cpp
    ${OPENFHE_ROOT}/src/core/lib/math/dftransform.cpp
    ${OPENFHE_ROOT}/src/pke/lib/schemebase/base-scheme.cpp
    ${OPENFHE_ROOT}/src/pke/lib/scheme/gen-cryptocontext-params-impl.cpp
    ${OPENFHE_ROOT}/src/pke/lib/scheme/gen-cryptocontext-params-validation.cpp
    ${OPENFHE_ROOT}/src/pke/lib/schemerns/rns-cryptoparameters.cpp
    ${OPENFHE_ROOT}/src/pke/lib/scheme/ckksrns/ckksrns-cryptoparameters.cpp
    ${OPENFHE_ROOT}/src/pke/lib/ciphertext-impl.cpp
    ${OPENFHE_ROOT}/src/pke/lib/encoding/encodingparams.cpp
    ${OPENFHE_ROOT}/src/pke/lib/keyswitch/keyswitch-bv.cpp
    ${OPENFHE_ROOT}/src/pke/lib/keyswitch/keyswitch-hybrid.cpp
    ${OPENFHE_ROOT}/src/pke/lib/schemebase/base-leveledshe.cpp
    ${OPENFHE_ROOT}/src/pke/lib/schemebase/base-advancedshe.cpp
    ${OPENFHE_ROOT}/src/pke/lib/schemebase/base-pre.cpp
    ${OPENFHE_ROOT}/src/pke/lib/cryptoobject-impl.cpp
    ${OPENFHE_ROOT}/src/pke/lib/constants-impl.cpp
    ${OPENFHE_ROOT}/src/pke/lib/scheme/ckksrns/ckksrns-utils.cpp
    ${OPENFHE_ROOT}/src/core/lib/math/chebyshev.cpp
    ${OPENFHE_ROOT}/src/pke/lib/keyswitch/keyswitch-base.cpp
    ${OPENFHE_ROOT}/src/pke/lib/schemerns/rns-leveledshe.cpp
    ${OPENFHE_ROOT}/src/pke/lib/schemerns/rns-multiparty.cpp
    ${OPENFHE_ROOT}/src/pke/lib/encoding/packedencoding.cpp
    ${OPENFHE_ROOT}/src/pke/lib/encoding/coefpackedencoding.cpp
    ${OPENFHE_ROOT}/src/pke/lib/encoding/stringencoding.cpp
    # ${OPENFHE_ROOT}/src/pke/lib/scheme/ckksrns/ckksrns-schemeswitching.cpp
    # ${OPENFHE_ROOT}/src/binfhe/lib/binfhecontext.cpp
    # ${OPENFHE_ROOT}/src/binfhe/lib/binfhe-base-scheme.cpp
    # ${OPENFHE_ROOT}/src/binfhe/lib/lwe-pke.cpp
    # ${OPENFHE_ROOT}/src/binfhe/lib/rgsw-acc.cpp
    # ${OPENFHE_ROOT}/src/binfhe/lib/rgsw-acc-dm.cpp
    # ${OPENFHE_ROOT}/src/binfhe/lib/rgsw-acc-cggi.cpp
    # ${OPENFHE_ROOT}/src/binfhe/lib/rgsw-acc-lmkcdey.cpp
    # ${OPENFHE_ROOT}/src/binfhe/lib/rgsw-cryptoparameters.cpp
    # ${OPENFHE_ROOT}/src/binfhe/lib/binfhe-constants-impl.cpp
    ${OPENFHE_ROOT}/src/pke/lib/globals-impl.cpp


    # 4. Scheme Base Classes (Keep this)
    ${OPENFHE_ROOT}/src/pke/lib/scheme/scheme-id-impl.cpp
  )

  # === OpenFHE Include Directories ===
  # Add OpenFHE headers (but not all sources yet)
  zephyr_library_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include/shim
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${OPENFHE_ROOT}
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

  zephyr_library_compile_options($<$<COMPILE_LANGUAGE:CXX>:-frtti>)
  zephyr_library_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fexceptions>)

endif()
