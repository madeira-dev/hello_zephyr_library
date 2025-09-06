#include "core/math/math_hal.h"
#include "core/math/prime_utils.h"
#include <zephyr/logging/log.h>
#include <zephyr/random/random.h>
#include <stdint.h>

LOG_MODULE_REGISTER(math_hal, LOG_LEVEL_DBG);

// ============================================================================
// Modular Arithmetic Primitives
// ============================================================================

math_word_t math_hal_mod_add(math_word_t a, math_word_t b, math_word_t m)
{
  if (m == 0)
    return 0;
  a %= m;
  b %= m;
  if (a >= m - b)
  {
    return a - (m - b); // (a + b) - m
  }
  else
  {
    return a + b;
  }
}

math_word_t math_hal_mod_sub(math_word_t a, math_word_t b, math_word_t m)
{
  if (m == 0)
    return 0;
  a %= m;
  b %= m;
  if (a >= b)
  {
    return a - b;
  }
  else
  {
    return m - (b - a);
  }
}

math_word_t math_hal_mod_mult(math_word_t a, math_word_t b, math_word_t m)
{
  if (m == 0)
    return 0;
  a %= m;
  b %= m;
  uint64_t prod = (uint64_t)a * (uint64_t)b;
  return (math_word_t)(prod % (uint64_t)m);
}

math_word_t math_hal_mod_pow(math_word_t base, math_word_t exp, math_word_t m)
{
  if (m == 1)
    return 0;
  if (exp == 0)
    return 1;
  math_word_t result = 1;
  base %= m;
  while (exp > 0)
  {
    if (exp & 1)
    {
      result = math_hal_mod_mult(result, base, m);
    }
    exp >>= 1;
    base = math_hal_mod_mult(base, base, m);
  }
  return result;
}

math_word_t math_hal_mod_inv(math_word_t a, math_word_t m)
{
  // Extended Euclidean Algorithm
  if (m == 0)
    return 0;
  a %= m;
  if (a == 0)
    return 0; // no inverse exists

  math_word_t m0 = m;
  int64_t x0 = 0, x1 = 1;
  int64_t aa = (int64_t)a;
  int64_t mm = (int64_t)m;

  while (aa > 1)
  {
    if (mm == 0)
      return 0; // a is not invertible
    int64_t q = aa / mm;
    int64_t t = mm;
    mm = aa % mm;
    aa = t;
    t = x0;
    x0 = x1 - q * x0;
    x1 = t;
  }

  if (aa != 1)
    return 0; // a is not invertible

  // Normalize x1 to be in [0, m0-1]
  if (x1 < 0)
    x1 += (int64_t)m0;

  return (math_word_t)x1;
}

// ============================================================================
// NTT Operations
// ============================================================================

// Helper: Bit-reversal permutation
static void bit_reverse_permute(math_word_t *data, uint32_t n, uint32_t log_n)
{
  for (uint32_t i = 0; i < n; i++)
  {
    uint32_t j = 0;
    uint32_t temp_i = i;
    for (uint32_t k = 0; k < log_n; k++)
    {
      j = (j << 1) | (temp_i & 1);
      temp_i >>= 1;
    }

    if (j > i)
    {
      math_word_t temp = data[i];
      data[i] = data[j];
      data[j] = temp;
    }
  }
}

// Helper: collect distinct prime factors of n
static uint32_t unique_prime_factors(uint32_t n, uint32_t out[], uint32_t max_out)
{
  uint32_t cnt = 0;
  if ((n & 1u) == 0u)
  {
    if (cnt < max_out)
      out[cnt++] = 2;
    while ((n & 1u) == 0u)
      n >>= 1;
  }
  for (uint32_t f = 3; (uint64_t)f * (uint64_t)f <= n; f += 2)
  {
    if (n % f == 0)
    {
      if (cnt < max_out)
        out[cnt++] = f;
      while (n % f == 0)
        n /= f;
    }
  }
  if (n > 1 && cnt < max_out)
    out[cnt++] = n;
  return cnt;
}

// Find a generator g of F_p^* (order p-1)
static math_word_t find_generator(math_word_t p)
{
  math_word_t phi = p - 1;
  uint32_t pf[16];
  uint32_t pf_cnt = unique_prime_factors((uint32_t)phi, pf, 16);
  for (math_word_t a = 2; a < p; ++a)
  {
    bool ok = true;
    for (uint32_t i = 0; i < pf_cnt; ++i)
    {
      if (math_hal_mod_pow(a, phi / pf[i], p) == 1)
      {
        ok = false;
        break;
      }
    }
    if (ok)
    {
      return a;
    }
  }
  return 0;
}

// Return an element of exact order n, assuming n | (p-1)
static math_word_t find_primitive_nth_root(uint32_t n, math_word_t p)
{
  if (n == 0)
    return 0;
  math_word_t phi = p - 1;
  if ((phi % n) != 0)
  {
    return 0;
  }

  math_word_t g = find_generator(p);
  if (!g)
    return 0;

  math_word_t w = math_hal_mod_pow(g, phi / n, p);
  return w;
}

// Safe modular inverse for prime modulus with Fermat fallback
static inline math_word_t mod_inv_prime_safe(math_word_t a, math_word_t p)
{
  math_word_t inv = math_hal_mod_inv(a % p, p);
  if (inv == 0)
  {
    inv = math_hal_mod_pow(a % p, p - 2, p);
  }
  return inv;
}

int math_hal_ntt_init_params(ntt_params_t *params, uint32_t n, math_word_t modulus)
{
  if (!params || (n & (n - 1)) != 0 || !is_prime_mod_1_mod_2N(modulus, n))
    return -1;

  if ((modulus - 1) % n != 0)
    return -1;

  params->n = n;
  params->modulus = modulus;
  params->log_n = 0;

  uint32_t temp = n;
  while (temp > 1)
  {
    temp >>= 1;
    params->log_n++;
  }

  params->root_of_unity = find_primitive_nth_root(n, modulus);
  if (params->root_of_unity == 0)
  {
    return -1;
  }

  math_word_t check_n = math_hal_mod_pow(params->root_of_unity, n, modulus);
  math_word_t check_n2 = (n > 1) ? math_hal_mod_pow(params->root_of_unity, n >> 1, modulus) : 0;
  if (check_n != 1 || (n > 1 && check_n2 == 1))
  {
    return -1;
  }

  params->inv_root_of_unity = mod_inv_prime_safe(params->root_of_unity, modulus);
  params->inv_n = mod_inv_prime_safe((math_word_t)(n % modulus), modulus);

  if (math_hal_mod_mult(params->root_of_unity, params->inv_root_of_unity, modulus) != 1)
  {
    return -1;
  }
  if (math_hal_mod_mult(n % modulus, params->inv_n, modulus) != 1)
  {
    return -1;
  }

  return 0;
}

int math_hal_ntt_forward(math_word_t *data, const ntt_params_t *params)
{
  if (!data || !params)
    return -1;

  uint32_t n = params->n;
  uint32_t log_n = params->log_n;
  math_word_t modulus = params->modulus;

  bit_reverse_permute(data, n, log_n);

  for (uint32_t len = 2; len <= n; len <<= 1)
  {
    math_word_t w_len = math_hal_mod_pow(params->root_of_unity, n / len, modulus);
    for (uint32_t i = 0; i < n; i += len)
    {
      math_word_t w = 1;
      for (uint32_t j = 0; j < len / 2; j++)
      {
        uint32_t idx1 = i + j;
        uint32_t idx2 = i + j + len / 2;
        math_word_t u = data[idx1];
        math_word_t v = math_hal_mod_mult(data[idx2], w, modulus);

        data[idx1] = math_hal_mod_add(u, v, modulus);
        data[idx2] = math_hal_mod_sub(u, v, modulus);

        w = math_hal_mod_mult(w, w_len, modulus);
      }
    }
  }

  return 0;
}

int math_hal_ntt_inverse(math_word_t *data, const ntt_params_t *params)
{
  if (!data || !params)
    return -1;

  uint32_t n = params->n;
  uint32_t log_n = params->log_n;
  math_word_t modulus = params->modulus;

  bit_reverse_permute(data, n, log_n);

  for (uint32_t len = 2; len <= n; len <<= 1)
  {
    math_word_t w_len = math_hal_mod_pow(params->inv_root_of_unity, n / len, modulus);
    for (uint32_t i = 0; i < n; i += len)
    {
      math_word_t w = 1;
      for (uint32_t j = 0; j < len / 2; j++)
      {
        uint32_t idx1 = i + j;
        uint32_t idx2 = i + j + len / 2;
        math_word_t u = data[idx1];
        math_word_t v = math_hal_mod_mult(data[idx2], w, modulus);

        data[idx1] = math_hal_mod_add(u, v, modulus);
        data[idx2] = math_hal_mod_sub(u, v, modulus);

        w = math_hal_mod_mult(w, w_len, modulus);
      }
    }
  }

  for (uint32_t i = 0; i < n; i++)
  {
    data[i] = math_hal_mod_mult(data[i], params->inv_n, modulus);
  }

  return 0;
}

int math_hal_ntt_mult(math_word_t *result, const math_word_t *a,
                      const math_word_t *b, const ntt_params_t *params)
{
  if (!result || !a || !b || !params)
    return -1;

  for (uint32_t i = 0; i < params->n; i++)
  {
    result[i] = math_hal_mod_mult(a[i], b[i], params->modulus);
  }

  return 0;
}

// ============================================================================
// Random Number Generation
// ============================================================================

static bool rng_initialized = false;

int math_hal_rng_init(void)
{
  // The Zephyr random subsystem is initialized at boot.
  // This function is kept for API compatibility.
  rng_initialized = true;
  return 0;
}

int math_hal_rng_bytes(uint8_t *buffer, size_t size)
{
  if (!buffer || size == 0)
    return -1;

  if (!rng_initialized)
  {
    if (math_hal_rng_init() != 0)
      return -1;
  }

  return sys_csrand_get(buffer, size);
}

math_word_t math_hal_rng_uniform(math_word_t max)
{
  if (max == 0)
    return 0;

  math_word_t result;
  int ret = math_hal_rng_bytes((uint8_t *)&result, sizeof(result));
  if (ret != 0)
    return 0;

  return result % max;
}

math_word_t math_hal_rng_mod(math_word_t m)
{
  return math_hal_rng_uniform(m);
}

// ============================================================================
// Memory and Performance Utilities
// ============================================================================

void math_hal_secure_zero(void *ptr, size_t size)
{
  if (!ptr || size == 0)
    return;

  volatile uint8_t *p = (volatile uint8_t *)ptr;
  for (size_t i = 0; i < size; i++)
  {
    p[i] = 0;
  }
}

bool math_hal_is_prime(math_word_t n)
{
  if (n < 2)
    return false;
  if (n == 2)
    return true;
  if (n % 2 == 0)
    return false;

  for (math_word_t i = 3; (uint64_t)i * i <= n; i += 2)
  {
    if (n % i == 0)
      return false;
  }

  return true;
}

math_word_t math_hal_next_prime(math_word_t n)
{
  if (n < 2)
    return 2;

  if (n % 2 == 0)
    n++;
  else if (n > 2)
    n += 2;

  while (!math_hal_is_prime(n))
  {
    n += 2;
  }

  return n;
}

math_word_t math_hal_gcd(math_word_t a, math_word_t b)
{
  while (b != 0)
  {
    math_word_t temp = b;
    b = a % b;
    a = temp;
  }
  return a;
}

uint64_t math_hal_get_cycles(void)
{
  return (uint64_t)k_uptime_get();
}