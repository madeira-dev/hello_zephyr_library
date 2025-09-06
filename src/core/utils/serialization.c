#include "core/utils/serialization.h"
#include <stdio.h>

// Helper to serialize a single RNS polynomial
static int rns_poly_serialize(const rns_polynomial_t *rns_poly, char *buffer, size_t buffer_size, size_t *offset)
{
  int written = 0;
  int ret;

  // Each RNS poly is an object with a vector "v"
  written = snprintf(buffer + *offset, buffer_size - *offset, "{\"v\":[");
  if (written < 0 || *offset + written >= buffer_size)
    return -1;
  *offset += written;

  for (uint32_t i = 0; i < rns_poly->num_moduli; i++)
  {
    const polynomial_t *poly = &rns_poly->polys[i];
    // Each poly is an object with a vector "v" and format "f"
    written = snprintf(buffer + *offset, buffer_size - *offset, "{\"v\":[");
    if (written < 0 || *offset + written >= buffer_size)
      return -1;
    *offset += written;

    for (uint32_t j = 0; j <= poly->degree; j++)
    {
      ret = snprintf(buffer + *offset, buffer_size - *offset, "%u%s",
                     (unsigned int)poly->coeffs[j], (j == poly->degree) ? "" : ",");
      if (ret < 0 || *offset + ret >= buffer_size)
        return -1;
      *offset += ret;
    }

    written = snprintf(buffer + *offset, buffer_size - *offset, "],\"f\":\"COEFFICIENT\"}%s",
                       (i == rns_poly->num_moduli - 1) ? "" : ",");
    if (written < 0 || *offset + written >= buffer_size)
      return -1;
    *offset += written;
  }

  written = snprintf(buffer + *offset, buffer_size - *offset, "]}");
  if (written < 0 || *offset + written >= buffer_size)
    return -1;
  *offset += written;

  return 0;
}

int ckks_ciphertext_serialize_json(const ckks_ciphertext_t *ciphertext, char *buffer, size_t buffer_size)
{
  if (!ciphertext || !buffer || buffer_size == 0)
    return -1;

  size_t offset = 0;
  int written;

  // Root object for cereal compatibility
  written = snprintf(buffer + offset, buffer_size - offset, "{\"v\":{");
  if (written < 0 || offset + written >= buffer_size)
    return -1;
  offset += written;

  // Serialize c0
  written = snprintf(buffer + offset, buffer_size - offset, "\"c0\":");
  if (written < 0 || offset + written >= buffer_size)
    return -1;
  offset += written;

  if (rns_poly_serialize(&ciphertext->parts[0], buffer, buffer_size, &offset) != 0)
    return -1;

  // Separator
  written = snprintf(buffer + offset, buffer_size - offset, ",");
  if (written < 0 || offset + written >= buffer_size)
    return -1;
  offset += written;

  // Serialize c1
  written = snprintf(buffer + offset, buffer_size - offset, "\"c1\":");
  if (written < 0 || offset + written >= buffer_size)
    return -1;
  offset += written;

  if (rns_poly_serialize(&ciphertext->parts[1], buffer, buffer_size, &offset) != 0)
    return -1;

  // Close root object
  written = snprintf(buffer + offset, buffer_size - offset, "}}");
  if (written < 0 || offset + written >= buffer_size)
    return -1;
  offset += written;

  return offset;
}