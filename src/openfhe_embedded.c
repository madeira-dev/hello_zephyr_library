#include "openfhe_embedded.h"
#include "scheme/ckks/ckks_keygen.h"
#include "scheme/ckks/ckks_encryptor.h"
#include "core/utils/serialization.h"
#include <string.h>

int fhe_context_init(fhe_context_t *context, uint32_t ring_dimension, double scaling_factor)
{
  if (!context)
    return -1;

  memset(context, 0, sizeof(fhe_context_t));

  // 1. Initialize crypto parameters (auto-generating NTT primes)
  if (ckks_cryptoparams_init(&context->params, ring_dimension, NULL, 0, scaling_factor, 1) != 0)
  {
    return -1;
  }

  // 2. Initialize the encoder with these parameters
  if (ckks_encoder_init(&context->encoder, &context->params) != 0)
  {
    return -1;
  }

  // 3. Generate the public key
  if (ckks_keygen(&context->params, &context->public_key, NULL) != 0)
  {
    return -1;
  }

  context->is_initialized = true;
  return 0;
}

int fhe_encrypt(const fhe_context_t *context, const double *values, size_t value_count, ckks_ciphertext_t *ciphertext)
{
  if (!context || !context->is_initialized || !values || !ciphertext)
    return -1;

  // 1. Encode the values into a plaintext polynomial
  ckks_plaintext_t plaintext;
  if (ckks_encode(&context->encoder, values, value_count, &plaintext) != 0)
  {
    return -1;
  }

  // 2. Encrypt the plaintext using the public key
  if (ckks_encrypt(&context->params, &context->public_key, &plaintext, ciphertext) != 0)
  {
    return -1;
  }

  return 0;
}

int fhe_ciphertext_serialize(const ckks_ciphertext_t *ciphertext, char *buffer, size_t buffer_size)
{
  return ckks_ciphertext_serialize_json(ciphertext, buffer, buffer_size);
}

void fhe_context_cleanup(fhe_context_t *context)
{
  if (context)
  {
    ckks_cryptoparams_cleanup(&context->params);
    // Other structures are stack-allocated or don't need explicit cleanup
    context->is_initialized = false;
  }
}