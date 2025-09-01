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

  // Compute (a + b) mod m without overflow
  if (a >= m - b)
  {
    // a + b >= m  =>  (a + b) - m
    return a - (m - b);
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

  // Ensure inputs are reduced
  a = a % m;
  b = b % m;

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
  base = base % m;

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
  // Extended Euclidean Algorithm with proper normalization
  if (m == 0)
    return 0;

  a %= m;
  if (a == 0)
    return 0; // no inverse exists

  math_word_t m0 = m;

  // Use wider signed accumulators to avoid overflow during updates
  int64_t x0 = 0, x1 = 1;  // coefficients
  int64_t aa = (int64_t)a; // current a
  int64_t mm = (int64_t)m; // current m

  while (aa > 1)
  {
    int64_t q = aa / mm;
    int64_t t = mm;

    mm = aa % mm;
    aa = t;

    int64_t t_signed = x0;
    x0 = x1 - q * x0;
    x1 = t_signed;
  }

  // x1 is the inverse in the range (-(m-1), m-1)
  if (x1 < 0)
    x1 += (int64_t)m0;

  // Ensure we return a fully reduced, non-negative representative
  x1 %= (int64_t)m0;
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

// Helper: collect distinct prime factors of n (n is small/power-of-two in NTT)
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

// Helper function to find a primitive n-th root of unity modulo modulus
// Assumes modulus is prime and n is a power of two that divides (modulus - 1)
static math_word_t find_primitive_root(uint32_t n, math_word_t modulus)
{
  if (n == 0)
    return 0;

  math_word_t order = modulus - 1;
  if ((order % n) != 0)
    return 0; // no n-th roots exist in F_p

  // Precompute prime divisors of n for exact-order test
  uint32_t facs[16];
  uint32_t fac_cnt = unique_prime_factors(n, facs, 16);

  // Project candidates into the subgroup of size dividing n, then enforce exact order n
  math_word_t e = order / n;
  for (math_word_t a = 2; a < modulus; ++a)
  {
    math_word_t w = math_hal_mod_pow(a, e, modulus);
    if (w == 1)
      continue; // landed at identity; order < n

    // Check exact order: w^n = 1 and w^(n/q) != 1 for every prime q | n
    if (math_hal_mod_pow(w, n, modulus) != 1)
      continue;

    bool ok = true;
    for (uint32_t i = 0; i < fac_cnt; ++i)
    {
      uint32_t q = facs[i];
      if (math_hal_mod_pow(w, n / q, modulus) == 1)
      {
        ok = false; // order is a proper divisor of n
        break;
      }
    }
    if (ok)
      return w; // primitive n-th root of unity found
  }

  return 0; // should not happen if n | (p-1)
}

int math_hal_ntt_init_params(ntt_params_t *params, uint32_t n, math_word_t modulus)
{
  if (!params || (n & (n - 1)) != 0 || !is_prime_mod_1_mod_2N(modulus, n)) // Use prime_utils for stricter check
    return -1;                                                             // n must be power of 2, modulus must be prime

  // Check if n divides (modulus - 1)
  if ((modulus - 1) % n != 0)
    return -1;

  LOG_DBG("NTT init: n=%u, modulus=%u, (mod-1)/n=%u", n, (uint32_t)modulus, (uint32_t)((modulus - 1) / n));

  params->n = n;
  params->modulus = modulus;
  params->log_n = 0;

  // Calculate log2(n)
  uint32_t temp = n;
  while (temp > 1)
  {
    temp >>= 1;
    params->log_n++;
  }

  // Find primitive n-th root of unity with exact order n
  params->root_of_unity = find_primitive_root(n, modulus);
  if (params->root_of_unity == 0)
  {
    LOG_ERR("Failed to find primitive root for NTT (n=%u, mod=%u)", n, (uint32_t)modulus);
    return -1;
  }

  params->inv_root_of_unity = math_hal_mod_inv(params->root_of_unity, modulus);
  params->inv_n = math_hal_mod_inv(n % modulus, modulus);

  // Sanity checks
  if (math_hal_mod_mult(params->root_of_unity, params->inv_root_of_unity, modulus) != 1)
  {
    LOG_ERR("inv_root_of_unity invalid: root*inv_root != 1 (mod %u)", (uint32_t)modulus);
    return -1;
  }
  if (math_hal_mod_mult(n % modulus, params->inv_n, modulus) != 1)
  {
    LOG_ERR("inv_n invalid: n*inv_n != 1 (mod %u)", (uint32_t)modulus);
    return -1;
  }

  LOG_DBG("Initialized NTT params: n=%u, modulus=%u, root=%u, inv_root=%u, inv_n=%u", n, (uint32_t)modulus, (uint32_t)params->root_of_unity, (uint32_t)params->inv_root_of_unity, (uint32_t)params->inv_n);
  return 0;
}

int math_hal_ntt_forward(math_word_t *data, const ntt_params_t *params)
{
  if (!data || !params)
    return -1;

  uint32_t n = params->n;
  uint32_t log_n = params->log_n;
  math_word_t modulus = params->modulus;

  // 1. Bit-reversal permutation
  bit_reverse_permute(data, n, log_n);

  // 2. Cooley-Tukey butterfly loops
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

  LOG_DBG("Performed forward NTT");
  return 0;
}

int math_hal_ntt_inverse(math_word_t *data, const ntt_params_t *params)
{
  if (!data || !params)
    return -1;

  uint32_t n = params->n;
  uint32_t log_n = params->log_n;
  math_word_t modulus = params->modulus;

  // 1. Bit-reversal permutation
  bit_reverse_permute(data, n, log_n);

  // 2. Cooley-Tukey butterfly loops (with inverse root)
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

  // 3. Scale by n^-1
  for (uint32_t i = 0; i < n; i++)
  {
    data[i] = math_hal_mod_mult(data[i], params->inv_n, modulus);
  }

  LOG_DBG("Performed inverse NTT");
  return 0;
}

int math_hal_ntt_mult(math_word_t *result, const math_word_t *a,
                      const math_word_t *b, const ntt_params_t *params)
{
  if (!result || !a || !b || !params)
    return -1;

  // Element-wise multiplication in NTT domain
  for (uint32_t i = 0; i < params->n; i++)
  {
    result[i] = math_hal_mod_mult(a[i], b[i], params->modulus);
  }

  LOG_DBG("Performed NTT domain multiplication");
  return 0;
}

// ============================================================================
// Random Number Generation
// ============================================================================

static bool rng_initialized = false;

// At the moment this function does nothing (due to some past problems) but I might change it in the future
int math_hal_rng_init(void)
{
  LOG_INF("Using nRF52840 hardware RNG");
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

  sys_rand_get(buffer, size);
  return 0;
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

  // Simple trial division up to sqrt(n)
  for (math_word_t i = 3; i * i <= n; i += 2)
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

  // Make odd if even
  if (n % 2 == 0)
    n++;

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
  return k_uptime_get();
}