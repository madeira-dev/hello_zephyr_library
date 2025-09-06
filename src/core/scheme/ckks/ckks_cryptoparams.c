#include <string.h>
#include "scheme/ckks/ckks_cryptoparams.h"
#include "core/math/prime_utils.h"

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

  // Set up polynomial ring parameters for each modulus in the RNS chain
  for (uint32_t i = 0; i < params->num_moduli; i++)
  {
    params->poly_params_rns[i].ring_dimension = ring_dimension;
    // This assumes the bigint modulus fits into a math_word_t, which is true for our NTT-friendly primes.
    params->poly_params_rns[i].coefficient_modulus = (math_word_t)params->modulus_chain[i].words[0];

    // Precompute NTT parameters for this modulus
    if (math_hal_ntt_init_params(&params->poly_params_rns[i].ntt_params,
                                 ring_dimension,
                                 params->poly_params_rns[i].coefficient_modulus) != 0)
    {
      // Failed to initialize NTT params, this modulus is not suitable
      return -1;
    }
  }

  return 0;
}

// Clean up CKKS crypto parameters
void ckks_cryptoparams_cleanup(ckks_cryptoparams_t *params)
{
  if (!params)
    return;

  // Use secure zero to clear sensitive parameter data
  math_hal_secure_zero(params, sizeof(ckks_cryptoparams_t));
}