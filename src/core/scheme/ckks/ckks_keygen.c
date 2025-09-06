#include "scheme/ckks/ckks_keygen.h"
#include "core/math/polynomial.h"
#include "core/math/biginteger.h"
#include "core/math/math_hal.h"
#include "core/math/dgg.h"
#include <string.h>

/**
 * Generate a CKKS key pair (public and secret key).
 * For embedded: secret key is optional (can be NULL).
 * Public key: pk[0] = -a * s + e, pk[1] = a
 *   - a: random polynomial
 *   - s: secret key (random binary polynomial)
 *   - e: error polynomial (small noise)
 */
int ckks_keygen(const ckks_cryptoparams_t *params, ckks_publickey_t *pk, ckks_secretkey_t *sk)
{
  if (!params || !pk)
    return -1;

  uint32_t N = params->ring_dimension;
  const poly_ring_params_t *rns_poly_params = params->poly_params_rns;
  uint32_t num_moduli = params->num_moduli;

  // 1. Generate secret key s (binary: 0/1 coefficients)
  // The secret key is a single polynomial, not in RNS format.
  // Its modulus is not critical as it's used in RNS multiplication context.
  static polynomial_t s;
  if (poly_init(&s, N - 1, 0) != 0)
    return -1;
  for (uint32_t i = 0; i < N; i++)
  {
    uint8_t r;
    math_hal_rng_bytes(&r, 1);
    poly_set_coeff(&s, i, r & 1);
  }
  if (sk)
  {
    poly_copy(&sk->sk, &s);
  }

  // 2. Generate random 'a' in RNS format (uniform in each modulus)
  // Use static to move large objects from stack to .bss section
  static rns_polynomial_t a_rns;
  if (rns_poly_init(&a_rns, N - 1, num_moduli) != 0)
    return -1;

  for (uint32_t i = 0; i < num_moduli; i++)
  {
    math_word_t modulus = rns_poly_params[i].coefficient_modulus;
    a_rns.polys[i].modulus = modulus; // Set modulus for the poly
    for (uint32_t j = 0; j < N; j++)
    {
      math_word_t coeff = math_hal_rng_mod(modulus);
      poly_set_coeff(&a_rns.polys[i], j, coeff);
    }
  }

  // 3. Generate error polynomial e in RNS format (small noise)
  static rns_polynomial_t e_rns;
  dgg_sampler_t dgg;
  dgg_init(&dgg, 3.2); // Standard deviation for noise

  if (rns_poly_init(&e_rns, N - 1, num_moduli) != 0)
    return -1;
  for (uint32_t i = 0; i < num_moduli; i++)
  {
    math_word_t modulus = rns_poly_params[i].coefficient_modulus;
    e_rns.polys[i].modulus = modulus; // Set modulus for the poly
    for (uint32_t j = 0; j < N; j++)
    {
      int32_t val = dgg_generate_integer(&dgg);
      math_word_t noise = (val < 0) ? (modulus - (math_word_t)(-val)) : (math_word_t)val;
      poly_set_coeff(&e_rns.polys[i], j, noise);
    }
  }

  // 4. Compute pk[1] = a (in RNS format)
  rns_poly_copy(&pk->pk[1], &a_rns);

  // 5. Compute pk[0] = -a * s + e (in RNS format)
  static rns_polynomial_t a_times_s_rns;
  if (rns_poly_init(&a_times_s_rns, N - 1, num_moduli) != 0)
    return -1;

  // Multiply each RNS component of 'a' by the single polynomial 's'
  for (uint32_t i = 0; i < num_moduli; i++)
  {
    // This multiplication is special: RNS poly * single poly
    // We need a helper or do it manually. For now, let's assume poly_mult handles it.
    // We need to ensure 's' coefficients are reduced mod the current modulus.
    static polynomial_t s_mod;
    poly_copy(&s_mod, &s);
    s_mod.modulus = rns_poly_params[i].coefficient_modulus;
    for (uint32_t k = 0; k < N; k++)
    {
      s_mod.coeffs[k] %= s_mod.modulus;
    }
    poly_mult(&a_times_s_rns.polys[i], &a_rns.polys[i], &s_mod, &rns_poly_params[i]);
  }

  // Negate a_times_s and add error: pk[0] = e - (a*s)
  rns_poly_sub(&pk->pk[0], &e_rns, &a_times_s_rns, rns_poly_params, num_moduli);

  return 0;
}