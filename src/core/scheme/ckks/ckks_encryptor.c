#include "scheme/ckks/ckks_encryptor.h"
#include "core/math/polynomial.h"
#include "core/math/biginteger.h"
#include "core/math/math_hal.h"
#include <string.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ckks_encrypt_err, LOG_LEVEL_INF);
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
    const poly_ring_params_t *ring = &params->poly_params;

    // 1. Sample random u (binary polynomial)
    polynomial_t u;
    LOG_INF("poly_mult(&u, N - 1) started");
    if (poly_init(&u, N - 1) != 0)
    {
        LOG_ERR("poly_init(&u, N - 1) failed");
        return -1;
    }
    for (uint32_t i = 0; i < N; i++)
    {
        bigint_t bit;
        bigint_init(&bit);
        uint8_t r;
        math_hal_rng_bytes(&r, 1);
        bigint_init_u64(&bit, r & 1);
        poly_set_coeff(&u, i, &bit);
    }

    // 2. Sample error polynomials e0, e1 (small noise: -1, 0, 1)
    polynomial_t e0, e1;
    if (poly_init(&e0, N - 1) != 0 || poly_init(&e1, N - 1) != 0)
        return -1;
    for (uint32_t i = 0; i < N; i++)
    {
        bigint_t noise0, noise1;
        bigint_init(&noise0);
        bigint_init(&noise1);
        uint8_t r0, r1;
        math_hal_rng_bytes(&r0, 1);
        math_hal_rng_bytes(&r1, 1);
        int8_t val0 = (r0 % 3) - 1;
        int8_t val1 = (r1 % 3) - 1;
        if (val0 < 0)
        {
            bigint_init_u64(&noise0, (uint64_t)(-val0));
            noise0.is_negative = true;
        }
        else
        {
            bigint_init_u64(&noise0, (uint64_t)val0);
        }
        if (val1 < 0)
        {
            bigint_init_u64(&noise1, (uint64_t)(-val1));
            noise1.is_negative = true;
        }
        else
        {
            bigint_init_u64(&noise1, (uint64_t)val1);
        }
        poly_set_coeff(&e0, i, &noise0);
        poly_set_coeff(&e1, i, &noise1);
    }

    // 3. Compute c[0] = pk[0]*u + e0 + m
    polynomial_t pk0u, c0;
    LOG_INF("poly_init(pk0u) started");
    if (poly_init(&pk0u, N - 1) != 0)
    {
        LOG_ERR("poly_init(pk0u) failed");
        return -1;
    }

    LOG_INF("poly_mult(pk0u, pk[0], u) started");
    if (poly_mult(&pk0u, &pk->pk[0], &u, ring) != 0)
    {
        LOG_ERR("poly_mult(pk0u, pk[0], u) failed");
        return -1;
    }

    LOG_INF("poly_init(c0) started");
    if (poly_init(&c0, N - 1) != 0)
    {
        LOG_ERR("poly_init(c0) failed");
        return -1;
    }
    LOG_INF("poly_add(c0, pk0u, e0) started");
    if (poly_add(&c0, &pk0u, &e0, ring) != 0)
    {
        LOG_ERR("poly_add(c0, pk0u, e0) failed");
        return -1;
    }
    LOG_INF("poly_add(c0, c0, plaintext) started");
    if (poly_add(&c0, &c0, &plaintext->poly, ring))
    {
        LOG_ERR("poly_add(c0, c0, plaintext) failed");
        return -1;
    }

    // 4. Compute c[1] = pk[1]*u + e1
    polynomial_t pk1u, c1;
    LOG_INF("poly_init(pk1u) started");
    if (poly_init(&pk1u, N - 1) != 0)
    {
        LOG_ERR("poly_init(pk1u) failed");
        return -1;
    }
    LOG_INF("poly_mult(pk1u, pk[1], u) started");
    if (poly_mult(&pk1u, &pk->pk[1], &u, ring))
    {
        LOG_ERR("poly_mult(pk1u, pk[1], u) failed");
        return -1;
    }

    LOG_INF("poly_init(c1) started");
    if (poly_init(&c1, N - 1) != 0)
    {
        LOG_ERR("poly_init(c1) failed");
        return -1;
    }
    LOG_INF("poly_add(c1, pk1u, e1) started");
    if (poly_add(&c1, &pk1u, &e1, ring))
    {
        LOG_ERR("poly_add(c1, pk1u, e1) failed");
        return -1;
    }

    // 5. Output ciphertext
    LOG_INF("poly_copy(ciphertext->parts[0], c0) started");
    if (poly_copy(&ciphertext->parts[0], &c0))
    {
        LOG_ERR("poly_copy(ciphertext->parts[0], c0) failed");
        return -1;
    }
    LOG_INF("poly_copy(ciphertext->parts[1], c1) started");
    if (poly_copy(&ciphertext->parts[1], &c1))
    {
        LOG_ERR("poly_copy(ciphertext->parts[1], c1) failed");
    }
    ciphertext->level = 0;
    ciphertext->scaling_factor = params->scaling_factor;
    ciphertext->depth = 1;

    return 0;
}