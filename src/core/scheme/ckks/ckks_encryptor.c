#include "scheme/ckks/ckks_encryptor.h"
#include "core/math/polynomial.h"
#include "core/math/biginteger.h"
#include "core/math/math_hal.h"
#include "core/math/dgg.h"
#include <string.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ckks_encrypt_err, LOG_LEVEL_ERR);

/**
 * CKKS encryption:
 * Given plaintext m, public key (pk[0], pk[1]), and parameters:
 *  - Sample random u (binary polynomial)
 *  - Sample error polynomials e0, e1 (small noise)
 *  - c[0] = pk[0]*u + e0 + m
 *  - c[1] = pk[1]*u + e1
 * Output: ciphertext (c[0], c[1])
 */
int ckks_encrypt(const ckks_cryptoparams_t *params,
                 const ckks_publickey_t *pk,
                 const ckks_plaintext_t *plaintext,
                 ckks_ciphertext_t *ciphertext)
{
  if (!params || !pk || !plaintext || !ciphertext)
    return -1;

  uint32_t N = params->ring_dimension;
  uint32_t num_moduli = params->num_moduli;
  const poly_ring_params_t *rns_poly_params = params->poly_params_rns;

  // 1. Sample random u (binary polynomial, but needs to be in RNS format for multiplication)
  // Use static to move large objects from stack to .bss section
  static rns_polynomial_t u_rns;
  if (rns_poly_init(&u_rns, N - 1, num_moduli) != 0)
    return -1;
  for (uint32_t i = 0; i < num_moduli; i++)
  {
    u_rns.polys[i].modulus = rns_poly_params[i].coefficient_modulus;
    for (uint32_t j = 0; j < N; j++)
    {
      uint8_t r;
      math_hal_rng_bytes(&r, 1);
      poly_set_coeff(&u_rns.polys[i], j, r & 1);
    }
  }

  // 2. Sample error polynomials e0, e1 in RNS format (from DGG)
  static rns_polynomial_t e0_rns, e1_rns;
  dgg_sampler_t dgg;
  dgg_init(&dgg, 3.2); // Standard deviation for noise

  if (rns_poly_init(&e0_rns, N - 1, num_moduli) != 0 || rns_poly_init(&e1_rns, N - 1, num_moduli) != 0)
    return -1;

  for (uint32_t i = 0; i < num_moduli; i++)
  {
    math_word_t modulus = rns_poly_params[i].coefficient_modulus;
    e0_rns.polys[i].modulus = modulus;
    e1_rns.polys[i].modulus = modulus;
    for (uint32_t j = 0; j < N; j++)
    {
      int32_t val0 = dgg_generate_integer(&dgg);
      int32_t val1 = dgg_generate_integer(&dgg);

      math_word_t noise0 = (val0 < 0) ? (modulus - (math_word_t)(-val0)) : (math_word_t)val0;
      math_word_t noise1 = (val1 < 0) ? (modulus - (math_word_t)(-val1)) : (math_word_t)val1;

      poly_set_coeff(&e0_rns.polys[i], j, noise0);
      poly_set_coeff(&e1_rns.polys[i], j, noise1);
    }
  }

  // 3. Compute c[0] = pk[0]*u + e0 + m (all in RNS)
  static rns_polynomial_t pk0u_rns, temp_rns;
  if (rns_poly_init(&pk0u_rns, N - 1, num_moduli) != 0 || rns_poly_init(&temp_rns, N - 1, num_moduli) != 0)
    return -1;

  // pk[0] * u
  rns_poly_mult(&pk0u_rns, &pk->pk[0], &u_rns, rns_poly_params, num_moduli);
  // (pk[0] * u) + e0
  rns_poly_add(&temp_rns, &pk0u_rns, &e0_rns, rns_poly_params, num_moduli);
  // ((pk[0] * u) + e0) + m
  rns_poly_add(&ciphertext->parts[0], &temp_rns, &plaintext->poly, rns_poly_params, num_moduli);

  // 4. Compute c[1] = pk[1]*u + e1 (all in RNS)
  static rns_polynomial_t pk1u_rns;
  if (rns_poly_init(&pk1u_rns, N - 1, num_moduli) != 0)
    return -1;

  // pk[1] * u
  rns_poly_mult(&pk1u_rns, &pk->pk[1], &u_rns, rns_poly_params, num_moduli);
  // (pk[1] * u) + e1
  rns_poly_add(&ciphertext->parts[1], &pk1u_rns, &e1_rns, rns_poly_params, num_moduli);

  // 5. Set ciphertext metadata
  ciphertext->level = 0;
  ciphertext->scaling_factor = params->scaling_factor;
  ciphertext->depth = 1;

  return 0;
}