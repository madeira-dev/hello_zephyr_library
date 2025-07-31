#ifndef OPENFHE_SCHEME_CKKS_CRYPTOPARAMS_H_
#define OPENFHE_SCHEME_CKKS_CRYPTOPARAMS_H_

#include <stdint.h>
#include <stdbool.h>
#include "core/math/biginteger.h"
#include "core/math/polynomial.h"

/**
 * @file ckks_cryptoparams.h
 * @brief CKKS scheme cryptographic parameters for OpenFHE-Embedded
 *
 * This structure holds all parameters required for CKKS encryption,
 * including ring dimension, modulus chain, scaling factor, and more.
 */

// Maximum number of modulus primes in the chain (for RNS)
#define CKKS_MAX_MODULI 2

typedef struct
{
    uint32_t ring_dimension;                 // N: polynomial ring dimension (power of 2)
    uint32_t num_moduli;                     // Number of moduli in the chain
    bigint_t modulus_chain[CKKS_MAX_MODULI]; // RNS modulus primes (q_0, q_1, ...)
    uint32_t modulus_bits[CKKS_MAX_MODULI];  // Bit size of each modulus
    double scaling_factor;                   // CKKS scaling factor (Δ)
    uint32_t max_depth;                      // Max multiplicative depth supported
    poly_ring_params_t poly_params;          // Polynomial ring parameters (Z[X]/(X^N+1), modulus)
} ckks_cryptoparams_t;

/**
 * @brief Initialize CKKS crypto parameters
 * @param params Output parameter structure
 * @param ring_dimension Polynomial ring dimension (N)
 * @param modulus_chain Array of modulus primes
 * @param num_moduli Number of moduli in the chain
 * @param scaling_factor CKKS scaling factor (Δ)
 * @param max_depth Maximum multiplicative depth
 * @return 0 on success, negative on error
 */
int ckks_cryptoparams_init(
    ckks_cryptoparams_t *params,
    uint32_t ring_dimension,
    const bigint_t *modulus_chain,
    uint32_t num_moduli,
    double scaling_factor,
    uint32_t max_depth);

/**
 * @brief Clean up CKKS crypto parameters
 * @param params Parameter structure to clean up
 */
void ckks_cryptoparams_cleanup(ckks_cryptoparams_t *params);

#endif