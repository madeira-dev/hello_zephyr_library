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
    if (!params || !modulus_chain || num_moduli == 0 || num_moduli > CKKS_MAX_MODULI)
        return -1;

    params->ring_dimension = ring_dimension;
    params->num_moduli = num_moduli;
    params->scaling_factor = scaling_factor;
    params->max_depth = max_depth;

    // Copy modulus chain and compute modulus bits
    for (uint32_t i = 0; i < num_moduli; i++)
    {
        bigint_copy(&params->modulus_chain[i], &modulus_chain[i]);
        params->modulus_bits[i] = bigint_bit_count(&modulus_chain[i]);
    }
    // Zero unused modulus slots
    for (uint32_t i = num_moduli; i < CKKS_MAX_MODULI; i++)
    {
        bigint_zero(&params->modulus_chain[i]);
        params->modulus_bits[i] = 0;
    }

    // Initialize polynomial ring parameters for the first modulus in the chain
    params->poly_params.ring_dimension = ring_dimension;
    if (num_moduli > 0)
    {
        bigint_copy(&params->poly_params.coefficient_modulus, &modulus_chain[0]);
        params->poly_params.modulus_bits = params->modulus_bits[0];
    }
    else
    {
        bigint_zero(&params->poly_params.coefficient_modulus);
        params->poly_params.modulus_bits = 0;
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