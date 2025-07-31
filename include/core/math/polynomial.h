#ifndef OPENFHE_CORE_MATH_POLYNOMIAL_H_
#define OPENFHE_CORE_MATH_POLYNOMIAL_H_

#include <zephyr/kernel.h>
#include <stdint.h>
#include <stdbool.h>
#include "biginteger.h"
#include "math_hal.h"
#include "scheme/ckks/ckks_cryptoparams.h"

// ============================================================================
// RNS Polynomial Structures (for multi-modulus support)
// ============================================================================

/**
 * @brief RNS polynomial: array of polynomials, one per modulus
 *
 * Each component polynomial is modulo a different prime in the modulus chain.
 * Used for RNS representation in CKKS and other lattice schemes.
 */
#define RNS_MAX_MODULI CKKS_MAX_MODULI

typedef struct
{
    polynomial_t polys[RNS_MAX_MODULI]; // One polynomial per modulus
    uint32_t num_moduli;                // Number of moduli in use
} rns_polynomial_t;

/**
 * @brief RNS NTT parameters: NTT tables for each modulus
 */
typedef struct
{
    ntt_params_t ntt_params[RNS_MAX_MODULI];
    uint32_t num_moduli;
} rns_ntt_params_t;

// ============================================================================
// RNS Polynomial Operations API
// ============================================================================

/**
 * @brief Initialize an RNS polynomial (all component polys to zero)
 * @param rpoly RNS polynomial to initialize
 * @param degree Degree for each component polynomial
 * @param num_moduli Number of moduli (RNS components)
 * @return 0 on success, negative on error
 */
int rns_poly_init(rns_polynomial_t *rpoly, uint32_t degree, uint32_t num_moduli);

/**
 * @brief Copy one RNS polynomial to another
 * @param dest Destination RNS polynomial
 * @param src Source RNS polynomial
 * @return 0 on success, negative on error
 */
int rns_poly_copy(rns_polynomial_t *dest, const rns_polynomial_t *src);

/**
 * @brief Convert all component polynomials to NTT form
 * @param rpoly RNS polynomial to transform (in-place)
 * @param ntt_params NTT parameters for each modulus
 * @return 0 on success, negative on error
 */
int rns_poly_to_ntt(rns_polynomial_t *rpoly, const rns_ntt_params_t *ntt_params);

/**
 * @brief Convert all component polynomials from NTT form to coefficient form
 * @param rpoly RNS polynomial to transform (in-place)
 * @param ntt_params NTT parameters for each modulus
 * @return 0 on success, negative on error
 */
int rns_poly_from_ntt(rns_polynomial_t *rpoly, const rns_ntt_params_t *ntt_params);

/**
 * @brief Add two RNS polynomials: result = a + b (component-wise)
 * @param result Output RNS polynomial
 * @param a First input
 * @param b Second input
 * @param params Ring parameters for each modulus
 * @return 0 on success, negative on error
 */
int rns_poly_add(rns_polynomial_t *result, const rns_polynomial_t *a,
                 const rns_polynomial_t *b, const poly_ring_params_t *params, uint32_t num_moduli);

/**
 * @brief Multiply two RNS polynomials: result = a * b (component-wise)
 * @param result Output RNS polynomial
 * @param a First input
 * @param b Second input
 * @param params Ring parameters for each modulus
 * @return 0 on success, negative on error
 */
int rns_poly_mult(rns_polynomial_t *result, const rns_polynomial_t *a,
                  const rns_polynomial_t *b, const poly_ring_params_t *params, uint32_t num_moduli);

/**
 * @file polynomial.h
 * @brief Polynomial operations for lattice-based cryptography
 *
 * This file implements polynomial arithmetic over finite fields/rings.
 * In CKKS and other lattice schemes, polynomials are the fundamental
 * data structure representing both plaintexts and ciphertexts.
 *
 * Key operations include:
 * - Polynomial addition/subtraction
 * - Polynomial multiplication (including NTT-based)
 * - Modular reduction
 * - Coefficient-wise operations
 */

// Maximum degree of polynomials - SEVERELY REDUCED for nRF52840's 256KB RAM
#ifdef CONFIG_SOC_NRF52840
#define POLY_MAX_DEGREE 256 // Reduced from 1024 (still ~2.6KB per polynomial)
#else
#define POLY_MAX_DEGREE 4096
#endif
#define POLY_MAX_COEFFS (POLY_MAX_DEGREE + 1)

/**
 * @brief Polynomial structure
 *
 * Represents a polynomial as an array of coefficients.
 * For CKKS, coefficients are typically large integers modulo some prime.
 */
typedef struct
{
    bigint_t coeffs[POLY_MAX_COEFFS]; // Polynomial coefficients
    uint32_t degree;                  // Actual degree of polynomial
    uint32_t modulus_bits;            // Size of coefficient modulus in bits
    bool is_ntt_form;                 // Whether polynomial is in NTT form
} polynomial_t;

/**
 * @brief Polynomial ring parameters
 *
 * Defines the ring Z[X]/(X^N + 1) where N is ring_dimension.
 * This is the standard ring used in CKKS and other schemes.
 */
typedef struct
{
    uint32_t ring_dimension;      // N in Z[X]/(X^N + 1)
    bigint_t coefficient_modulus; // Modulus for coefficients
    uint32_t modulus_bits;        // Bit size of modulus
} poly_ring_params_t;

// ============================================================================
// Basic Polynomial Operations
// ============================================================================

/**
 * @brief Initialize a polynomial to zero
 * @param poly Polynomial to initialize
 * @param degree Maximum degree
 * @return 0 on success, negative on error
 */
int poly_init(polynomial_t *poly, uint32_t degree);

/**
 * @brief Clean up polynomial memory
 * @param poly Polynomial to cleanup
 */
void poly_cleanup(polynomial_t *poly);

/**
 * @brief Copy one polynomial to another
 * @param dest Destination polynomial
 * @param src Source polynomial
 * @return 0 on success, negative on error
 */
int poly_copy(polynomial_t *dest, const polynomial_t *src);

/**
 * @brief Set polynomial coefficient at given index
 * @param poly Target polynomial
 * @param index Coefficient index
 * @param value Coefficient value
 * @return 0 on success, negative on error
 */
int poly_set_coeff(polynomial_t *poly, uint32_t index, const bigint_t *value);

/**
 * @brief Get polynomial coefficient at given index
 * @param poly Source polynomial
 * @param index Coefficient index
 * @param result Output coefficient value
 * @return 0 on success, negative on error
 */
int poly_get_coeff(const polynomial_t *poly, uint32_t index, bigint_t *result);

// ============================================================================
// Arithmetic Operations
// ============================================================================

/**
 * @brief Add two polynomials: result = a + b (mod modulus)
 * @param result Output polynomial
 * @param a First polynomial
 * @param b Second polynomial
 * @param params Ring parameters
 * @return 0 on success, negative on error
 */
int poly_add(polynomial_t *result, const polynomial_t *a,
             const polynomial_t *b, const poly_ring_params_t *params);

/**
 * @brief Subtract polynomials: result = a - b (mod modulus)
 * @param result Output polynomial
 * @param a First polynomial
 * @param b Second polynomial
 * @param params Ring parameters
 * @return 0 on success, negative on error
 */
int poly_sub(polynomial_t *result, const polynomial_t *a,
             const polynomial_t *b, const poly_ring_params_t *params);

/**
 * @brief Multiply polynomials: result = a * b (mod X^N + 1, modulus)
 * @param result Output polynomial
 * @param a First polynomial
 * @param b Second polynomial
 * @param params Ring parameters
 * @return 0 on success, negative on error
 */
int poly_mult(polynomial_t *result, const polynomial_t *a,
              const polynomial_t *b, const poly_ring_params_t *params);

/**
 * @brief Multiply polynomial by scalar: result = scalar * poly (mod modulus)
 * @param result Output polynomial
 * @param poly Input polynomial
 * @param scalar Scalar value
 * @param params Ring parameters
 * @return 0 on success, negative on error
 */
int poly_mult_scalar(polynomial_t *result, const polynomial_t *poly,
                     const bigint_t *scalar, const poly_ring_params_t *params);

// ============================================================================
// Number Theoretic Transform (NTT) Operations
// ============================================================================

/**
 * @brief Convert polynomial to NTT form for fast multiplication
 * @param poly Polynomial to transform (modified in-place)
 * @param params Ring parameters
 * @return 0 on success, negative on error
 *
 * NTT allows O(N log N) polynomial multiplication instead of O(N^2)
 */
int poly_to_ntt(polynomial_t *poly, const poly_ring_params_t *params);

/**
 * @brief Convert polynomial from NTT form back to coefficient form
 * @param poly Polynomial to transform (modified in-place)
 * @param params Ring parameters
 * @return 0 on success, negative on error
 */
int poly_from_ntt(polynomial_t *poly, const poly_ring_params_t *params);

/**
 * @brief Multiply two polynomials in NTT form (coefficient-wise)
 * @param result Output polynomial (in NTT form)
 * @param a First polynomial (must be in NTT form)
 * @param b Second polynomial (must be in NTT form)
 * @param params Ring parameters
 * @return 0 on success, negative on error
 */
int poly_mult_ntt(polynomial_t *result, const polynomial_t *a,
                  const polynomial_t *b, const poly_ring_params_t *params);

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Reduce polynomial coefficients modulo the coefficient modulus
 * @param poly Polynomial to reduce (modified in-place)
 * @param params Ring parameters containing modulus
 * @return 0 on success, negative on error
 */
int poly_mod_reduce(polynomial_t *poly, const poly_ring_params_t *params);

/**
 * @brief Check if two polynomials are equal
 * @param a First polynomial
 * @param b Second polynomial
 * @return true if equal, false otherwise
 */
bool poly_equals(const polynomial_t *a, const polynomial_t *b);

/**
 * @brief Set polynomial to zero
 * @param poly Polynomial to zero out
 */
void poly_zero(polynomial_t *poly);

/**
 * @brief Get the actual degree of polynomial (highest non-zero coefficient)
 * @param poly Input polynomial
 * @return Degree of polynomial
 */
uint32_t poly_get_degree(const polynomial_t *poly);

#endif /* OPENFHE_CORE_MATH_POLYNOMIAL_H_ */