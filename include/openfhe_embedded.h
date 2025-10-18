#ifndef OPENFHE_EMBEDDED_H_
#define OPENFHE_EMBEDDED_H_

#include "scheme/ckks/ckks.h"
#include "scheme/ckks/ckks_encoder.h"
#include <stddef.h>

/**
 * @file openfhe_embedded.h
 * @brief Main user-facing API for the OpenFHE-Embedded library.
 *
 * This file provides a high-level interface for performing CKKS encryption.
 */

/**
 * @brief Main context for all OpenFHE-Embedded operations.
 *
 * This structure encapsulates all cryptographic parameters, keys, and encoders
 * needed to perform encryption.
 */
typedef struct
{
  ckks_cryptoparams_t params;
  ckks_encoder_t encoder;
  ckks_publickey_t public_key;
  ckks_secretkey_t secret_key;
  bool is_initialized;
} fhe_context_t;

/**
 * @brief Initializes the FHE context.
 *
 * This sets up the cryptographic parameters and generates a new public key.
 *
 * @param context The context to initialize.
 * @param ring_dimension The polynomial ring dimension (N).
 * @param scaling_factor The CKKS scaling factor (delta).
 * @return 0 on success, negative on error.
 */
int fhe_context_init(fhe_context_t *context, uint32_t ring_dimension, double scaling_factor);

/**
 * @brief Encrypts a vector of doubles.
 *
 * @param context The initialized FHE context.
 * @param values The array of double-precision values to encrypt.
 * @param value_count The number of values in the array.
 * @param ciphertext The output ciphertext structure.
 * @return 0 on success, negative on error.
 */
int fhe_encrypt(const fhe_context_t *context, const double *values, size_t value_count, ckks_ciphertext_t *ciphertext);

/**
 * @brief Cleans up and releases all resources used by the context.
 * @param context The context to clean up.
 */
void fhe_context_cleanup(fhe_context_t *context);

#endif // OPENFHE_EMBEDDED_H_