#include "scheme/ckks/ckks_keygen.h"
#include "core/math/polynomial.h"
#include "core/math/biginteger.h"
#include "core/math/math_hal.h"
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
    const poly_ring_params_t *ring = &params->poly_params;

    // 1. Generate secret key s (binary: 0/1 coefficients)
    polynomial_t s;
    if (poly_init(&s, N - 1) != 0)
        return -1;
    for (uint32_t i = 0; i < N; i++)
    {
        bigint_t bit;
        bigint_init(&bit);
        uint8_t r;
        math_hal_rng_bytes(&r, 1);
        bigint_init_u64(&bit, r & 1);
        poly_set_coeff(&s, i, &bit);
    }
    if (sk)
    {
        poly_copy(&sk->sk, &s);
    }

    // 2. Generate random a (uniform in modulus)
    polynomial_t a;
    if (poly_init(&a, N - 1) != 0)
        return -1;
    for (uint32_t i = 0; i < N; i++)
    {
        bigint_t coeff;
        bigint_random_mod(&coeff, &ring->coefficient_modulus);
        poly_set_coeff(&a, i, &coeff);
    }

    // 3. Generate error polynomial e (small noise, e.g., {-1, 0, 1})
    polynomial_t e;
    if (poly_init(&e, N - 1) != 0)
        return -1;
    for (uint32_t i = 0; i < N; i++)
    {
        bigint_t noise;
        bigint_init(&noise);
        uint8_t r;
        math_hal_rng_bytes(&r, 1);
        int8_t val = (r % 3) - 1; // -1, 0, or 1
        if (val < 0)
        {
            bigint_init_u64(&noise, (uint64_t)(-val));
            noise.is_negative = true;
        }
        else
        {
            bigint_init_u64(&noise, (uint64_t)val);
        }
        poly_set_coeff(&e, i, &noise);
    }

    // 4. Compute pk[1] = a
    poly_copy(&pk->pk[1], &a);

    // 5. Compute pk[0] = -a * s + e
    polynomial_t a_times_s;
    if (poly_init(&a_times_s, N - 1) != 0)
        return -1;
    poly_mult(&a_times_s, &a, &s, ring);

    // Negate a_times_s
    for (uint32_t i = 0; i < N; i++)
    {
        pk->pk[0].coeffs[i] = a_times_s.coeffs[i];
        pk->pk[0].coeffs[i].is_negative = !pk->pk[0].coeffs[i].is_negative;
    }
    pk->pk[0].degree = N - 1;
    pk->pk[0].modulus_bits = ring->modulus_bits;

    // Add error polynomial: pk[0] = -a*s + e
    poly_add(&pk->pk[0], &pk->pk[0], &e, ring);

    pk->pk[1].modulus_bits = ring->modulus_bits;

    return 0;
}