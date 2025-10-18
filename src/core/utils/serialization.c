#include "core/utils/serialization.h"
#include <stdint.h>
#include <string.h>
#include "core/math/biginteger.h"
#include <limits.h>

static int write_bytes(FILE *stream, const uint8_t *data, size_t size)
{
  return fwrite(data, 1, size, stream) == size ? 0 : -1;
}

static int write_u32(FILE *stream, uint32_t value)
{
  uint8_t buf[4] = {
      (uint8_t)(value),
      (uint8_t)(value >> 8),
      (uint8_t)(value >> 16),
      (uint8_t)(value >> 24)};
  return write_bytes(stream, buf, sizeof(buf));
}

static int write_u64(FILE *stream, uint64_t value)
{
  uint8_t buf[8];
  for (size_t i = 0; i < sizeof(buf); i++)
  {
    buf[i] = (uint8_t)(value >> (8 * i));
  }
  return write_bytes(stream, buf, sizeof(buf));
}

static int write_double(FILE *stream, double value)
{
  uint64_t raw;
  memcpy(&raw, &value, sizeof(raw));
  return write_u64(stream, raw);
}

static int write_math_word(FILE *stream, math_word_t value)
{
  uint8_t buf[sizeof(math_word_t)];
  for (size_t i = 0; i < sizeof(math_word_t); i++)
  {
    buf[i] = (uint8_t)(value >> (8 * i));
  }
  return write_bytes(stream, buf, sizeof(buf));
}

static int write_bigint(FILE *stream, const bigint_t *value)
{
  if (!value)
    return -1;

  if (write_u32(stream, value->word_count) != 0)
    return -1;

  uint8_t sign = value->is_negative ? 1U : 0U;
  if (write_bytes(stream, &sign, sizeof(sign)) != 0)
    return -1;

  for (uint32_t i = 0; i < value->word_count; i++)
  {
    uint8_t buf[sizeof(bigint_word_t)];
    for (size_t j = 0; j < sizeof(bigint_word_t); j++)
    {
      buf[j] = (uint8_t)(value->words[i] >> (8 * j));
    }
    if (write_bytes(stream, buf, sizeof(buf)) != 0)
      return -1;
  }

  return 0;
}

size_t serialization_measure_cryptocontext(const ckks_cryptoparams_t *params)
{
  if (!params || !params->modulus_bits || !params->poly_params_rns || !params->modulus_chain)
    return 0;

  size_t total = sizeof(uint8_t) * 8 + sizeof(uint32_t) * 3 + sizeof(double);

  for (uint32_t i = 0; i < params->num_moduli; i++)
  {
    const bigint_t *chain = &params->modulus_chain[i];
    if (!chain)
      return 0;

    total += sizeof(uint32_t);
    total += sizeof(math_word_t);
    total += sizeof(uint32_t);
    total += sizeof(uint8_t);
    total += (size_t)chain->word_count * sizeof(bigint_word_t);
  }

  return total;
}

int serialization_export_cryptocontext(FILE *stream, const ckks_cryptoparams_t *params)
{
  if (!stream || !params)
    return -1;

  size_t total_bytes = serialization_measure_cryptocontext(params);
  if (total_bytes == 0)
    return -1;

  const uint8_t magic[] = {'O', 'F', 'H', 'E', 'C', 'T', 'X', 0x01};
  if (write_bytes(stream, magic, sizeof(magic)) != 0)
    return -1;

  if (write_u32(stream, params->ring_dimension) != 0)
    return -1;
  if (write_u32(stream, params->num_moduli) != 0)
    return -1;
  if (write_double(stream, params->scaling_factor) != 0)
    return -1;
  if (write_u32(stream, params->max_depth) != 0)
    return -1;

  for (uint32_t i = 0; i < params->num_moduli; i++)
  {
    if (write_u32(stream, params->modulus_bits[i]) != 0)
      return -1;
    if (write_math_word(stream, params->poly_params_rns[i].coefficient_modulus) != 0)
      return -1;
    if (write_bigint(stream, &params->modulus_chain[i]) != 0)
      return -1;
  }

  if (fflush(stream) != 0)
    return -1;

  return (total_bytes > INT_MAX) ? INT_MAX : (int)total_bytes;
}

int serialization_export_cryptocontext_path(const char *filepath, const ckks_cryptoparams_t *params)
{
  if (!filepath)
    return -1;

  FILE *fp = fopen(filepath, "wb");
  if (!fp)
    return -1;

  int rc = serialization_export_cryptocontext(fp, params);
  fclose(fp);
  return rc;
}
