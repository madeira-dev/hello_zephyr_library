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

#define POLY_MAX_DEGREE 64
#define POLY_MAX_COEFFS (POLY_MAX_DEGREE + 1)
#define CKKS_MAX_MODULI 2
#define RNS_MAX_MODULI CKKS_MAX_MODULI

// ============================================================================
// Type Definitions
// ============================================================================

/**
 * @brief Polynomial structure
 */
typedef struct
{
    bigint_t coeffs[POLY_MAX_COEFFS];
    uint32_t degree;
    uint32_t modulus_bits;
    bool is_ntt_form;
} polynomial_t;

/**
 * @brief Polynomial ring parameters
 */
typedef struct
{
    uint32_t ring_dimension;
    bigint_t coefficient_modulus;
    uint32_t modulus_bits;
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
int poly_init(polynomial_t *poly, uint32_t degree);
void poly_cleanup(polynomial_t *poly);
int poly_copy(polynomial_t *dest, const polynomial_t *src);
int poly_set_coeff(polynomial_t *poly, uint32_t index, const bigint_t *value);
int poly_get_coeff(const polynomial_t *poly, uint32_t index, bigint_t *result);

// --- Arithmetic Operations ---
int poly_add(polynomial_t *result, const polynomial_t *a, const polynomial_t *b, const poly_ring_params_t *params);
int poly_sub(polynomial_t *result, const polynomial_t *a, const polynomial_t *b, const poly_ring_params_t *params);
int poly_mult(polynomial_t *result, const polynomial_t *a, const polynomial_t *b, const poly_ring_params_t *params);
int poly_mult_scalar(polynomial_t *result, const polynomial_t *poly, const bigint_t *scalar, const poly_ring_params_t *params);

// --- RNS Polynomial Operations ---
int rns_poly_init(rns_polynomial_t *rpoly, uint32_t degree, uint32_t num_moduli);
int rns_poly_copy(rns_polynomial_t *dest, const rns_polynomial_t *src);
int rns_poly_to_ntt(rns_polynomial_t *rpoly, const rns_ntt_params_t *ntt_params);
int rns_poly_from_ntt(rns_polynomial_t *rpoly, const rns_ntt_params_t *ntt_params);
int rns_poly_add(rns_polynomial_t *result, const rns_polynomial_t *a, const rns_polynomial_t *b, const poly_ring_params_t *params, uint32_t num_moduli);
int rns_poly_mult(rns_polynomial_t *result, const rns_polynomial_t *a, const rns_polynomial_t *b, const poly_ring_params_t *params, uint32_t num_moduli);

// --- NTT Operations ---
int poly_to_ntt(polynomial_t *poly, const poly_ring_params_t *params);
int poly_from_ntt(polynomial_t *poly, const poly_ring_params_t *params);
int poly_mult_ntt(polynomial_t *result, const polynomial_t *a, const polynomial_t *b, const poly_ring_params_t *params);

// --- Utility Functions ---
int poly_mod_reduce(polynomial_t *poly, const poly_ring_params_t *params);
bool poly_equals(const polynomial_t *a, const polynomial_t *b);
void poly_zero(polynomial_t *poly);
uint32_t poly_get_degree(const polynomial_t *poly);

#endif /* OPENFHE_CORE_MATH_POLYNOMIAL_H_ */