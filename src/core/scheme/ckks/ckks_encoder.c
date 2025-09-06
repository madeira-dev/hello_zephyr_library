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
int ckks_encode(const ckks_encoder_t *encoder, const double *values, size_t value_count, ckks_plaintext_t *plaintext)
{
  if (!encoder || !values || !plaintext || value_count > encoder->slot_count)
    return -1;

  const ckks_cryptoparams_t *params = encoder->params;
  uint32_t N = params->ring_dimension;
  uint32_t num_moduli = params->num_moduli;

  // Initialize the RNS polynomial inside the plaintext
  if (rns_poly_init(&plaintext->poly, N - 1, num_moduli) != 0)
    return -1;

  // For each modulus in the RNS chain...
  for (uint32_t i = 0; i < num_moduli; i++)
  {
        polynomial_t *poly = &plaintext->poly.polys[i];
        math_word_t modulus = params->poly_params_rns[i].coefficient_modulus;
        poly->modulus = modulus; // Set modulus for the poly

        // Set all coefficients to zero initially
        poly_zero(poly);

        // Encode each value as a scaled integer, reduced modulo the current prime
        for (size_t j = 0; j < value_count; j++)
        {
            double scaled = values[j] * encoder->scaling_factor;
            int64_t rounded = (int64_t)llround(scaled);

            math_word_t coeff;
            if (rounded < 0)
            {
                // Handle negative numbers with modular arithmetic
                coeff = modulus - (math_word_t)(-rounded % modulus);
            }
            else
            {
                coeff = (math_word_t)(rounded % modulus);
            }

            poly_set_coeff(poly, j, coeff);
        }
    }

    return 0;
}