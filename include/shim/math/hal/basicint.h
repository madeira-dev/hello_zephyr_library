#ifndef LBCRYPTO_MATH_HAL_BASICINT_H
#define LBCRYPTO_MATH_HAL_BASICINT_H

#include "config_core.h"
#include <cstdint>
#include <type_traits>

// --- SHIM CONFIGURATION ---
// 1. We define these in the GLOBAL namespace to ensure visibility everywhere.
// 2. We alias 128-bit types to 64-bit. This allows the code to compile
//    valid C++ syntax on the Cortex-M4, which lacks native 128-bit support.

using BasicInteger = uint32_t;
using DoubleNativeInt = uint64_t;

// Must be a valid integer type for static_cast<...> to compile.
// Since we are in 32-bit mode, 64-bit is the largest available container.
using uint128_t = uint64_t;
using int128_t = int64_t;

namespace lbcrypto {
// OpenFHE classes expect these typedefs to be available in the
// global scope for the 32-bit backend logic to work correctly
// across all modules.
}

#endif // LBCRYPTO_MATH_HAL_BASICINT_H