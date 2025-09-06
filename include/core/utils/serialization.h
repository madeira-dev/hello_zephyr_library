#ifndef OPENFHE_CORE_UTILS_SERIALIZATION_H_
#define OPENFHE_CORE_UTILS_SERIALIZATION_H_

#include "scheme/ckks/ckks.h"
#include <stddef.h>

/**
 * @file serialization.h
 * @brief Provides serialization for cryptographic objects.
 */

/**
 * @brief Serializes a CKKS ciphertext into a JSON string.
 *
 * This function creates a JSON representation of the ciphertext that is
 * compatible with the format expected by the full OpenFHE library's
 * deserializer.
 *
 * @param ciphertext The ciphertext to serialize.
 * @param buffer The character buffer to write the JSON string to.
 * @param buffer_size The size of the output buffer.
 * @return The number of characters written, or a negative value on error.
 */
int ckks_ciphertext_serialize_json(const ckks_ciphertext_t *ciphertext, char *buffer, size_t buffer_size);

#endif // OPENFHE_CORE_UTILS_SERIALIZATION_H_