#include "string.h"
#include "core/math/polynomial.h"
#include "core/math/math_hal.h"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(polynomial, LOG_LEVEL_DBG);

// ============================================================================
// Basic Polynomial Operations
// ============================================================================

int poly_init(polynomial_t *poly, uint32_t degree)
{
    if (!poly)
        return -1;

    if (degree >= POLY_MAX_DEGREE)
    {
        LOG_ERR("Requested degree %u exceeds maximum %u", degree, POLY_MAX_DEGREE);
        return -1;
    }

    // Initialize all coefficients to zero
    for (uint32_t i = 0; i <= degree; i++)
    {
        bigint_init(&poly->coeffs[i]);
    }

    poly->degree = degree;
    poly->modulus_bits = 0;
    poly->is_ntt_form = false;

    LOG_DBG("Initialized polynomial with degree %u", degree);
    return 0;
}

void poly_cleanup(polynomial_t *poly)
{
    if (!poly)
        return;

    // In our simple implementation, no dynamic memory to free
    // But we can zero out for security
    for (uint32_t i = 0; i <= poly->degree && i < POLY_MAX_COEFFS; i++)
    {
        bigint_zero(&poly->coeffs[i]);
    }

    poly->degree = 0;
    poly->is_ntt_form = false;
}

int poly_copy(polynomial_t *dest, const polynomial_t *src)
{
    if (!dest || !src)
        return -1;

    if (src->degree >= POLY_MAX_DEGREE)
        return -1;

    // Copy all coefficients
    for (uint32_t i = 0; i <= src->degree; i++)
    {
        bigint_copy(&dest->coeffs[i], &src->coeffs[i]);
    }

    dest->degree = src->degree;
    dest->modulus_bits = src->modulus_bits;
    dest->is_ntt_form = src->is_ntt_form;

    return 0;
}

int poly_set_coeff(polynomial_t *poly, uint32_t index, const bigint_t *value)
{
    if (!poly || !value)
        return -1;

    if (index > poly->degree || index >= POLY_MAX_COEFFS)
    {
        LOG_ERR("Coefficient index %u out of bounds (degree=%u)", index, poly->degree);
        return -1;
    }

    bigint_copy(&poly->coeffs[index], value);
    return 0;
}

int poly_get_coeff(const polynomial_t *poly, uint32_t index, bigint_t *result)
{
    if (!poly || !result)
        return -1;

    if (index > poly->degree || index >= POLY_MAX_COEFFS)
    {
        LOG_ERR("Coefficient index %u out of bounds (degree=%u)", index, poly->degree);
        return -1;
    }

    bigint_copy(result, &poly->coeffs[index]);
    return 0;
}

// ============================================================================
// RNS Polynomial Operations
// ============================================================================

int rns_poly_init(rns_polynomial_t *rpoly, uint32_t degree, uint32_t num_moduli)
{
    if (!rpoly || num_moduli == 0 || num_moduli > RNS_MAX_MODULI)
        return -1;

    rpoly->num_moduli = num_moduli;
    for (uint32_t i = 0; i < num_moduli; i++)
    {
        if (poly_init(&rpoly->polys[i], degree) != 0)
            return -1;
    }
    return 0;
}

int rns_poly_copy(rns_polynomial_t *dest, const rns_polynomial_t *src)
{
    if (!dest || !src || src->num_moduli > RNS_MAX_MODULI)
        return -1;

    dest->num_moduli = src->num_moduli;
    for (uint32_t i = 0; i < src->num_moduli; i++)
    {
        if (poly_copy(&dest->polys[i], &src->polys[i]) != 0)
            return -1;
    }
    return 0;
}

int rns_poly_to_ntt(rns_polynomial_t *rpoly, const rns_ntt_params_t *ntt_params)
{
    if (!rpoly || !ntt_params || rpoly->num_moduli != ntt_params->num_moduli)
        return -1;

    for (uint32_t i = 0; i < rpoly->num_moduli; i++)
    {
        // Each modulus may have its own NTT params
        if (poly_to_ntt(&rpoly->polys[i], (const poly_ring_params_t *)&ntt_params->ntt_params[i]) != 0)
            return -1;
    }
    return 0;
}

int rns_poly_from_ntt(rns_polynomial_t *rpoly, const rns_ntt_params_t *ntt_params)
{
    if (!rpoly || !ntt_params || rpoly->num_moduli != ntt_params->num_moduli)
        return -1;

    for (uint32_t i = 0; i < rpoly->num_moduli; i++)
    {
        if (poly_from_ntt(&rpoly->polys[i], (const poly_ring_params_t *)&ntt_params->ntt_params[i]) != 0)
            return -1;
    }
    return 0;
}

int rns_poly_add(rns_polynomial_t *result, const rns_polynomial_t *a,
                 const rns_polynomial_t *b, const poly_ring_params_t *params, uint32_t num_moduli)
{
    if (!result || !a || !b || !params || num_moduli == 0 || num_moduli > RNS_MAX_MODULI)
        return -1;

    result->num_moduli = num_moduli;
    for (uint32_t i = 0; i < num_moduli; i++)
    {
        if (poly_add(&result->polys[i], &a->polys[i], &b->polys[i], &params[i]) != 0)
            return -1;
    }
    return 0;
}

int rns_poly_mult(rns_polynomial_t *result, const rns_polynomial_t *a,
                  const rns_polynomial_t *b, const poly_ring_params_t *params, uint32_t num_moduli)
{
    if (!result || !a || !b || !params || num_moduli == 0 || num_moduli > RNS_MAX_MODULI)
        return -1;

    result->num_moduli = num_moduli;
    for (uint32_t i = 0; i < num_moduli; i++)
    {
        if (poly_mult(&result->polys[i], &a->polys[i], &b->polys[i], &params[i]) != 0)
            return -1;
    }
    return 0;
}

// ============================================================================
// Arithmetic Operations
// ============================================================================

int poly_add(polynomial_t *result, const polynomial_t *a,
             const polynomial_t *b, const poly_ring_params_t *params)
{
    if (!result || !a || !b || !params)
        return -1;

    uint32_t max_degree = (a->degree > b->degree) ? a->degree : b->degree;

    if (max_degree >= POLY_MAX_DEGREE)
        return -1;

    // Initialize result polynomial
    if (poly_init(result, max_degree) != 0)
        return -1;

    result->modulus_bits = params->modulus_bits;

    // Add coefficients
    for (uint32_t i = 0; i <= max_degree; i++)
    {
        bigint_t coeff_a, coeff_b, sum;

        bigint_init(&coeff_a);
        bigint_init(&coeff_b);
        bigint_init(&sum);

        // Get coefficient from a (or 0 if beyond degree)
        if (i <= a->degree)
        {
            bigint_copy(&coeff_a, &a->coeffs[i]);
        }

        // Get coefficient from b (or 0 if beyond degree)
        if (i <= b->degree)
        {
            bigint_copy(&coeff_b, &b->coeffs[i]);
        }

        // Add coefficients
        if (bigint_add(&sum, &coeff_a, &coeff_b) != 0)
        {
            LOG_ERR("Failed to add coefficients at index %u", i);
            return -1;
        }

        // Set result coefficient (modular reduction would go here)
        bigint_copy(&result->coeffs[i], &sum);
    }

    LOG_DBG("Added polynomials: degree_a=%u, degree_b=%u, result_degree=%u",
            a->degree, b->degree, result->degree);

    return 0;
}

int poly_sub(polynomial_t *result, const polynomial_t *a,
             const polynomial_t *b, const poly_ring_params_t *params)
{
    if (!result || !a || !b || !params)
        return -1;

    uint32_t max_degree = (a->degree > b->degree) ? a->degree : b->degree;

    if (max_degree >= POLY_MAX_DEGREE)
        return -1;

    // Initialize result polynomial
    if (poly_init(result, max_degree) != 0)
        return -1;

    result->modulus_bits = params->modulus_bits;

    // Subtract coefficients
    for (uint32_t i = 0; i <= max_degree; i++)
    {
        bigint_t coeff_a, coeff_b, diff;

        bigint_init(&coeff_a);
        bigint_init(&coeff_b);
        bigint_init(&diff);

        // Get coefficient from a (or 0 if beyond degree)
        if (i <= a->degree)
        {
            bigint_copy(&coeff_a, &a->coeffs[i]);
        }

        // Get coefficient from b (or 0 if beyond degree)
        if (i <= b->degree)
        {
            bigint_copy(&coeff_b, &b->coeffs[i]);
        }

        // Subtract coefficients (simplified - would need proper modular arithmetic)
        if (bigint_compare(&coeff_a, &coeff_b) >= 0)
        {
            if (bigint_sub(&diff, &coeff_a, &coeff_b) != 0)
            {
                LOG_ERR("Failed to subtract coefficients at index %u", i);
                return -1;
            }
        }
        else
        {
            // Would need to add modulus here for proper modular subtraction
            bigint_zero(&diff);
        }

        // Set result coefficient
        bigint_copy(&result->coeffs[i], &diff);
    }

    return 0;
}

int poly_mult(polynomial_t *result, const polynomial_t *a,
              const polynomial_t *b, const poly_ring_params_t *params)
{
    if (!result || !a || !b || !params)
        return -1;

    uint32_t result_degree = a->degree + b->degree;

    // For ring polynomial multiplication, degree should be reduced mod (X^N + 1)
    if (result_degree >= params->ring_dimension)
    {
        result_degree = params->ring_dimension - 1;
    }

    if (result_degree >= POLY_MAX_DEGREE)
        return -1;

    // Initialize result polynomial
    if (poly_init(result, result_degree) != 0)
        return -1;

    result->modulus_bits = params->modulus_bits;

    // Simple schoolbook multiplication
    for (uint32_t i = 0; i <= a->degree; i++)
    {
        for (uint32_t j = 0; j <= b->degree; j++)
        {
            uint32_t k = i + j;

            // Handle ring reduction: X^N = -1, so X^(N+k) = -X^k
            bool negate = false;
            if (k >= params->ring_dimension)
            {
                k = k - params->ring_dimension;
                negate = true;
            }

            if (k <= result->degree)
            {
                bigint_t prod, current;
                bigint_init(&prod);
                bigint_init(&current);

                // Multiply coefficients
                if (bigint_mult(&prod, &a->coeffs[i], &b->coeffs[j]) != 0)
                {
                    LOG_ERR("Failed to multiply coefficients");
                    return -1;
                }

                // Handle negation for ring reduction
                if (negate)
                {
                    prod.is_negative = !prod.is_negative;
                }

                // Add to existing coefficient
                bigint_copy(&current, &result->coeffs[k]);
                if (bigint_add(&result->coeffs[k], &current, &prod) != 0)
                {
                    LOG_ERR("Failed to accumulate product");
                    return -1;
                }
            }
        }
    }

    LOG_DBG("Multiplied polynomials in ring Z[X]/(X^%u + 1)", params->ring_dimension);

    return 0;
}

int poly_mult_scalar(polynomial_t *result, const polynomial_t *poly,
                     const bigint_t *scalar, const poly_ring_params_t *params)
{
    if (!result || !poly || !scalar || !params)
        return -1;

    // Initialize result polynomial
    if (poly_init(result, poly->degree) != 0)
        return -1;

    result->modulus_bits = params->modulus_bits;

    // Multiply each coefficient by scalar
    for (uint32_t i = 0; i <= poly->degree; i++)
    {
        if (bigint_mult(&result->coeffs[i], &poly->coeffs[i], scalar) != 0)
        {
            LOG_ERR("Failed to multiply coefficient %u by scalar", i);
            return -1;
        }
    }

    return 0;
}

// ============================================================================
// NTT Operations
// ============================================================================

// Helper: Convert polynomial coefficients to/from math_word_t arrays
static int poly_coeffs_to_words(const polynomial_t *poly, math_word_t *words, uint32_t n)
{
    if (!poly || !words || n == 0)
        return -1;
    for (uint32_t i = 0; i < n; i++)
    {
        // Only support small moduli that fit in math_word_t
        if (i <= poly->degree)
            words[i] = (math_word_t)(poly->coeffs[i].words[0]);
        else
            words[i] = 0;
    }
    return 0;
}

static int poly_words_to_coeffs(polynomial_t *poly, const math_word_t *words, uint32_t n)
{
    if (!poly || !words || n == 0)
        return -1;
    for (uint32_t i = 0; i < n; i++)
    {
        bigint_init_u64(&poly->coeffs[i], words[i]);
    }
    return 0;
}

// NTT: Convert polynomial to NTT form
int poly_to_ntt(polynomial_t *poly, const poly_ring_params_t *params)
{
    if (!poly || !params)
        return -1;

    ntt_params_t ntt_params;
    math_word_t modulus = (math_word_t)(params->coefficient_modulus.words[0]);
    uint32_t n = params->ring_dimension;

    if (math_hal_ntt_init_params(&ntt_params, n, modulus) != 0)
        return -1;

    math_word_t data[POLY_MAX_COEFFS] = {0};
    if (poly_coeffs_to_words(poly, data, n) != 0)
        return -1;

    if (math_hal_ntt_forward(data, &ntt_params) != 0)
        return -1;

    if (poly_words_to_coeffs(poly, data, n) != 0)
        return -1;

    poly->is_ntt_form = true;
    return 0;
}

// NTT: Convert polynomial from NTT form to coefficient form
int poly_from_ntt(polynomial_t *poly, const poly_ring_params_t *params)
{
    if (!poly || !params)
        return -1;

    ntt_params_t ntt_params;
    math_word_t modulus = (math_word_t)(params->coefficient_modulus.words[0]);
    uint32_t n = params->ring_dimension;

    if (math_hal_ntt_init_params(&ntt_params, n, modulus) != 0)
        return -1;

    math_word_t data[POLY_MAX_COEFFS] = {0};
    if (poly_coeffs_to_words(poly, data, n) != 0)
        return -1;

    if (math_hal_ntt_inverse(data, &ntt_params) != 0)
        return -1;

    if (poly_words_to_coeffs(poly, data, n) != 0)
        return -1;

    poly->is_ntt_form = false;
    return 0;
}

// NTT: Multiply two polynomials in NTT form (coefficient-wise)
int poly_mult_ntt(polynomial_t *result, const polynomial_t *a,
                  const polynomial_t *b, const poly_ring_params_t *params)
{
    if (!result || !a || !b || !params)
        return -1;

    if (!a->is_ntt_form || !b->is_ntt_form)
        return -1;

    ntt_params_t ntt_params;
    math_word_t modulus = (math_word_t)(params->coefficient_modulus.words[0]);
    uint32_t n = params->ring_dimension;

    if (math_hal_ntt_init_params(&ntt_params, n, modulus) != 0)
        return -1;

    math_word_t data_a[POLY_MAX_COEFFS] = {0};
    math_word_t data_b[POLY_MAX_COEFFS] = {0};
    math_word_t data_res[POLY_MAX_COEFFS] = {0};

    if (poly_coeffs_to_words(a, data_a, n) != 0)
        return -1;
    if (poly_coeffs_to_words(b, data_b, n) != 0)
        return -1;

    if (math_hal_ntt_mult(data_res, data_a, data_b, &ntt_params) != 0)
        return -1;

    if (poly_words_to_coeffs(result, data_res, n) != 0)
        return -1;

    result->degree = n - 1;
    result->modulus_bits = params->modulus_bits;
    result->is_ntt_form = true;
    return 0;
}

// Modular reduction: Reduce all coefficients modulo the coefficient modulus
int poly_mod_reduce(polynomial_t *poly, const poly_ring_params_t *params)
{
    if (!poly || !params)
        return -1;

    for (uint32_t i = 0; i <= poly->degree; i++)
    {
        bigint_mod(&poly->coeffs[i], &poly->coeffs[i], &params->coefficient_modulus);
    }
    return 0;
}

// ============================================================================
// Utility Functions
// ============================================================================

bool poly_equals(const polynomial_t *a, const polynomial_t *b)
{
    if (!a || !b)
        return false;

    if (a->degree != b->degree)
        return false;

    for (uint32_t i = 0; i <= a->degree; i++)
    {
        if (!bigint_equals(&a->coeffs[i], &b->coeffs[i]))
        {
            return false;
        }
    }

    return true;
}

void poly_zero(polynomial_t *poly)
{
    if (!poly)
        return;

    for (uint32_t i = 0; i <= poly->degree && i < POLY_MAX_COEFFS; i++)
    {
        bigint_zero(&poly->coeffs[i]);
    }
}

uint32_t poly_get_degree(const polynomial_t *poly)
{
    if (!poly)
        return 0;

    // Find the highest non-zero coefficient
    for (int i = poly->degree; i >= 0; i--)
    {
        if (!bigint_is_zero(&poly->coeffs[i]))
        {
            return i;
        }
    }

    return 0; // Polynomial is zero
}