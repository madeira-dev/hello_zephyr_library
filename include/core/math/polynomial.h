#ifndef OPENFHE_CORE_MATH_POLYNOMIAL_H_
#define OPENFHE_CORE_MATH_POLYNOMIAL_H_

#include <zephyr/kernel.h>
#include <stdint.h>
#include <stdbool.h>
#include "biginteger.h"
#include "math_hal.h"

// ============================================================================
// Configuration Macros
// ============================================================================

/**
 * @brief Maximum degree and number of coefficients for polynomials.
 *
 * For embedded systems, this is a fixed-size array to avoid dynamic allocation.
 */
#define POLY_MAX_RING_DIMENSION 1024
#define POLY_MAX_DEGREE (POLY_MAX_RING_DIMENSION - 1)
#define POLY_MAX_COEFFS POLY_MAX_RING_DIMENSION

/**
 * @brief Maximum number of moduli in the RNS chain.
 */
#define RNS_MAX_MODULI 4

/**
 * @brief Polynomial structure
 */
typedef struct
{
  math_word_t coeffs[POLY_MAX_COEFFS]; // Coefficients are now native words
  uint32_t degree;
  math_word_t modulus; // Store the modulus directly for faster operations
  bool is_ntt_form;
} polynomial_t;

/**
 * @brief Polynomial ring parameters
 */
typedef struct
{
  uint32_t ring_dimension;
  math_word_t coefficient_modulus; // Use math_word_t for the modulus
  ntt_params_t ntt_params;         // Precomputed NTT parameters for this modulus
} poly_ring_params_t;

/**
 * @brief RNS polynomial: array of polynomials, one per modulus
 */
typedef struct
{
  polynomial_t polys[RNS_MAX_MODULI];
  uint32_t num_moduli;
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
// Function Declarations
// ============================================================================

// --- Basic Polynomial Operations ---
int poly_init(polynomial_t *poly, uint32_t degree, math_word_t modulus);
void poly_cleanup(polynomial_t *poly);
int poly_copy(polynomial_t *dest, const polynomial_t *src);
int poly_set_coeff(polynomial_t *poly, uint32_t index, math_word_t value);
int poly_get_coeff(const polynomial_t *poly, uint32_t index, math_word_t *result);

// --- Arithmetic Operations ---
int poly_add(polynomial_t *result, const polynomial_t *a, const polynomial_t *b, const poly_ring_params_t *params);
int poly_sub(polynomial_t *result, const polynomial_t *a, const polynomial_t *b, const poly_ring_params_t *params);
int poly_mult(polynomial_t *result, const polynomial_t *a, const polynomial_t *b, const poly_ring_params_t *params);
int poly_mult_scalar(polynomial_t *result, const polynomial_t *poly, const bigint_t *scalar, const poly_ring_params_t *params);

// --- RNS Polynomial Operations ---
int rns_poly_init(rns_polynomial_t *rpoly, uint32_t degree, uint32_t num_moduli);
int rns_poly_copy(rns_polynomial_t *dest, const rns_polynomial_t *src);

// --- RNS Arithmetic Operations ---
int rns_poly_add(rns_polynomial_t *result, const rns_polynomial_t *a,
                 const rns_polynomial_t *b, const poly_ring_params_t *params, uint32_t num_moduli);
int rns_poly_sub(rns_polynomial_t *result, const rns_polynomial_t *a,
                 const rns_polynomial_t *b, const poly_ring_params_t *params, uint32_t num_moduli);
int rns_poly_mult(rns_polynomial_t *result, const rns_polynomial_t *a,
                  const rns_polynomial_t *b, const poly_ring_params_t *params, uint32_t num_moduli);

// --- NTT Operations ---
int poly_to_ntt(polynomial_t *poly, const ntt_params_t *ntt_params);
int poly_from_ntt(polynomial_t *poly, const ntt_params_t *ntt_params);
int poly_mult_ntt(polynomial_t *result, const polynomial_t *a,
                  const polynomial_t *b, const ntt_params_t *ntt_params);

// --- Utility Functions ---
int poly_mod_reduce(polynomial_t *poly, const poly_ring_params_t *params);
bool poly_equals(const polynomial_t *a, const polynomial_t *b);
void poly_zero(polynomial_t *poly);
uint32_t poly_get_degree(const polynomial_t *poly);

#endif /* OPENFHE_CORE_MATH_POLYNOMIAL_H_ */