#ifndef OPENFHE_CORE_MATH_BIGINTEGER_H_
#define OPENFHE_CORE_MATH_BIGINTEGER_H_

#include <zephyr/kernel.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "math_hal.h"

/**
 * @file biginteger.h
 * @brief Big integer arithmetic for lattice-based cryptography
 *
 * This file implements arbitrary precision integer arithmetic needed for
 * cryptographic operations in CKKS and other lattice schemes. Big integers
 * are used for:
 * - Polynomial coefficients (typically 50-100+ bits)
 * - Cryptographic parameters and keys
 * - Modular arithmetic with large moduli
 *
 * The implementation is optimized for embedded systems with limited memory
 * while maintaining cryptographic security requirements.
 */

// Optimize word size for ARM Cortex-M4 32-bit architecture
#ifdef CONFIG_SOC_NRF52840
#define BIGINT_MAX_WORDS 4   // Reduced from 8 (128 bits max)
#define BIGINT_WORD_BITS 32  // Use 32-bit words for better ARM performance
#define BIGINT_SMALL_SIZE 2  // 64 bits
#define BIGINT_MEDIUM_SIZE 3 // 96 bits
#define BIGINT_LARGE_SIZE 4  // 128 bits
#else
#define BIGINT_MAX_WORDS 8
#define BIGINT_WORD_BITS 64
#define BIGINT_MAX_BITS (BIGINT_MAX_WORDS * BIGINT_WORD_BITS)

// Commonly used sizes for CKKS parameters
#define BIGINT_SMALL_SIZE 2  // 128 bits - for smaller operations
#define BIGINT_MEDIUM_SIZE 4 // 256 bits - for most CKKS operations
#define BIGINT_LARGE_SIZE 8  // 512 bits - for high-security parameters
#endif

// Use 32-bit word type for nRF52840
#ifdef CONFIG_SOC_NRF52840
typedef uint32_t bigint_word_t;
#else
typedef uint64_t bigint_word_t;
#endif

typedef struct
{
    bigint_word_t words[BIGINT_MAX_WORDS]; // Use platform-specific word type
    uint32_t word_count;
    bool is_negative;
} bigint_t;

// ============================================================================
// Initialization and Basic Operations
// ============================================================================

/**
 * @brief Initialize big integer to zero
 * @param a Big integer to initialize
 */
void bigint_init(bigint_t *a);

/**
 * @brief Initialize big integer from unsigned 64-bit value
 * @param a Big integer to initialize
 * @param value Initial value
 */
void bigint_init_u64(bigint_t *a, uint64_t value);

/**
 * @brief Initialize big integer from string (hexadecimal)
 * @param a Big integer to initialize
 * @param hex_string Hexadecimal string (without "0x" prefix)
 * @return 0 on success, negative on error
 */
int bigint_init_string(bigint_t *a, const char *hex_string);

/**
 * @brief Copy big integer
 * @param dest Destination
 * @param src Source
 */
void bigint_copy(bigint_t *dest, const bigint_t *src);

/**
 * @brief Set big integer to zero
 * @param a Big integer to zero
 */
void bigint_zero(bigint_t *a);

/**
 * @brief Check if big integer is zero
 * @param a Big integer to check
 * @return true if zero, false otherwise
 */
bool bigint_is_zero(const bigint_t *a);

/**
 * @brief Check if big integer is one
 * @param a Big integer to check
 * @return true if one, false otherwise
 */
bool bigint_is_one(const bigint_t *a);

// ============================================================================
// Comparison Operations
// ============================================================================

/**
 * @brief Compare two big integers
 * @param a First big integer
 * @param b Second big integer
 * @return -1 if a < b, 0 if a == b, 1 if a > b
 */
int bigint_compare(const bigint_t *a, const bigint_t *b);

/**
 * @brief Check if two big integers are equal
 * @param a First big integer
 * @param b Second big integer
 * @return true if equal, false otherwise
 */
bool bigint_equals(const bigint_t *a, const bigint_t *b);

/**
 * @brief Check if big integer is less than another
 * @param a First big integer
 * @param b Second big integer
 * @return true if a < b, false otherwise
 */
bool bigint_less_than(const bigint_t *a, const bigint_t *b);

// ============================================================================
// Arithmetic Operations
// ============================================================================

/**
 * @brief Add two big integers: result = a + b
 * @param result Output big integer
 * @param a First operand
 * @param b Second operand
 * @return 0 on success, negative on overflow
 */
int bigint_add(bigint_t *result, const bigint_t *a, const bigint_t *b);

/**
 * @brief Subtract big integers: result = a - b (assumes a >= b)
 * @param result Output big integer
 * @param a Minuend
 * @param b Subtrahend
 * @return 0 on success, negative if a < b
 */
int bigint_sub(bigint_t *result, const bigint_t *a, const bigint_t *b);

/**
 * @brief Multiply two big integers: result = a * b
 * @param result Output big integer
 * @param a First operand
 * @param b Second operand
 * @return 0 on success, negative on overflow
 */
int bigint_mult(bigint_t *result, const bigint_t *a, const bigint_t *b);

/**
 * @brief Multiply big integer by small word: result = a * word
 * @param result Output big integer
 * @param a Big integer operand
 * @param word Word operand
 * @return 0 on success, negative on overflow
 */
int bigint_mult_word(bigint_t *result, const bigint_t *a, math_word_t word);

/**
 * @brief Divide big integer by word: quotient = a / word, remainder = a % word
 * @param quotient Output quotient (can be NULL)
 * @param remainder Output remainder (can be NULL)
 * @param a Dividend
 * @param word Divisor
 * @return 0 on success, negative on error (division by zero)
 */
int bigint_div_word(bigint_t *quotient, math_word_t *remainder,
                    const bigint_t *a, math_word_t word);

// ============================================================================
// Modular Arithmetic
// ============================================================================

/**
 * @brief Modular addition: result = (a + b) mod m
 * @param result Output big integer
 * @param a First operand
 * @param b Second operand
 * @param m Modulus
 * @return 0 on success, negative on error
 */
int bigint_mod_add(bigint_t *result, const bigint_t *a, const bigint_t *b, const bigint_t *m);

/**
 * @brief Modular subtraction: result = (a - b) mod m
 * @param result Output big integer
 * @param a First operand
 * @param b Second operand
 * @param m Modulus
 * @return 0 on success, negative on error
 */
int bigint_mod_sub(bigint_t *result, const bigint_t *a, const bigint_t *b, const bigint_t *m);

/**
 * @brief Modular multiplication: result = (a * b) mod m
 * @param result Output big integer
 * @param a First operand
 * @param b Second operand
 * @param m Modulus
 * @return 0 on success, negative on error
 */
int bigint_mod_mult(bigint_t *result, const bigint_t *a, const bigint_t *b, const bigint_t *m);

/**
 * @brief Modular exponentiation: result = base^exp mod m
 * @param result Output big integer
 * @param base Base
 * @param exp Exponent
 * @param m Modulus
 * @return 0 on success, negative on error
 */
int bigint_mod_pow(bigint_t *result, const bigint_t *base, const bigint_t *exp, const bigint_t *m);

/**
 * @brief Reduce big integer modulo m: result = a mod m
 * @param result Output big integer
 * @param a Input big integer
 * @param m Modulus
 * @return 0 on success, negative on error
 */
int bigint_mod(bigint_t *result, const bigint_t *a, const bigint_t *m);

// ============================================================================
// Bit Operations
// ============================================================================

/**
 * @brief Left shift big integer by specified number of bits
 * @param result Output big integer
 * @param a Input big integer
 * @param shift Number of bits to shift
 * @return 0 on success, negative on overflow
 */
int bigint_shift_left(bigint_t *result, const bigint_t *a, uint32_t shift);

/**
 * @brief Right shift big integer by specified number of bits
 * @param result Output big integer
 * @param a Input big integer
 * @param shift Number of bits to shift
 * @return 0 on success, negative on error
 */
int bigint_shift_right(bigint_t *result, const bigint_t *a, uint32_t shift);

/**
 * @brief Get bit at specified position
 * @param a Big integer
 * @param bit_pos Bit position (0 = least significant)
 * @return Bit value (0 or 1)
 */
int bigint_get_bit(const bigint_t *a, uint32_t bit_pos);

/**
 * @brief Set bit at specified position
 * @param a Big integer to modify
 * @param bit_pos Bit position (0 = least significant)
 * @param value Bit value (0 or 1)
 */
void bigint_set_bit(bigint_t *a, uint32_t bit_pos, int value);

/**
 * @brief Count number of significant bits
 * @param a Big integer
 * @return Number of bits needed to represent the value
 */
uint32_t bigint_bit_count(const bigint_t *a);

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Convert big integer to string (hexadecimal)
 * @param a Big integer to convert
 * @param buffer Output buffer
 * @param buffer_size Size of output buffer
 * @return 0 on success, negative on error
 */
int bigint_to_string(const bigint_t *a, char *buffer, size_t buffer_size);

/**
 * @brief Generate random big integer with specified bit length
 * @param result Output big integer
 * @param bit_length Number of random bits
 * @return 0 on success, negative on error
 */
int bigint_random(bigint_t *result, uint32_t bit_length);

/**
 * @brief Generate random big integer less than specified maximum
 * @param result Output big integer
 * @param max Maximum value (exclusive)
 * @return 0 on success, negative on error
 */
int bigint_random_mod(bigint_t *result, const bigint_t *max);

/**
 * @brief Validate big integer structure (for debugging)
 * @param a Big integer to validate
 * @return true if valid, false otherwise
 */
bool bigint_is_valid(const bigint_t *a);

/**
 * @brief Update word count to remove leading zeros
 * @param a Big integer to normalize
 */
void bigint_normalize(bigint_t *a);

#endif /* OPENFHE_CORE_MATH_BIGINTEGER_H_ */