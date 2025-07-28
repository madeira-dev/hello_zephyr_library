#include "scheme/ckks/ckks_encryptor.h"
#include "core/math/polynomial.h"
#include "core/math/biginteger.h"
#include "core/math/math_hal.h"
#include <string.h>

/**
 * CKKS encryption (encryption-only, no decryption, no homomorphic ops)
 * c[0] = pk[0] * u + Delta * m + e0
 * c[1] = pk[1] * u + e1
 *   - u: random binary polynomial (0/1 coefficients)
 *   - e0, e1: small error polynomials
 *   - m: encoded plaintext polynomial
 *   - Delta: scaling factor (already applied in encoding)
 */
int ckks_encrypt(const ckks_cryptoparams_t *params,
                 const ckks_publickey_t *pk,
                 const ckks_plaintext_t *plaintext,
                 ckks_ciphertext_t *ciphertext)
{
    if (!params || !pk || !plaintext || !ciphertext)
        return -1;

    uint32_t N = params->ring_dimension;
    const poly_ring_params_t *ring = &params->poly_params;

    // 1. Generate random u (binary: 0/1 coefficients)
    polynomial_t u;
    if (poly_init(&u, N - 1) != 0)
        return -1;
    for (uint32_t i = 0; i < N; i++)
    {
        bigint_t bit;
        bigint_init(&bit);
        uint8_t r;
        math_hal_rng_bytes(&r, 1);
        bigint_init_u64(&bit, r & 1);
        poly_set_coeff(&u, i, &bit);
    }

    // 2. Generate error polynomials e0, e1 (small noise: {-1,0,1})
    polynomial_t e0, e1;
    if (poly_init(&e0, N - 1) != 0)
        return -1;
    if (poly_init(&e1, N - 1) != 0)
        return -1;
    for (uint32_t i = 0; i < N; i++)
    {
        // e0
        bigint_t noise0;
        bigint_init(&noise0);
        uint8_t r0;
        math_hal_rng_bytes(&r0, 1);
        int8_t val0 = (r0 % 3) - 1;
        if (val0 < 0)
        {
            bigint_init_u64(&noise0, (uint64_t)(-val0));
            noise0.is_negative = true;
        }
        else
        {
            bigint_init_u64(&noise0, (uint64_t)val0);
        }
        poly_set_coeff(&e0, i, &noise0);

        // e1
        bigint_t noise1;
        bigint_init(&noise1);
        uint8_t r1;
        math_hal_rng_bytes(&r1, 1);
        int8_t val1 = (r1 % 3) - 1;
        if (val1 < 0)
        {
            bigint_init_u64(&noise1, (uint64_t)(-val1));
            noise1.is_negative = true;
        }
        else
        {
            bigint_init_u64(&noise1, (uint64_t)val1);
        }
        poly_set_coeff(&e1, i, &noise1);
    }

    // 3. Compute c[0] = pk[0] * u + m + e0
    polynomial_t pk0u, c0tmp;
    if (poly_init(&pk0u, N - 1) != 0)
        return -1;
    if (poly_mult(&pk0u, &pk->pk[0], &u, ring) != 0)
        return -1;
    if (poly_init(&c0tmp, N - 1) != 0)
        return -1;
    if (poly_add(&c0tmp, &pk0u, &plaintext->poly, ring) != 0)
        return -1;
    if (poly_add(&ciphertext->parts[0], &c0tmp, &e0, ring) != 0)
        return -1;

    // 4. Compute c[1] = pk[1] * u + e1
    polynomial_t pk1u;
    if (poly_init(&pk1u, N - 1) != 0)
        return -1;
    if (poly_mult(&pk1u, &pk->pk[1], &u, ring) != 0)
        return -1;
    if (poly_add(&ciphertext->parts[1], &pk1u, &e1, ring) != 0)
        return -1;

    ciphertext->level = 0;
    ciphertext->scaling_factor = params->scaling_factor;
    ciphertext->depth = 1;

    return 0;
}