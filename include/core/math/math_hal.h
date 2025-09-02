#ifndef OPENFHE_CORE_MATH_HAL_H_
#define OPENFHE_CORE_MATH_HAL_H_

#include <zephyr/kernel.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * @file math_hal.h
 * @brief Mathematics Hardware Abstraction Layer
 *
 * This file provides a hardware abstraction layer for mathematical operations
 * used in lattice-based cryptography. It abstracts platform-specific
 * optimizations like SIMD instructions, hardware crypto accelerators,
 * and memory-efficient algorithms suitable for embedded systems.
 *
 * Key responsibilities:
 * - Fast modular arithmetic
 * - Number Theoretic Transform (NTT) primitives
 * - Random number generation
 * - Platform-specific optimizations
 * - Memory-constrained algorithm selection
 */

// ============================================================================
// Platform Configuration (nRF52840-only)
// ============================================================================

// Platform Configuration (nRF52840-only)
#define MATH_HAL_HAS_DSP_INSTRUCTIONS 1
#define MATH_HAL_HAS_FPU 1
#define MATH_HAL_HAS_HARDWARE_RNG 1
#define MATH_HAL_HAS_AES_ACCELERATOR 1
#define MATH_HAL_WORD_SIZE 32
#define MATH_HAL_CPU_FREQUENCY_MHZ 64

// Maximum modulus size supported (in bits)
#define MATH_HAL_MAX_MODULUS_BITS 64
#define MATH_HAL_MAX_WORD_BITS 64

// NTT-specific parameters
#define MATH_HAL_MAX_NTT_SIZE 4096
#define MATH_HAL_NTT_WORD_SIZE 64

/**
 * @brief Basic word type for mathematical operations
 */
typedef uint32_t math_word_t; // Use 32-bit for nRF52840
typedef uint16_t math_half_word_t;

// ============================================================================
// Modular Arithmetic Primitives
// ============================================================================

/**
 * @brief Fast modular addition: result = (a + b) mod m
 * @param a First operand
 * @param b Second operand
 * @param m Modulus
 * @return (a + b) mod m
 *
 * Optimized for specific modulus sizes common in CKKS
 */
math_word_t math_hal_mod_add(math_word_t a, math_word_t b, math_word_t m);

/**
 * @brief Fast modular subtraction: result = (a - b) mod m
 * @param a Minuend
 * @param b Subtrahend
 * @param m Modulus
 * @return (a - b) mod m
 */
math_word_t math_hal_mod_sub(math_word_t a, math_word_t b, math_word_t m);

/**
 * @brief Fast modular multiplication: result = (a * b) mod m
 * @param a First operand
 * @param b Second operand
 * @param m Modulus
 * @return (a * b) mod m
 *
 * Uses Montgomery multiplication or Barrett reduction for efficiency
 */
math_word_t math_hal_mod_mult(math_word_t a, math_word_t b, math_word_t m);

/**
 * @brief Fast modular exponentiation: result = base^exp mod m
 * @param base Base value
 * @param exp Exponent
 * @param m Modulus
 * @return base^exp mod m
 *
 * Uses square-and-multiply algorithm
 */
math_word_t math_hal_mod_pow(math_word_t base, math_word_t exp, math_word_t m);

/**
 * @brief Modular inverse: result = a^(-1) mod m
 * @param a Value to invert
 * @param m Modulus (must be prime)
 * @return a^(-1) mod m, or 0 if inverse doesn't exist
 *
 * Uses extended Euclidean algorithm
 */
math_word_t math_hal_mod_inv(math_word_t a, math_word_t m);

// ============================================================================
// Number Theoretic Transform (NTT) Operations
// ============================================================================

/**
 * @brief NTT parameters structure
 */
typedef struct
{
  math_word_t modulus;           // NTT modulus (must be prime)
  math_word_t root_of_unity;     // Primitive n-th root of unity
  math_word_t inv_root_of_unity; // Inverse of root of unity
  math_word_t inv_n;             // Modular inverse of n
  uint32_t n;                    // Transform size (power of 2)
  uint32_t log_n;                // log2(n)
} ntt_params_t;

/**
 * @brief Initialize NTT parameters for given transform size
 * @param params Output NTT parameters
 * @param n Transform size (must be power of 2)
 * @param modulus Prime modulus for NTT
 * @return 0 on success, negative on error
 */
int math_hal_ntt_init_params(ntt_params_t *params, uint32_t n, math_word_t modulus);

/**
 * @brief Forward Number Theoretic Transform
 * @param data Input/output array (modified in-place)
 * @param params NTT parameters
 * @return 0 on success, negative on error
 *
 * Converts polynomial from coefficient representation to NTT form
 */
int math_hal_ntt_forward(math_word_t *data, const ntt_params_t *params);

/**
 * @brief Inverse Number Theoretic Transform
 * @param data Input/output array (modified in-place)
 * @param params NTT parameters
 * @return 0 on success, negative on error
 *
 * Converts polynomial from NTT form back to coefficient representation
 */
int math_hal_ntt_inverse(math_word_t *data, const ntt_params_t *params);

/**
 * @brief Element-wise multiplication in NTT domain
 * @param result Output array
 * @param a First input array
 * @param b Second input array
 * @param params NTT parameters
 * @return 0 on success, negative on error
 */
int math_hal_ntt_mult(math_word_t *result, const math_word_t *a,
                      const math_word_t *b, const ntt_params_t *params);

// ============================================================================
// Random Number Generation
// ============================================================================

/**
 * @brief Initialize cryptographically secure random number generator
 * @return 0 on success, negative on error
 *
 * Uses hardware RNG if available, otherwise falls back to software PRNG
 */
int math_hal_rng_init(void);

/**
 * @brief Generate random bytes
 * @param buffer Output buffer
 * @param size Number of bytes to generate
 * @return 0 on success, negative on error
 */
int math_hal_rng_bytes(uint8_t *buffer, size_t size);

/**
 * @brief Generate random integer in range [0, max)
 * @param max Upper bound (exclusive)
 * @return Random integer in specified range
 */
math_word_t math_hal_rng_uniform(math_word_t max);

/**
 * @brief Generate random integer modulo m with uniform distribution
 * @param m Modulus
 * @return Random integer in [0, m)
 */
math_word_t math_hal_rng_mod(math_word_t m);

/**
 * @brief Use nRF52840 hardware RNG
 * @param buffer Output buffer
 * @param size Number of bytes to generate
 * @return 0 on success, negative on error
 */
int math_hal_hw_rng_bytes(uint8_t *buffer, size_t size);

// ============================================================================
// Memory and Performance Utilities
// ============================================================================

/**
 * @brief Secure memory zero (resistant to compiler optimization)
 * @param ptr Memory to zero
 * @param size Number of bytes to zero
 */
void math_hal_secure_zero(void *ptr, size_t size);

/**
 * @brief Check if number is prime (for modulus validation)
 * @param n Number to test
 * @return true if prime, false otherwise
 */
bool math_hal_is_prime(math_word_t n);

/**
 * @brief Find next prime greater than or equal to n
 * @param n Starting value
 * @return Next prime >= n
 */
math_word_t math_hal_next_prime(math_word_t n);

/**
 * @brief Calculate greatest common divisor
 * @param a First number
 * @param b Second number
 * @return GCD(a, b)
 */
math_word_t math_hal_gcd(math_word_t a, math_word_t b);

/**
 * @brief Get platform-specific timing for performance measurement
 * @return Current timestamp in platform-specific units
 */
uint64_t math_hal_get_cycles(void);

#endif /* OPENFHE_CORE_MATH_HAL_H_ */