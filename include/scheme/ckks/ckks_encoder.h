#ifndef OPENFHE_SCHEME_CKKS_ENCODER_H_
#define OPENFHE_SCHEME_CKKS_ENCODER_H_

#include <stdint.h>
#include <stddef.h>
#include "core/math/polynomial.h"
#include "core/math/biginteger.h"
#include "scheme/ckks/ckks_cryptoparams.h"
#include "ckks.h"

/**
 * @file ckks_encoder.h
 * @brief CKKS encoder for OpenFHE-Embedded (encryption-only)
 *
 * Provides encoding of real/complex values into polynomials for CKKS encryption.
 * Only encoding (not decoding) is implemented for embedded encryption-only use.
 */

// CKKS plaintext slot count = ring_dimension / 2

typedef struct
{
  uint32_t slot_count;               // Number of slots (N/2)
  double scaling_factor;             // Scaling factor (Δ)
  const ckks_cryptoparams_t *params; // Pointer to CKKS crypto parameters
} ckks_encoder_t;

/**
 * @brief Initialize CKKS encoder
 * @param encoder Encoder structure to initialize
 * @param params CKKS crypto parameters
 * @return 0 on success, negative on error
 */
int ckks_encoder_init(ckks_encoder_t *encoder, const ckks_cryptoparams_t *params);

/**
 * @brief Encode an array of doubles into a polynomial (plaintext)
 * @param encoder Encoder instance
 * @param values Input array of doubles (length <= slot_count)
 * @param value_count Number of values to encode
 * @param poly Output polynomial (plaintext)
 * @return 0 on success, negative on error
 *
 * Note: Only encoding is supported (no decoding).
 */
int ckks_encode(const ckks_encoder_t *encoder, const double *values, size_t value_count, ckks_plaintext_t *plaintext);

#endif // OPENFHE_SCHEME_CKKS_ENCODER_H_