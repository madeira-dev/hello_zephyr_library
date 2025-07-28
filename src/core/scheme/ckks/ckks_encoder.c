#include "scheme/ckks/ckks_encoder.h"
#include <math.h>
#include <string.h>

// Initialize CKKS encoder
int ckks_encoder_init(ckks_encoder_t *encoder, const ckks_cryptoparams_t *params)
{
    if (!encoder || !params)
        return -1;

    encoder->params = params;
    encoder->scaling_factor = params->scaling_factor;
    encoder->slot_count = params->ring_dimension / 2;

    return 0;
}

// Encode an array of doubles into a polynomial (plaintext)
// This is a simple "real-to-coefficient" encoding (no complex packing, no NTT)
// Only supports encoding up to slot_count values
int ckks_encode(const ckks_encoder_t *encoder, const double *values, size_t value_count, polynomial_t *poly)
{
    if (!encoder || !values || !poly || value_count > encoder->slot_count)
        return -1;

    // Initialize polynomial to degree N-1
    uint32_t degree = encoder->params->ring_dimension - 1;
    if (poly_init(poly, degree) != 0)
        return -1;

    // Set all coefficients to zero
    poly_zero(poly);

    // Encode each value as a scaled integer in the first slot_count coefficients
    for (size_t i = 0; i < value_count; i++)
    {
        double scaled = values[i] * encoder->scaling_factor;
        int64_t rounded;

        if (scaled >= 0)
            rounded = (int64_t)(scaled + 0.5);
        else
            rounded = (int64_t)(scaled - 0.5);

        bigint_t coeff;
        bigint_init(&coeff);

        // Set bigint from int64_t
        if (rounded < 0)
        {
            bigint_init_u64(&coeff, (uint64_t)(-rounded));
            coeff.is_negative = true;
        }
        else
        {
            bigint_init_u64(&coeff, (uint64_t)rounded);
        }

        poly_set_coeff(poly, i, &coeff);
    }

    // Remaining coefficients are already zero
    poly->modulus_bits = encoder->params->poly_params.modulus_bits;

    return 0;
}