#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "internal/ckks_params.h"

static bool is_power_of_two(uint32_t n)
{
  if (n == 0)
  {
    return false;
  }
  return (n & (n - 1)) == 0;
}

static uint32_t mod_inverse(uint32_t a, uint32_t mod)
{
  if (mod == 0 || a % mod == 0)
    return 0;

  int64_t t = 0, newt = 1;
  int64_t r = mod, newr = a % mod;

  while (newr != 0)
  {
    int64_t quotient = r / newr;
    int64_t temp;

    temp = t;
    t = newt;
    newt = temp - quotient * newt;

    temp = r;
    r = newr;
    newr = temp - quotient * newr;
  }

  if (r > 1) // gcd != 1, inverse does not exist
    return 0;

  if (t < 0)
    t += mod;

  return (uint32_t)t;
}

// Selects 3 hardcoded 30-bit NTT-friendly primes q ≡ 1 (mod 4096).
// These primes are commonly used for NTT and satisfy q ≡ 1 (mod 4096):
//   998244353   = 119 * 2^23 + 1
//   1004535809  = 479 * 2^21 + 1
//   469762049   = 7   * 2^26 + 1
// The congruence ensures compatibility with NTT of size up to 4096.
static void select_ntt_primes_2048(uint32_t *out_q, uint32_t *out_num_q)
{
  if (!out_q || !out_num_q)
    return;
  out_q[0] = 998244353;  // 119 * 2^23 + 1, q ≡ 1 mod 4096
  out_q[1] = 1004535809; // 479 * 2^21 + 1, q ≡ 1 mod 4096
  out_q[2] = 469762049;  // 7   * 2^26 + 1, q ≡ 1 mod 4096
  *out_num_q = 3;
}

// Deterministic Miller-Rabin for 32-bit unsigned integers with bases {2,3,5,7,11}
static bool is_prime32(uint32_t n)
{
  if (n < 2)
    return false;
  if (n == 2 || n == 3 || n == 5 || n == 7 || n == 11)
    return true;
  if ((n & 1) == 0)
    return false;
  // Check divisibility by small primes
  if (n % 3 == 0 || n % 5 == 0 || n % 7 == 0 || n % 11 == 0)
    return false;
  // Write n-1 = d * 2^s
  uint32_t d = n - 1;
  uint32_t s = 0;
  while ((d & 1) == 0)
  {
    d >>= 1;
    s++;
  }
  uint32_t bases[] = {2, 3, 5, 7, 11};
  for (size_t i = 0; i < sizeof(bases) / sizeof(bases[0]); ++i)
  {
    uint32_t a = bases[i];
    if (a >= n)
      continue;
    // Compute a^d mod n with 64-bit intermediates
    uint64_t x = 1, base = a, exp = d;
    while (exp)
    {
      if (exp & 1)
        x = (x * base) % n;
      base = (base * base) % n;
      exp >>= 1;
    }
    if (x == 1 || x == n - 1)
      continue;
    bool witness = true;
    for (uint32_t r = 1; r < s; ++r)
    {
      x = (x * x) % n;
      if (x == n - 1)
      {
        witness = false;
        break;
      }
    }
    if (witness)
      return false;
  }
  return true;
}

// Returns true iff:
//  - N is a nonzero power of two,
//  - qi < (1u << OE_Q_MAX_BITS),
//  - qi ≡ 1 mod 2N,
//  - and is_prime32(qi) is true.
static bool qi_is_ntt_friendly(uint32_t qi, uint32_t N)
{
  if (!is_power_of_two(N) || N == 0)
    return false;
  if (qi >= (1u << OE_Q_MAX_BITS))
    return false;
  uint32_t mod = 2 * N;
  if ((qi % mod) != 1)
    return false;
  if (!is_prime32(qi))
    return false;
  return true;
}

// Returns OE_OK on success, or OE_ERR_INVALID_ARG on any invalid input.
oe_status_t ckks_params_get_preset_2048(oe_ckks_params_t *io_params)
{
  if (!io_params)
    return OE_ERR_INVALID_ARG;
  if (io_params->ring_dim != 2048)
    return OE_ERR_INVALID_ARG;
  if (!is_power_of_two(io_params->ring_dim))
    return OE_ERR_INVALID_ARG;

  // Fill q and num_q if needed.
  if (io_params->num_q == 0)
  {
    select_ntt_primes_2048(io_params->q, &io_params->num_q);
  }
  if (io_params->num_q == 0 || io_params->num_q > OE_MAX_LIMBS)
    return OE_ERR_INVALID_ARG;

  for (uint32_t i = 0; i < io_params->num_q; ++i)
  {
    uint32_t qi = io_params->q[i];
    if (qi >= (1u << OE_Q_MAX_BITS))
      return OE_ERR_INVALID_ARG;
    if (!qi_is_ntt_friendly(qi, 2048))
      return OE_ERR_INVALID_ARG;
  }

  if (io_params->scale <= 0.0)
    io_params->scale = 33554432.0; // 2^25

  // Do not touch ntt_mode, ifft_mode, mempool_hint_bytes.
  return OE_OK;
}

// Returns OE_OK on success, or OE_ERR_INVALID_ARG on any invalid input.
oe_status_t ckks_params_validate(const oe_ckks_params_t *params)
{
  if (params == NULL)
    return OE_ERR_INVALID_ARG;

  // Check ring_dim is nonzero and power of two
  if (params->ring_dim == 0 || !is_power_of_two(params->ring_dim))
    return OE_ERR_INVALID_ARG;

  // Check num_q in (0, OE_MAX_LIMBS]
  if (params->num_q == 0 || params->num_q > OE_MAX_LIMBS)
    return OE_ERR_INVALID_ARG;

  // Check each qi
  for (uint32_t i = 0; i < params->num_q; ++i)
  {
    uint32_t qi = params->q[i];
    if (qi == 0)
      return OE_ERR_INVALID_ARG;
    if (qi >= (1u << OE_Q_MAX_BITS))
      return OE_ERR_INVALID_ARG;
    if (!qi_is_ntt_friendly(qi, params->ring_dim))
      return OE_ERR_INVALID_ARG;
  }

  // Check scale is positive and not NaN
  if (!(params->scale > 0.0) || params->scale != params->scale)
    return OE_ERR_INVALID_ARG;

  // Check ntt_mode is valid
  if (!(params->ntt_mode == OE_NTT_ON_THE_FLY || params->ntt_mode == OE_NTT_PRECOMPUTED))
    return OE_ERR_INVALID_ARG;

  // Check ifft_mode is valid
  if (!(params->ifft_mode == OE_IFFT_ON_THE_FLY || params->ifft_mode == OE_IFFT_PRECOMPUTED))
    return OE_ERR_INVALID_ARG;

  return OE_OK;
}

// Externally visible function: returns true iff qi is NTT-friendly for the given ring dimension.
bool ckks_qi_is_ntt_friendly(uint32_t qi, uint32_t ring_dim)
{
  return qi_is_ntt_friendly(qi, ring_dim);
}

// Computes derived constants for CKKS parameters.
oe_status_t ckks_params_derive(const oe_ckks_params_t *params, oe_derived_consts_t *out)
{
  if (!params || !out)
    return OE_ERR_INVALID_ARG;
  uint32_t ring_dim = params->ring_dim;
  if (ring_dim == 0 || !is_power_of_two(ring_dim))
    return OE_ERR_INVALID_ARG;
  uint32_t num_q = params->num_q;
  if (num_q == 0 || num_q > OE_MAX_LIMBS)
    return OE_ERR_INVALID_ARG;

  for (uint32_t i = 0; i < num_q; ++i)
  {
    uint32_t qi = params->q[i];
    if (!qi_is_ntt_friendly(qi, ring_dim))
      return OE_ERR_INVALID_ARG;
    out->primes[i].q = qi;
    // Compute barrett_mu = floor(2^64 / qi) using only 64-bit ops
    uint64_t mu = UINT64_MAX / qi;
    uint64_t r = UINT64_MAX % qi;
    if (r == (uint64_t)qi - 1)
      mu += 1;
    out->primes[i].barrett_mu = mu;
    // Compute n_inv_mod_q = mod_inverse(ring_dim % qi, qi)
    uint32_t ring_dim_mod_q = ring_dim % qi;
    uint32_t n_inv_mod_q = mod_inverse(ring_dim_mod_q, qi);
    if (n_inv_mod_q == 0)
      return OE_ERR_INVALID_ARG;
    out->primes[i].n_inv_mod_q = n_inv_mod_q;
    out->primes[i].reserved0 = 0;
    out->primes[i].reserved1 = 0;
  }
  out->ring_dim = ring_dim;
  out->twoN = 2 * ring_dim;
  out->num_q = num_q;
  return OE_OK;
}

// Returns a conservative estimate of the number of bytes needed for NTT precomputation.
size_t ckks_params_estimate_ntt_plan_bytes(uint32_t ring_dim, uint32_t num_q, const uint32_t *q, oe_ntt_mode_t mode)
{
  // Return 0 for invalid inputs or on-the-fly mode.
  if (mode == OE_NTT_ON_THE_FLY || ring_dim == 0 || num_q == 0 || q == NULL)
    return 0;
  // Check ring_dim is a power of two.
  if (!is_power_of_two(ring_dim))
    return 0;
  // For OE_NTT_PRECOMPUTED, estimate required bytes.
  // Each prime needs storage for forward and inverse twiddles: 2 * ring_dim * sizeof(uint32_t)
  // Plus a small metadata overhead per prime (32 bytes).
  // Total = num_q * (2 * ring_dim * 4 + 32)
  // Guard against overflow: ring_dim > SIZE_MAX / (8 * num_q) => return 0
  if (num_q > 0 && ring_dim > SIZE_MAX / (8 * num_q))
    return 0;
  size_t per_prime = 2 * (size_t)ring_dim * 4 + 32;
  size_t total = (size_t)num_q * per_prime;
  return total;
}

// Returns a conservative estimate of the number of bytes needed for IFFT precomputation.
size_t ckks_params_estimate_ifft_plan_bytes(uint32_t ring_dim, oe_ifft_mode_t mode)
{
  // Return 0 for on-the-fly mode, ring_dim == 0, or non-power-of-two ring_dim.
  if (mode == OE_IFFT_ON_THE_FLY || ring_dim == 0 || !is_power_of_two(ring_dim))
    return 0;
  // Only OE_IFFT_PRECOMPUTED is supported for estimation.
  // For twiddles: ring_dim * 16 bytes (complex double: 2 doubles per entry).
  // For permutation map: ring_dim * 2 bytes (uint16_t per entry).
  // Metadata overhead: 32 bytes.
  // Total: ring_dim * 18 + 32.
  // Guard against overflow: ring_dim > (SIZE_MAX - 32) / 18 => overflow.
  if (ring_dim > (SIZE_MAX - 32) / 18)
    return 0;
  size_t total = (size_t)ring_dim * 18 + 32;
  return total;
}
