#ifndef OPENFHE_CORE_MATH_DGG_H_
#define OPENFHE_CORE_MATH_DGG_H_

#include "core/math/math_hal.h"

/**
 * @file dgg.h
 * @brief Discrete Gaussian Generator for sampling noise.
 *
 * Provides cryptographically secure noise generation from a discrete
 * Gaussian distribution, which is essential for the security of lattice-based
 * schemes like CKKS.
 */

/**
 * @brief Discrete Gaussian Generator parameters.
 *
 * For embedded use, this can be a lightweight structure, as the distribution
 * table can be pre-computed and stored in ROM.
 */
typedef struct
{
    double sigma; // Standard deviation (for reference)
} dgg_sampler_t;

/**
 * @brief Initialize the DGG sampler.
 * @param sampler The sampler instance to initialize.
 * @param sigma The standard deviation of the Gaussian distribution.
 * @return 0 on success.
 */
int dgg_init(dgg_sampler_t *sampler, double sigma);

/**
 * @brief Generate a signed integer from a discrete Gaussian distribution.
 *
 * This function uses a pre-computed table to efficiently sample a small
 * integer value.
 *
 * @param sampler The DGG sampler instance.
 * @return A randomly sampled integer.
 */
int32_t dgg_generate_integer(const dgg_sampler_t *sampler);

#endif // OPENFHE_CORE_MATH_DGG_H_