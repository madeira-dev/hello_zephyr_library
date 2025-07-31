#include "scheme/ckks/ckks_cryptoparams.h"
#include <string.h>

// Initialize CKKS crypto parameters
int ckks_cryptoparams_init(
    ckks_cryptoparams_t *params,
    uint32_t ring_dimension,
    const bigint_t *modulus_chain,
    uint32_t num_moduli,
    double scaling_factor,
    uint32_t max_depth)
{
    if (!params)
        return -1;

    params->ring_dimension = ring_dimension;
    params->scaling_factor = scaling_factor;
    params->max_depth = max_depth;

    // If caller passed num_moduli == 0, auto-generate NTT-friendly primes
    if (num_moduli == 0)
    {
        const uint32_t desired_count = 1;
        math_word_t start = (math_word_t)(1ULL << 30); // initial guess
        for (uint32_t i = 0; i < desired_count; i++)
        {
            math_word_t q = find_next_prime_1_mod_2N(start, ring_dimension);
            bigint_init_u64(&params->modulus_chain[i], q);
            params->modulus_bits[i] = bigint_bit_count(&params->modulus_chain[i]);
            start = q + (2 * ring_dimension);
        }
        params->num_moduli = desired_count;
    }
    else
    {
        // Use provided chain
        params->num_moduli = num_moduli;
        for (uint32_t i = 0; i < num_moduli; i++)
        {
            bigint_copy(&params->modulus_chain[i], &modulus_chain[i]);
            params->modulus_bits[i] = bigint_bit_count(&modulus_chain[i]);
        }
    }

    // Zero out leftover slots
    for (uint32_t i = params->num_moduli; i < CKKS_MAX_MODULI; i++)
    {
        bigint_zero(&params->modulus_chain[i]);
        params->modulus_bits[i] = 0;
    }

    // Set coefficient_modulus to first prime
    params->poly_params.ring_dimension = ring_dimension;
    if (params->num_moduli > 0)
    {
        bigint_copy(&params->poly_params.coefficient_modulus,
                    &params->modulus_chain[0]);
        params->poly_params.modulus_bits = params->modulus_bits[0];
    }

    return 0;
}

// Clean up CKKS crypto parameters
void ckks_cryptoparams_cleanup(ckks_cryptoparams_t *params)
{
    if (!params)
        return;

    for (uint32_t i = 0; i < CKKS_MAX_MODULI; i++)
    {
        bigint_zero(&params->modulus_chain[i]);
        params->modulus_bits[i] = 0;
    }
    params->ring_dimension = 0;
    params->num_moduli = 0;
    params->scaling_factor = 0.0;
    params->max_depth = 0;
    bigint_zero(&params->poly_params.coefficient_modulus);
    params->poly_params.ring_dimension = 0;
    params->poly_params.modulus_bits = 0;
}