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
// NTT Operations (Stubs)
// ============================================================================

int poly_to_ntt(polynomial_t *poly, const poly_ring_params_t *params)
{
    if (!poly || !params)
        return -1;

    // Stub implementation - would convert to NTT form
    poly->is_ntt_form = true;

    LOG_DBG("Converted polynomial to NTT form (stub)");
    return 0;
}

int poly_from_ntt(polynomial_t *poly, const poly_ring_params_t *params)
{
    if (!poly || !params)
        return -1;

    // Stub implementation - would convert from NTT form
    poly->is_ntt_form = false;

    LOG_DBG("Converted polynomial from NTT form (stub)");
    return 0;
}

int poly_mult_ntt(polynomial_t *result, const polynomial_t *a,
                  const polynomial_t *b, const poly_ring_params_t *params)
{
    if (!result || !a || !b || !params)
        return -1;

    if (!a->is_ntt_form || !b->is_ntt_form)
    {
        LOG_ERR("Polynomials must be in NTT form for NTT multiplication");
        return -1;
    }

    // Stub implementation - would do coefficient-wise multiplication
    if (poly_init(result, (a->degree > b->degree) ? a->degree : b->degree) != 0)
    {
        return -1;
    }

    result->is_ntt_form = true;
    result->modulus_bits = params->modulus_bits;

    // Coefficient-wise multiplication in NTT domain
    for (uint32_t i = 0; i <= result->degree; i++)
    {
        bigint_t coeff_a, coeff_b;
        bigint_init(&coeff_a);
        bigint_init(&coeff_b);

        if (i <= a->degree)
            bigint_copy(&coeff_a, &a->coeffs[i]);
        if (i <= b->degree)
            bigint_copy(&coeff_b, &b->coeffs[i]);

        if (bigint_mult(&result->coeffs[i], &coeff_a, &coeff_b) != 0)
        {
            return -1;
        }
    }

    LOG_DBG("Performed NTT multiplication (stub)");
    return 0;
}

// ============================================================================
// Utility Functions
// ============================================================================

int poly_mod_reduce(polynomial_t *poly, const poly_ring_params_t *params)
{
    if (!poly || !params)
        return -1;

    // Stub implementation - would reduce coefficients modulo the coefficient modulus
    LOG_DBG("Performed modular reduction on polynomial (stub)");
    return 0;
}

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