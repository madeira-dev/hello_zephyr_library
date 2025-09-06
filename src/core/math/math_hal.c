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

math_word_t math_hal_find_primitive_root(math_word_t k, math_word_t modulus)
{
  if (modulus == 2)
    return 1;

  // Check if a k-th root exists
  if ((modulus - 1) % k != 0)
  {
    return 0;
  }

  math_word_t phi = modulus - 1;
  math_word_t exponent = phi / k;

  // Iterate through candidates for a generator
  for (math_word_t g = 2; g < modulus; g++)
  {
    // Check if g is a generator (primitive root) of the full group
    // This is a simplified check that works for many cases.
    // A full check would require factoring phi.
    if (math_hal_mod_pow(g, phi / 2, modulus) == 1)
    {
      continue; // Not a generator
    }

    // If g is a generator, then g^((modulus-1)/k) is a primitive k-th root of unity.
    return math_hal_mod_pow(g, exponent, modulus);
  }

  return 0; // No primitive root found
}

// ============================================================================
// Number Theoretic Transform (NTT) - Generic C Implementation
// ============================================================================

// Forward declarations for the C implementation of NTT
static void ntt_forward_c(math_word_t *data, const ntt_params_t *params);
static void ntt_inverse_c(math_word_t *data, const ntt_params_t *params);
static void ntt_mult_c(math_word_t *result, const math_word_t *a, const math_word_t *b, const ntt_params_t *params);

// Helper for bit-reversal permutation
static void bit_reverse_permutation(math_word_t *data, uint32_t n, uint32_t log_n)
{
  for (uint32_t i = 0; i < n; i++)
  {
    uint32_t j = 0;
    uint32_t temp_i = i;
    for (uint32_t bit = 0; bit < log_n; bit++)
    {
      j = (j << 1) | (temp_i & 1);
      temp_i >>= 1;
    }
    if (i < j)
    {
      math_word_t temp = data[i];
      data[i] = data[j];
      data[j] = temp;
    }
  }
}

// Cooley-Tukey NTT (Forward)
static void ntt_forward_c(math_word_t *data, const ntt_params_t *params)
{
  uint32_t n = params->n;
  uint32_t log_n = params->log_n;
  math_word_t modulus = params->modulus;
  math_word_t root = params->root_of_unity;

  bit_reverse_permutation(data, n, log_n);

  for (uint32_t len = 2; len <= n; len <<= 1)
  {
    math_word_t w_len = math_hal_mod_pow(root, n / len, modulus);
    for (uint32_t i = 0; i < n; i += len)
    {
      math_word_t w = 1;
      for (uint32_t j = 0; j < len / 2; j++)
      {
        math_word_t u = data[i + j];
        math_word_t v = math_hal_mod_mult(data[i + j + len / 2], w, modulus);
        data[i + j] = math_hal_mod_add(u, v, modulus);
        data[i + j + len / 2] = math_hal_mod_sub(u, v, modulus);
        w = math_hal_mod_mult(w, w_len, modulus);
      }
    }
  }
}

// Cooley-Tukey NTT (Inverse)
static void ntt_inverse_c(math_word_t *data, const ntt_params_t *params)
{
  uint32_t n = params->n;
  math_word_t modulus = params->modulus;
  math_word_t inv_root = params->inv_root_of_unity;
  math_word_t inv_n = params->inv_n;

  // The inverse NTT is the same as forward, but with the inverse root
  ntt_params_t inv_params = *params;
  inv_params.root_of_unity = inv_root;
  ntt_forward_c(data, &inv_params);

  // Final multiplication by N^-1
  for (uint32_t i = 0; i < n; i++)
  {
    data[i] = math_hal_mod_mult(data[i], inv_n, modulus);
  }
}

// Point-wise multiplication in NTT domain
static void ntt_mult_c(math_word_t *result, const math_word_t *a, const math_word_t *b, const ntt_params_t *params)
{
  uint32_t n = params->n;
  math_word_t modulus = params->modulus;
  for (uint32_t i = 0; i < n; i++)
  {
    result[i] = math_hal_mod_mult(a[i], b[i], modulus);
  }
}

// ============================================================================
// Public HAL Functions
// ============================================================================

// ============================================================================
// NTT Operations
// ============================================================================

int math_hal_ntt_init_params(ntt_params_t *params, uint32_t n, math_word_t modulus)
{
  if (!params || (n & (n - 1)) != 0 || n > MATH_HAL_MAX_NTT_SIZE)
    return -1; // n must be a power of 2

  params->n = n;
  params->modulus = modulus;
  params->log_n = 0;
  uint32_t temp_n = n;
  while (temp_n > 1)
  {
    temp_n >>= 1;
    params->log_n++;
  }

  // Find a 2n-th primitive root of unity
  math_word_t root = math_hal_find_primitive_root(2 * n, modulus);
  if (root == 0)
    return -1; // No suitable root found

  params->root_of_unity = math_hal_mod_pow(root, 2, modulus);
  params->inv_root_of_unity = math_hal_mod_inv(params->root_of_unity, modulus);
  params->inv_n = math_hal_mod_inv(n, modulus);

  // Set the function pointers to the generic C implementation
  params->ntt_forward_impl = ntt_forward_c;
  params->ntt_inverse_impl = ntt_inverse_c;
  params->ntt_mult_impl = ntt_mult_c;

  return 0;
}

int math_hal_ntt_forward(math_word_t *data, const ntt_params_t *params)
{
  if (!params || !params->ntt_forward_impl)
    return -1;
  params->ntt_forward_impl(data, params);
  return 0;
}

int math_hal_ntt_inverse(math_word_t *data, const ntt_params_t *params)
{
  if (!params || !params->ntt_inverse_impl)
    return -1;
  params->ntt_inverse_impl(data, params);
  return 0;
}

int math_hal_ntt_mult(math_word_t *result, const math_word_t *a,
                      const math_word_t *b, const ntt_params_t *params)
{
  if (!params || !params->ntt_mult_impl)
    return -1;
  params->ntt_mult_impl(result, a, b, params);
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