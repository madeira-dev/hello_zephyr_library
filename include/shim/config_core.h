#ifndef __CONFIG_CORE_H__
#define __CONFIG_CORE_H__

// 1. Math Constant
#ifndef M_E
#define M_E 2.71828182845904523536
#endif

// M_PI is required for CKKS trigonometry (Chebyshev/FFT)
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// 2. BACKEND CONFIGURATION
#undef WITH_BE4
#define WITH_BE2 1

// 3. Disable Extras
#undef WITH_NOISE_DEBUG
#undef WITH_REDUCED_NOISE
#undef WITH_NTL
#undef WITH_TCM
#undef WITH_OPENMP
#undef WITH_NATIVEOPT

// 4. Math Configuration
#define CKKS_M_FACTOR 1
#undef HAVE_INT128
#define HAVE_INT64 1
#define NATIVEINT 32

// 5. SELECT BACKEND 2
#define MATHBACKEND 2

// CRITICAL: Define Max Modulus Size for 32-bit backend
// This allows the library to validate parameters.
#define MAX_MODULUS_SIZE 28

#define COMPOSITESCALING_MAX_MODULUS_SIZE 28

// 6. Thread Local Fix
#define thread_local static

#endif // __CONFIG_CORE_H__
