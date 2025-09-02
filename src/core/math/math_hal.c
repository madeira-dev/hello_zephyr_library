#include "core/math/math_hal.h"
#include "core/math/prime_utils.h"
#include <zephyr/logging/log.h>
#include <zephyr/random/random.h>
#include <stdint.h>

LOG_MODULE_REGISTER(math_hal, LOG_LEVEL_DBG);

// ============================================================================
// Modular Arithmetic Primitives
// ============================================================================

// math_word_t math_hal_mod_add(math_word_t a, math_word_t b, math_word_t m)
// {
//   if (m == 0)
//     return 0;
//   a %= m;
//   b %= m;
//   if (a >= m - b)
//   {
//     return a - (m - b); // (a + b) - m
//   }
//   else
//   {
//     return a + b;
//   }
// }

// math_word_t math_hal_mod_sub(math_word_t a, math_word_t b, math_word_t m)
// {
//   if (m == 0)
//     return 0;
//   a %= m;
//   b %= m;
//   if (a >= b)
//   {
//     return a - b;
//   }
//   else
//   {
//     return m - (b - a);
//   }
// }

// math_word_t math_hal_mod_mult(math_word_t a, math_word_t b, math_word_t m)
// {
//   if (m == 0)
//     return 0;
//   a %= m;
//   b %= m;
//   uint64_t prod = (uint64_t)a * (uint64_t)b;
//   return (math_word_t)(prod % (uint64_t)m);
// }

// math_word_t math_hal_mod_pow(math_word_t base, math_word_t exp, math_word_t m)
// {
//   if (m == 1)
//     return 0;
//   if (exp == 0)
//     return 1;
//   math_word_t result = 1;
//   base %= m;
//   while (exp > 0)
//   {
//     if (exp & 1)
//     {
//       result = math_hal_mod_mult(result, base, m);
//     }
//     exp >>= 1;
//     base = math_hal_mod_mult(base, base, m);
//   }
//   return result;
// }

// math_word_t math_hal_mod_inv(math_word_t a, math_word_t m)
// {
//   // Extended Euclidean Algorithm with proper normalization
//   if (m == 0)
//     return 0;
//   a %= m;
//   LOG_DBG("mod_inv: a=%u (reduced), m=%u", (uint32_t)a, (uint32_t)m);
//   if (a == 0)
//     return 0; // no inverse exists

//   math_word_t m0 = m;
//   // Use wider signed accumulators to avoid overflow during updates
//   int64_t x0 = 0, x1 = 1;  // coefficients
//   int64_t aa = (int64_t)a; // current a
//   int64_t mm = (int64_t)m; // current m

//   while (aa > 1)
//   {
//     int64_t q = aa / mm;
//     int64_t t = mm;
//     mm = aa % mm;
//     aa = t;
//     int64_t t_signed = x0;
//     x0 = x1 - q * x0;
//     x1 = t_signed;
//   }

//   // Normalize into [0, m0-1] robustly, even if x1 is negative
//   int64_t inv = x1 % (int64_t)m0;
//   if (inv < 0)
//     inv += (int64_t)m0;
//   LOG_DBG("mod_inv: inverse=%u", (uint32_t)inv);
//   return (math_word_t)inv;
// }

// ============================================================================
// NTT Operations
// ============================================================================

// Helper: Bit-reversal permutation
// static void bit_reverse_permute(math_word_t *data, uint32_t n, uint32_t log_n)
// {
//   for (uint32_t i = 0; i < n; i++)
//   {
//     uint32_t j = 0;
//     uint32_t temp_i = i;
//     for (uint32_t k = 0; k < log_n; k++)
//     {
//       j = (j << 1) | (temp_i & 1);
//       temp_i >>= 1;
//     }

//     if (j > i)
//     {
//       math_word_t temp = data[i];
//       data[i] = data[j];
//       data[j] = temp;
//     }
//   }
// }

// Helper: collect distinct prime factors of n (n is small/power-of-two in NTT)
static uint32_t unique_prime_factors(uint32_t n, uint32_t out[], uint32_t max_out)
{
  LOG_DBG("unique_prime_factors: input n=%u", (uint32_t)n);
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
  for (uint32_t i = 0; i < cnt; ++i)
  {
    LOG_DBG("unique_prime_factors: out[%u]=%u", i, (uint32_t)out[i]);
  }
  return cnt;
}

// Find a generator g of F_p^* (order p-1)
static math_word_t find_generator(math_word_t p)
{
  math_word_t phi = p - 1;
  LOG_DBG("find_generator: p=%u, phi=%u", (uint32_t)p, (uint32_t)phi);
  uint32_t pf[16];
  uint32_t pf_cnt = unique_prime_factors((uint32_t)phi, pf, 16);
  for (math_word_t a = 2; a < p; ++a)
  {
    // Be careful to not flood logs for large p; keep it informative for small p
    if (p <= 257)
      LOG_DBG("find_generator: testing a=%u", (uint32_t)a);
    bool ok = true;
    for (uint32_t i = 0; i < pf_cnt; ++i)
    {
      if (math_hal_mod_pow(a, phi / pf[i], p) == 1)
      {
        if (p <= 257)
          LOG_DBG("find_generator: a=%u rejected by factor q=%u", (uint32_t)a, (uint32_t)pf[i]);
        ok = false;
        break;
      }
    }
    if (ok)
    {
      LOG_DBG("find_generator: generator found g=%u", (uint32_t)a);
      return a;
    }
  }
  LOG_ERR("find_generator: no generator found for p=%u", (uint32_t)p);
  return 0;
}

// Return an element of exact order n, assuming n | (p-1)
static math_word_t find_primitive_nth_root(uint32_t n, math_word_t p)
{
  LOG_DBG("find_primitive_nth_root: n=%u, p=%u", (uint32_t)n, (uint32_t)p);
  if (n == 0)
    return 0;
  math_word_t phi = p - 1;
  if ((phi % n) != 0)
  {
    LOG_DBG("find_primitive_nth_root: phi=%u, phi/n=%u", (uint32_t)phi, (uint32_t)(phi / n));
    return 0;
  }
  LOG_DBG("find_primitive_nth_root: phi=%u, phi/n=%u", (uint32_t)phi, (uint32_t)(phi / n));

  math_word_t g = find_generator(p);
  if (!g)
    return 0;

  math_word_t w = math_hal_mod_pow(g, phi / n, p);
  LOG_DBG("find_primitive_nth_root: g=%u, w=g^(phi/n)=%u", (uint32_t)g, (uint32_t)w);

  // Power-of-two exactness check: w^n == 1 and w^(n/2) != 1
  math_word_t wn = math_hal_mod_pow(w, n, p);
  math_word_t wn2 = (n & 1u) ? 0 : math_hal_mod_pow(w, n >> 1, p);
  LOG_DBG("find_primitive_nth_root: w^n=%u, w^(n/2)=%u", (uint32_t)wn, (uint32_t)wn2);

  if (wn != 1)
  {
    LOG_ERR("find_primitive_nth_root: failure w^n != 1 (=%u)", (uint32_t)wn);
    return 0;
  }
  if ((n & 1u) == 0u && wn2 == 1)
  {
    LOG_ERR("find_primitive_nth_root: failure w^(n/2) == 1 (order too small)");
    return 0;
  }

  return w;
}

// Safe modular inverse for prime modulus with Fermat fallback
static inline math_word_t mod_inv_prime_safe(math_word_t a, math_word_t p)
{
  math_word_t inv = math_hal_mod_inv(a % p, p);
  if (inv == 0 || inv >= p)
  {
    LOG_DBG("mod_inv_prime_safe: fallback Fermat for a=%u mod p=%u", (uint32_t)(a % p), (uint32_t)p);
    inv = math_hal_mod_pow(a % p, p - 2, p);
  }
  return inv;
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
  LOG_DBG("NTT init mid: n=%u, modulus=%u, log_n=%u", n, (uint32_t)modulus, params->log_n);

  // Find primitive n-th root of unity with exact order n
  params->root_of_unity = find_primitive_nth_root(n, modulus);
  LOG_DBG("NTT init: root_of_unity=%u", (uint32_t)params->root_of_unity);
  if (params->root_of_unity == 0)
  {
    LOG_ERR("Failed to find primitive root for NTT (n=%u, mod=%u)", n, (uint32_t)modulus);
    return -1;
  }

  // Verify exact order for power-of-two n: w^n == 1 and w^(n/2) != 1
  math_word_t check_n = math_hal_mod_pow(params->root_of_unity, n, modulus);
  math_word_t check_n2 = (n > 1) ? math_hal_mod_pow(params->root_of_unity, n >> 1, modulus) : 0;
  LOG_DBG("NTT verify: w^n=%u, w^(n/2)=%u", (uint32_t)check_n, (uint32_t)check_n2);
  if (check_n != 1 ||
      (n > 1 && check_n2 == 1))
  {
    LOG_ERR("Primitive root does not have exact order n (n=%u, mod=%u)", n, (uint32_t)modulus);
    return -1;
  }

  params->inv_root_of_unity = mod_inv_prime_safe(params->root_of_unity, modulus);
  params->inv_n = mod_inv_prime_safe((math_word_t)(n % modulus), modulus);
  LOG_DBG("NTT inverses: inv_root=%u, inv_n=%u", (uint32_t)params->inv_root_of_unity, (uint32_t)params->inv_n);

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

// int math_hal_ntt_forward(math_word_t *data, const ntt_params_t *params)
// {
//   if (!data || !params)
//     return -1;

//   uint32_t n = params->n;
//   uint32_t log_n = params->log_n;
//   math_word_t modulus = params->modulus;

//   // 1. Bit-reversal permutation
//   bit_reverse_permute(data, n, log_n);

//   // 2. Cooley-Tukey butterfly loops
//   for (uint32_t len = 2; len <= n; len <<= 1)
//   {
//     math_word_t w_len = math_hal_mod_pow(params->root_of_unity, n / len, modulus);
//     for (uint32_t i = 0; i < n; i += len)
//     {
//       math_word_t w = 1;
//       for (uint32_t j = 0; j < len / 2; j++)
//       {
//         uint32_t idx1 = i + j;
//         uint32_t idx2 = i + j + len / 2;
//         math_word_t u = data[idx1];
//         math_word_t v = math_hal_mod_mult(data[idx2], w, modulus);

//         data[idx1] = math_hal_mod_add(u, v, modulus);
//         data[idx2] = math_hal_mod_sub(u, v, modulus);

//         w = math_hal_mod_mult(w, w_len, modulus);
//       }
//     }
//   }

//   LOG_DBG("Performed forward NTT");
//   return 0;
// }

// int math_hal_ntt_inverse(math_word_t *data, const ntt_params_t *params)
// {
//   if (!data || !params)
//     return -1;

//   uint32_t n = params->n;
//   uint32_t log_n = params->log_n;
//   math_word_t modulus = params->modulus;

//   // 1. Bit-reversal permutation
//   bit_reverse_permute(data, n, log_n);

//   // 2. Cooley-Tukey butterfly loops (with inverse root)
//   for (uint32_t len = 2; len <= n; len <<= 1)
//   {
//     math_word_t w_len = math_hal_mod_pow(params->inv_root_of_unity, n / len, modulus);
//     for (uint32_t i = 0; i < n; i += len)
//     {
//       math_word_t w = 1;
//       for (uint32_t j = 0; j < len / 2; j++)
//       {
//         uint32_t idx1 = i + j;
//         uint32_t idx2 = i + j + len / 2;
//         math_word_t u = data[idx1];
//         math_word_t v = math_hal_mod_mult(data[idx2], w, modulus);

//         data[idx1] = math_hal_mod_add(u, v, modulus);
//         data[idx2] = math_hal_mod_sub(u, v, modulus);

//         w = math_hal_mod_mult(w, w_len, modulus);
//       }
//     }
//   }

//   // 3. Scale by n^-1
//   for (uint32_t i = 0; i < n; i++)
//   {
//     data[i] = math_hal_mod_mult(data[i], params->inv_n, modulus);
//   }

//   LOG_DBG("Performed inverse NTT");
//   return 0;
// }

// int math_hal_ntt_mult(math_word_t *result, const math_word_t *a,
//                       const math_word_t *b, const ntt_params_t *params)
// {
//   if (!result || !a || !b || !params)
//     return -1;

//   // Element-wise multiplication in NTT domain
//   for (uint32_t i = 0; i < params->n; i++)
//   {
//     result[i] = math_hal_mod_mult(a[i], b[i], params->modulus);
//   }

//   LOG_DBG("Performed NTT domain multiplication");
//   return 0;
// }

// ============================================================================
// Random Number Generation
// ============================================================================

// static bool rng_initialized = false;

// At the moment this function does nothing (due to some past problems) but I might change it in the future
// int math_hal_rng_init(void)
// {
//   LOG_INF("Using nRF52840 hardware RNG");
//   rng_initialized = true;

//   return 0;
// }

// int math_hal_rng_bytes(uint8_t *buffer, size_t size)
// {
//   if (!buffer || size == 0)
//     return -1;

//   if (!rng_initialized)
//   {
//     if (math_hal_rng_init() != 0)
//       return -1;
//   }

//   sys_rand_get(buffer, size);
//   return 0;
// }

// math_word_t math_hal_rng_uniform(math_word_t max)
// {
//   if (max == 0)
//     return 0;

//   math_word_t result;
//   int ret = math_hal_rng_bytes((uint8_t *)&result, sizeof(result));
//   if (ret != 0)
//     return 0;

//   return result % max;
// }

// math_word_t math_hal_rng_mod(math_word_t m)
// {
//   return math_hal_rng_uniform(m);
// }

// ============================================================================
// Memory and Performance Utilities
// ============================================================================

// void math_hal_secure_zero(void *ptr, size_t size)
// {
//   if (!ptr || size == 0)
//     return;

//   volatile uint8_t *p = (volatile uint8_t *)ptr;
//   for (size_t i = 0; i < size; i++)
//   {
//     p[i] = 0;
//   }
// }

// bool math_hal_is_prime(math_word_t n)
// {
//   if (n < 2)
//     return false;
//   if (n == 2)
//     return true;
//   if (n % 2 == 0)
//     return false;

//   // Simple trial division up to sqrt(n)
//   for (math_word_t i = 3; i * i <= n; i += 2)
//   {
//     if (n % i == 0)
//       return false;
//   }

//   return true;
// }

// math_word_t math_hal_next_prime(math_word_t n)
// {
//   if (n < 2)
//     return 2;

//   // Make odd if even
//   if (n % 2 == 0)
//     n++;

//   while (!math_hal_is_prime(n))
//   {
//     n += 2;
//   }

//   return n;
// }

// math_word_t math_hal_gcd(math_word_t a, math_word_t b)
// {
//   while (b != 0)
//   {
//     math_word_t temp = b;
//     b = a % b;
//     a = temp;
//   }
//   return a;
// }

// uint64_t math_hal_get_cycles(void)
// {
//   return k_uptime_get();
// }